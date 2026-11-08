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
//     highlight it replaces is dropped. Only apps seen releasing the
//     primary selection get this: in the others (Firefox) a highlight stays
//     "live" after the user clicks into another field, so a paste there
//     pastes the highlight. What was learned is kept across restarts.
//   - Anything else that ends the gesture commits the highlight to the
//     clipboard: clicking elsewhere (the app clears primary), pasting it into
//     another window, or starting a different selection.
//
// Window identity comes from Hyprland's IPC sockets. Without them every paste
// commits the highlight, which is plain "select to copy" behaviour.

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>

#include "autocopy.h"
#include "clipboard.h"
#include "content.h"
#include "ext-data-control-v1-client-protocol.h"
#include "hypr.h"
#include "log.h"
#include "offer.h"
#include "releasers.h"

#define RELEASE_GRACE_MS 100      // Chromium clears and re-sets primary on every drag step

static struct wl_display *display;

// The live gesture: the text currently highlighted and where.
static char *sel;
static size_t sel_len;
static int sel_live;
static int sel_pasted_over; // pasted over: ending the gesture drops it
static char sel_window[64];
static char sel_class[128];

static long release_at = -1; // when a pending primary clear becomes a release, -1 for none

static int same_window(void)
{
	return focused[0] && sel_window[0] && !strcmp(focused, sel_window);
}

// ---- pastes --------------------------------------------------------------

static void commit(void);

static void on_paste(const char *mime, int fd)
{
	if (sel && is_text(mime) && clipboard_settled()) {
		if (sel_live && same_window() && releases_primary(sel_class)) {
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
	clipboard_send(mime, fd);
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
	clipboard_own(&c);
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
		if (!clipboard_owned() && focused[0])
			clipboard_snapshot();
		snprintf(sel_window, sizeof sel_window, "%s", focused);
		snprintf(sel_class, sizeof sel_class, "%s", focused_class);
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
		add_releaser(sel_class);
	logf_(sel_pasted_over ? "selection replaced by paste: dropped" : "selection released: copied");
	end_gesture();
}

// Leaving the highlight's window: it was made to be pasted elsewhere.
// Committing now offers only text, so an app that prefers an image or
// HTML type does not get the old clipboard instead.
void autocopy_focus_changed(void)
{
	if (sel && focused[0] && strcmp(focused, sel_window)) {
		logf_(sel_pasted_over ? "focus moved to %s: pasted-over selection dropped"
			: "focus moved to %s: selection copied", focused);
		end_gesture();
	}
}

long autocopy_deadline(void)
{
	return release_at;
}

void autocopy_tick(void)
{
	if (release_at >= 0 && now_ms() >= release_at)
		finish_release();
}

// ---- device --------------------------------------------------------------

static void device_data_offer(void *data, struct ext_data_control_device_v1 *dev,
	struct ext_data_control_offer_v1 *offer)
{
	(void)data;
	(void)dev;
	track_offer(offer);
}

static void device_selection(void *data, struct ext_data_control_device_v1 *dev,
	struct ext_data_control_offer_v1 *offer)
{
	(void)data;
	(void)dev;
	if (clipboard_changed(offer))
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
		char *text = receive(display, offer, mime, &len);
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

void autocopy_start(struct wl_display *dpy, struct ext_data_control_manager_v1 *manager,
	struct wl_seat *seat)
{
	display = dpy;
	struct ext_data_control_device_v1 *device = ext_data_control_manager_v1_get_data_device(manager, seat);
	ext_data_control_device_v1_add_listener(device, &device_listener, NULL);
	clipboard_start(display, manager, device, on_paste);
}
