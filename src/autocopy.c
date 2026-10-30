// autocopy: highlighted text lands in the clipboard, and pasting over a
// highlight still pastes what you copied before it.
//
// The daemon watches the primary selection (text you highlight) through the
// ext-data-control protocol and owns the clipboard itself, so it sees every
// paste request and decides at that moment what to hand over:
//
//   - A highlight starts a "gesture". The clipboard from before the gesture is
//     kept (snapshotted into memory, every mime type) and served as before.
//   - Pasting while the highlight is still live in the window it was made in
//     is a paste-over: the app gets the pre-gesture clipboard, and the
//     highlight it replaces is dropped. Only windows seen releasing the
//     primary selection get this: in the others (Firefox) a highlight stays
//     "live" after the user clicks into another field, so a paste there
//     pastes the highlight.
//   - Anything else that ends the gesture commits the highlight to the
//     clipboard: clicking elsewhere (the app clears primary), pasting it into
//     another window, or starting a different selection.
//
// Window identity comes from Hyprland's IPC sockets. Without them every paste
// commits the highlight, which is plain "select to copy" behaviour.

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>

#include "ext-data-control-v1-client-protocol.h"

#define MAX_BLOB (64u << 20)      // per mime type
#define RECEIVE_TIMEOUT_MS 1000   // a stalled clipboard owner must not hang us
#define OWN_SETTLE_MS 100         // reads this soon after we set the clipboard are watchers, not pastes
#define RELEASE_GRACE_MS 100      // Chromium clears and re-sets primary on every drag step
#define MAX_RELEASERS 64

static const char *const text_mimes[] = {
	"text/plain;charset=utf-8", "text/plain", "UTF8_STRING", "STRING", "TEXT",
};
#define N_TEXT_MIMES (sizeof text_mimes / sizeof *text_mimes)

struct blob {
	char *mime;
	char *data;
	size_t len;
};

struct content {
	struct blob *b;
	size_t n;
};

struct offer_info {
	char **mimes;
	size_t n;
};

static struct wl_display *display;
static struct wl_seat *seat;
static struct ext_data_control_manager_v1 *manager;
static struct ext_data_control_device_v1 *device;

// Clipboard we own (clip_source != NULL) or nothing. cur_offer is whatever the
// compositor last announced as the clipboard, ours or someone else's.
static struct content clip;
static struct ext_data_control_source_v1 *clip_source;
static struct ext_data_control_offer_v1 *cur_offer;

// The live gesture: the text currently highlighted and where.
static char *sel;
static size_t sel_len;
static int sel_live;
static int sel_pasted_over; // pasted over: ending the gesture drops it
static char sel_window[64];
static char focused[64];

// Windows seen releasing the primary selection (a clear with nothing after
// it). Firefox never does: it keeps primary after the user clicks into
// another field, so there a live highlight says nothing about what a paste
// is aimed at.
static char releasers[MAX_RELEASERS][64];
static size_t n_releasers;

static long last_own_set = -1000000;
static long release_at = -1; // when a pending primary clear becomes a release, -1 for none

static long now_ms(void);

