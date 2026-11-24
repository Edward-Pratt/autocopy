#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "clipboard.h"
#include "log.h"
#include "offer.h"

#define OWN_SETTLE_MS 100         // reads this soon after we set the clipboard are watchers, not pastes

static struct wl_display *display;
static struct ext_data_control_manager_v1 *manager;
static struct ext_data_control_device_v1 *device;
static void (*paste_handler)(const char *mime, int fd);

// Clipboard we own (clip_source != NULL) or nothing. cur_offer is whatever the
// compositor last announced as the clipboard, ours or someone else's.
static struct content clip;
static struct ext_data_control_source_v1 *clip_source;
static struct ext_data_control_offer_v1 *cur_offer;

static long last_own_set = -1000000;

static void source_send(void *data, struct ext_data_control_source_v1 *source, const char *mime, int fd)
{
	(void)data;
	(void)source;
	paste_handler(mime, fd);
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

void clipboard_start(struct wl_display *dpy, struct ext_data_control_manager_v1 *mgr,
	struct ext_data_control_device_v1 *dev, void (*on_paste)(const char *mime, int fd))
{
	display = dpy;
	manager = mgr;
	device = dev;
	paste_handler = on_paste;
}

// Take ownership of the clipboard with content c (moved into clip).
void clipboard_own(struct content *c)
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

// Copy someone else's clipboard into memory and become its owner, so that the
// paste requests that follow come to us. Nothing to do if it is empty.
void clipboard_snapshot(void)
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
		char *d = receive(display, cur_offer, oi->mimes[i], &len);
		if (d && len)
			add_blob(&c, oi->mimes[i], d, len);
		free(d);
	}
	if (c.n) {
		logf_("snapshot: keeping clipboard (%zu types) across the selection", c.n);
		clipboard_own(&c);
	}
}

void clipboard_send(const char *mime, int fd)
{
	struct blob *b = find_blob(&clip, mime);
	if (b)
		write_blob(fd, b->data, b->len);
	else
		close(fd);
}

int clipboard_owned(void)
{
	return clip_source != NULL;
}

int clipboard_settled(void)
{
	return now_ms() - last_own_set > OWN_SETTLE_MS;
}

int clipboard_changed(struct ext_data_control_offer_v1 *offer)
{
	if (cur_offer)
		destroy_offer(cur_offer);
	cur_offer = offer;
	// Our own source announces itself here too; cancelled arrives first when
	// it is not ours any more.
	return clip_source != NULL;
}