static void logf_(const char *fmt, ...)
{
	va_list ap;
	if (getenv("AUTOCOPY_DEBUG"))
		fprintf(stderr, "%ld ", now_ms() % 100000);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static long now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int is_text(const char *mime)
{
	for (size_t i = 0; i < N_TEXT_MIMES; i++)
		if (!strcmp(mime, text_mimes[i]))
			return 1;
	return !strncmp(mime, "text/plain", 10);
}

static void free_content(struct content *c)
{
	for (size_t i = 0; i < c->n; i++) {
		free(c->b[i].mime);
		free(c->b[i].data);
	}
	free(c->b);
	c->b = NULL;
	c->n = 0;
}

static void add_blob(struct content *c, const char *mime, const char *data, size_t len)
{
	c->b = realloc(c->b, (c->n + 1) * sizeof *c->b);
	c->b[c->n].mime = strdup(mime);
	c->b[c->n].data = memcpy(malloc(len ? len : 1), data, len);
	c->b[c->n].len = len;
	c->n++;
}

static struct blob *find_blob(struct content *c, const char *mime)
{
	for (size_t i = 0; i < c->n; i++)
		if (!strcmp(c->b[i].mime, mime))
			return &c->b[i];
	if (is_text(mime))
		for (size_t i = 0; i < c->n; i++)
			if (is_text(c->b[i].mime))
				return &c->b[i];
	return NULL;
}

// Hand data to a paste request. The reader may be slow or huge, so a child
// does the writing while the event loop carries on.
static void write_blob(int fd, const char *data, size_t len)
{
	pid_t pid = fork();
	if (pid == 0) {
		while (len) {
			ssize_t k = write(fd, data, len);
			if (k <= 0)
				break;
			data += k;
			len -= k;
		}
		_exit(0);
	}
	close(fd);
}

static int same_window(void)
{
	return focused[0] && sel_window[0] && !strcmp(focused, sel_window);
}

static int releases_primary(const char *window)
{
	for (size_t i = 0; i < n_releasers && i < MAX_RELEASERS; i++)
		if (!strcmp(releasers[i], window))
			return 1;
	return 0;
}

static void add_releaser(const char *window)
{
	if (!window[0] || releases_primary(window))
		return;
	// Oldest first out: windows come and go over a session.
	snprintf(releasers[n_releasers++ % MAX_RELEASERS], sizeof *releasers, "%s", window);
	logf_("%s releases the primary selection: paste-over on", window);
}

// ---- offers --------------------------------------------------------------

static void offer_mime(void *data, struct ext_data_control_offer_v1 *offer, const char *mime)
{
	(void)offer;
	struct offer_info *oi = data;
	oi->mimes = realloc(oi->mimes, (oi->n + 1) * sizeof *oi->mimes);
	oi->mimes[oi->n++] = strdup(mime);
}

static const struct ext_data_control_offer_v1_listener offer_listener = { .offer = offer_mime };

static int offer_has(struct offer_info *oi, const char *mime)
{
	for (size_t i = 0; i < oi->n; i++)
		if (!strcmp(oi->mimes[i], mime))
			return 1;
	return 0;
}

static void destroy_offer(struct ext_data_control_offer_v1 *offer)
{
	struct offer_info *oi = ext_data_control_offer_v1_get_user_data(offer);
	for (size_t i = 0; i < oi->n; i++)
		free(oi->mimes[i]);
	free(oi->mimes);
	free(oi);
	ext_data_control_offer_v1_destroy(offer);
}

// Read one mime type of an offer we do not own. Blocks the loop for at most
// RECEIVE_TIMEOUT_MS; nothing we own is ever read this way, so no deadlock.
static char *receive(struct ext_data_control_offer_v1 *offer, const char *mime, size_t *len)
{
	int p[2];
	*len = 0;
	if (pipe2(p, O_CLOEXEC) < 0)
		return NULL;
	ext_data_control_offer_v1_receive(offer, mime, p[1]);
	close(p[1]);
	wl_display_flush(display);

	char *buf = NULL;
	size_t cap = 0, n = 0;
	long deadline = now_ms() + RECEIVE_TIMEOUT_MS;
	for (;;) {
		long left = deadline - now_ms();
		struct pollfd pf = { p[0], POLLIN, 0 };
		if (left <= 0 || poll(&pf, 1, (int)left) <= 0)
			break;
		if (n + 65536 > cap) {
			cap = cap ? cap * 2 : 65536;
			buf = realloc(buf, cap);
		}
		ssize_t k = read(p[0], buf + n, cap - n);
		if (k <= 0)
			break;
		n += (size_t)k;
		if (n > MAX_BLOB)
			break;
	}
	close(p[0]);
	*len = n;
	if (getenv("AUTOCOPY_DEBUG"))
		logf_("receive %s: %zu bytes", mime, n);
	return buf;
}

// ---- our clipboard source ------------------------------------------------

static void commit(void);

static void source_send(void *data, struct ext_data_control_source_v1 *source, const char *mime, int fd)
{
	(void)data;
	(void)source;
	long t = now_ms();

	if (sel && is_text(mime) && t - last_own_set > OWN_SETTLE_MS) {
		if (sel_live && same_window() && releases_primary(sel_window)) {
			// Paste over the highlight: hand over what was copied before it.
			sel_pasted_over = 1;
			logf_("paste-over in %s: serving pre-selection clipboard", focused);
		} else {
			// Pasting the highlight somewhere else: it is the clipboard now.
			logf_("paste of selection in %s: committing", focused);
			write_blob(fd, sel, sel_len);
			commit();
			return;
		}
	}

	struct blob *b = find_blob(&clip, mime);
	if (b)
		write_blob(fd, b->data, b->len);
	else
		close(fd);
}

static void source_cancelled(void *data, struct ext_data_control_source_v1 *source)
{
	(void)data;
	if (source == clip_source) {
		// Someone else owns the clipboard now; forget our copy of the old one.
		clip_source = NULL;
		free_content(&clip);
	}
	ext_data_control_source_v1_destroy(source);
}

static const struct ext_data_control_source_v1_listener source_listener = {
	.send = source_send,
	.cancelled = source_cancelled,
};

// Take ownership of the clipboard with content c (moved into clip).
static void own(struct content *c)
{
	struct ext_data_control_source_v1 *src = ext_data_control_manager_v1_create_data_source(manager);
	ext_data_control_source_v1_add_listener(src, &source_listener, NULL);
	for (size_t i = 0; i < c->n; i++)
		ext_data_control_source_v1_offer(src, c->b[i].mime);
	free_content(&clip);
	clip = *c;
	c->b = NULL;
	c->n = 0;
	clip_source = src;
	last_own_set = now_ms();
	ext_data_control_device_v1_set_selection(device, src);
	wl_display_flush(display);
}

// The highlight becomes the clipboard, offered as every common text type.
static void commit(void)
{
	release_at = -1;
	if (!sel)
		return;
	struct content c = { 0 };
	for (size_t i = 0; i < N_TEXT_MIMES; i++)
		add_blob(&c, text_mimes[i], sel, sel_len);
	own(&c);
	free(sel);
	sel = NULL;
	sel_len = 0;
	sel_live = 0;
	sel_pasted_over = 0;
}

static void discard(void)
{
	release_at = -1;
	free(sel);
	sel = NULL;
	sel_len = 0;
	sel_live = 0;
	sel_pasted_over = 0;
}

// The gesture is over. A highlight that was pasted over was not meant to be
// copied; any other was.
static void end_gesture(void)
{
	if (sel_pasted_over)
		discard();
	else
		commit();
}

// Copy someone else's clipboard into memory and become its owner, so that the
// paste requests that follow come to us. Nothing to do if it is empty.
static void snapshot(void)
{
	if (!cur_offer)
		return;
	struct offer_info *oi = ext_data_control_offer_v1_get_user_data(cur_offer);
	// A password manager's clipboard stays with the manager, so its own
	// clearing keeps working. Pasting over a highlight still reads from it.
	if (offer_has(oi, "x-kde-passwordManagerHint"))
		return;
	struct content c = { 0 };
	for (size_t i = 0; i < oi->n; i++) {
		size_t len;
		int dup = 0; // wl-copy and others list some types twice
		for (size_t j = 0; j < c.n; j++)
			dup |= !strcmp(c.b[j].mime, oi->mimes[i]);
		if (dup)
			continue;
		char *d = receive(cur_offer, oi->mimes[i], &len);
		if (d && len)
			add_blob(&c, oi->mimes[i], d, len);
		free(d);
	}
	if (c.n) {
		logf_("snapshot: keeping clipboard (%zu types) across the selection", c.n);
		own(&c);
	}
}

// ---- gesture logic -------------------------------------------------------

static void on_primary_data(char *text, size_t len)
{
	if (sel) {
		// Growing or shrinking a drag keeps the gesture. A different text, or
		// one from another window, is a new selection: the old one was made
		// to be copied, unless it was just pasted over.
		int extends = same_window() &&
			(memmem(text, len, sel, sel_len) || memmem(sel, sel_len, text, len));
		if (!extends)
			end_gesture();
	}
	if (!sel) {
		if (!clip_source && focused[0])
			snapshot();
		snprintf(sel_window, sizeof sel_window, "%s", focused);
	}
	free(sel);
	sel = text;
	sel_len = len;
	sel_live = 1;
	release_at = -1; // live again: a pending clear was a drag step
	// A paste-over is about the text it pasted over, not this one.
	sel_pasted_over = 0;
	// No window to compare against: plain select-to-copy.
	if (!focused[0])
		commit();
}

// Primary cleared. Some toolkits clear and immediately re-set it on every
// step of a drag, so wait a moment: a re-set that extends the text continues
// the gesture, and only silence is a real release.
static void on_primary_clear(void)
{
	sel_live = 0;
	if (sel)
		release_at = now_ms() + RELEASE_GRACE_MS;
}

static void finish_release(void)
{
	release_at = -1;
	if (!sel)
		return;
	// The window that made the highlight let go of it with nothing after:
	// its highlights are live only while they are really highlighted.
	if (same_window())
		add_releaser(sel_window);
	logf_(sel_pasted_over ? "selection replaced by paste: dropped" : "selection released: copied");
	end_gesture();
}

// ---- device --------------------------------------------------------------

static void device_data_offer(void *data, struct ext_data_control_device_v1 *dev,
	struct ext_data_control_offer_v1 *offer)
{
	(void)data;
	(void)dev;
	ext_data_control_offer_v1_add_listener(offer, &offer_listener, calloc(1, sizeof(struct offer_info)));
}

static void device_selection(void *data, struct ext_data_control_device_v1 *dev,
	struct ext_data_control_offer_v1 *offer)
{
	(void)data;
	(void)dev;
	if (cur_offer)
		destroy_offer(cur_offer);
	cur_offer = offer;
	// Our own source announces itself here too; cancelled arrives first when
	// it is not ours any more.
	if (clip_source)
		return;
	// The user copied explicitly. Their clipboard wins over the highlight.
	if (sel) {
		logf_("clipboard set by an app: selection dropped");
		discard();
	}
}

static void device_primary_selection(void *data, struct ext_data_control_device_v1 *dev,
	struct ext_data_control_offer_v1 *offer)
{
	(void)data;
	(void)dev;
	if (!offer) {
		on_primary_clear();
		return;
	}
	struct offer_info *oi = ext_data_control_offer_v1_get_user_data(offer);
	const char *mime = NULL;
	for (size_t i = 0; i < N_TEXT_MIMES && !mime; i++)
		if (offer_has(oi, text_mimes[i]))
			mime = text_mimes[i];
	// Password fields mark the selection; leave them alone.
	if (mime && !offer_has(oi, "x-kde-passwordManagerHint")) {
		size_t len;
		char *text = receive(offer, mime, &len);
		if (text && len)
			on_primary_data(text, len);
		else
			free(text);
	}
	destroy_offer(offer);
}

static void device_finished(void *data, struct ext_data_control_device_v1 *dev)
{
	(void)data;
	(void)dev;
	logf_("compositor closed the data control device");
	exit(1);
}

static const struct ext_data_control_device_v1_listener device_listener = {
	.data_offer = device_data_offer,
	.selection = device_selection,
	.finished = device_finished,
	.primary_selection = device_primary_selection,
};

// ---- registry ------------------------------------------------------------

static void registry_global(void *data, struct wl_registry *reg, uint32_t name,
	const char *iface, uint32_t version)
{
	(void)data;
	(void)version;
	if (!strcmp(iface, wl_seat_interface.name) && !seat)
		seat = wl_registry_bind(reg, name, &wl_seat_interface, 1);
	else if (!strcmp(iface, ext_data_control_manager_v1_interface.name))
		manager = wl_registry_bind(reg, name, &ext_data_control_manager_v1_interface, 1);
}

static void registry_global_remove(void *data, struct wl_registry *reg, uint32_t name)
{
	(void)data;
	(void)reg;
	(void)name;
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

// ---- Hyprland focus ------------------------------------------------------

static void set_focused(const char *addr)
{
	if (!strncmp(addr, "0x", 2))
		addr += 2;
	snprintf(focused, sizeof focused, "%s", addr);
	for (char *p = focused; *p; p++)
		if (*p == ',' || *p == '\n' || *p == '"') {
			*p = 0;
			break;
		}
}

static int hypr_socket(const char *name)
{
	const char *sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
	const char *rt = getenv("XDG_RUNTIME_DIR");
	if (!sig || !rt)
		return -1;
	struct sockaddr_un sa = { .sun_family = AF_UNIX };
	if ((size_t)snprintf(sa.sun_path, sizeof sa.sun_path, "%s/hypr/%s/%s", rt, sig, name) >= sizeof sa.sun_path)
		return -1;
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;
	if (connect(fd, (struct sockaddr *)&sa, sizeof sa) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

// Seed the focused window, then return the event socket (or -1).
static int hypr_connect(void)
{
	int fd = hypr_socket(".socket.sock");
	if (fd < 0)
		return -1;
	static const char req[] = "j/activewindow";
	if (write(fd, req, sizeof req - 1) == (ssize_t)(sizeof req - 1)) {
		char buf[8192];
		size_t n = 0;
		ssize_t k;
		while (n < sizeof buf - 1 && (k = read(fd, buf + n, sizeof buf - 1 - n)) > 0)
			n += (size_t)k;
		buf[n] = 0;
		if (getenv("AUTOCOPY_DEBUG"))
			logf_("activewindow reply: %zu bytes", n);
		char *p = strstr(buf, "\"address\": \"");
		if (p)
			set_focused(p + strlen("\"address\": \""));
	}
	close(fd);
	return hypr_socket(".socket2.sock");
}

static int hypr_read(int fd, char *buf, size_t *n, size_t cap)
{
	ssize_t k = read(fd, buf + *n, cap - *n - 1);
	if (k <= 0) {
		logf_("Hyprland event socket closed: window checks off");
		focused[0] = 0;
		close(fd);
		return -1;
	}
	*n += (size_t)k;
	buf[*n] = 0;
	char *line = buf, *nl;
	while ((nl = strchr(line, '\n'))) {
		*nl = 0;
		if (!strncmp(line, "activewindowv2>>", 16)) {
			set_focused(line + 16);
			// Leaving the highlight's window: it was made to be pasted elsewhere.
			// Committing now offers only text, so an app that prefers an image or
			// HTML type does not get the old clipboard instead.
			if (sel && focused[0] && strcmp(focused, sel_window)) {
				logf_(sel_pasted_over ? "focus moved to %s: pasted-over selection dropped"
					: "focus moved to %s: selection copied", focused);
				end_gesture();
			}
		}
		line = nl + 1;
	}
	*n = strlen(line);
	memmove(buf, line, *n + 1);
	return 0;
}

// ---- main ----------------------------------------------------------------

int main(void)
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	setvbuf(stderr, NULL, _IOLBF, 0);

	display = wl_display_connect(NULL);
	if (!display) {
		logf_("cannot connect to the Wayland display");
		return 1;
	}
	struct wl_registry *reg = wl_display_get_registry(display);
	wl_registry_add_listener(reg, &registry_listener, NULL);
	wl_display_roundtrip(display);
	if (!seat || !manager) {
		logf_("compositor lacks ext-data-control-v1 (need Hyprland 0.47+, Sway with wlroots 0.19+)");
		return 1;
	}
	device = ext_data_control_manager_v1_get_data_device(manager, seat);
	ext_data_control_device_v1_add_listener(device, &device_listener, NULL);

	int hypr = hypr_connect();
	if (hypr < 0)
		logf_("no Hyprland IPC: every paste commits the highlight");
	else
		logf_("watching selections; focused window %s", focused[0] ? focused : "(none)");

	char hbuf[4096];
	size_t hn = 0;
	int wfd = wl_display_get_fd(display);
	for (;;) {
		while (wl_display_prepare_read(display) != 0)
			wl_display_dispatch_pending(display);
		wl_display_flush(display);

		int timeout = -1;
		if (release_at >= 0) {
			long left = release_at - now_ms();
			timeout = left > 0 ? (int)left : 0;
		}
		struct pollfd pf[2] = { { wfd, POLLIN, 0 }, { hypr, POLLIN, 0 } };
		if (poll(pf, hypr >= 0 ? 2 : 1, timeout) < 0 && errno != EINTR) {
			wl_display_cancel_read(display);
			break;
		}
		if (pf[0].revents & POLLIN)
			wl_display_read_events(display);
		else
			wl_display_cancel_read(display);
		if (wl_display_dispatch_pending(display) < 0)
			break;
		if (hypr >= 0 && (pf[1].revents & (POLLIN | POLLHUP)) &&
			hypr_read(hypr, hbuf, &hn, sizeof hbuf) < 0)
			hypr = -1;
		if (release_at >= 0 && now_ms() >= release_at)
			finish_release();
	}
	logf_("Wayland connection lost");
	return 1;
}
