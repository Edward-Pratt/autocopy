// autocopy: highlighted text lands in the clipboard, and pasting over a
// highlight still pastes what you copied before it.
//
// The daemon watches the primary selection (text you highlight) through the
// ext-data-control protocol and owns the clipboard itself, so it sees every
// paste request and decides at that moment what to hand over:
//
//   - A highlight starts a "gesture". In an app with paste-over (below), the
//     clipboard from before the gesture is kept (snapshotted into memory,
//     every mime type) and served as before.
//   - Pasting while the highlight is still live in the window it was made in
//     is a paste-over: the app gets the pre-gesture clipboard, and the
//     highlight it replaces is dropped. Only apps seen releasing the
//     primary selection get this: in the others (Firefox) a highlight stays
//     "live" after the user clicks into another field, so a paste there
//     pastes the highlight. What was learned is kept across restarts.
//   - In those other apps a paste can only mean the highlight, so it is the
//     clipboard at once, as text only: an app that prefers an HTML or image
//     type must not get the old clipboard's. It is announced once the drag
//     settles.
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

#include "autocopy.h"
#include "clipboard.h"
#include "content.h"
#include "hypr.h"
#include "log.h"
#include "notify.h"
#include "releasers.h"

#define RELEASE_GRACE_MS 100      // Chromium clears and re-sets primary on every drag step
#define COPY_SETTLE_MS 250        // a highlight that stops changing this long is done

// The live gesture: the text currently highlighted and where.
static char *sel;
static size_t sel_len;
static int sel_live;
static int sel_pasted_over; // pasted over: ending the gesture drops it
static int sel_owned;       // the clipboard holds exactly this text
static int sel_announced;   // the notification for this text is out
static char sel_window[64];
static char sel_class[128];

static long release_at = -1;  // when a pending primary clear becomes a release, -1 for none
static long announce_at = -1; // when a highlight copied at once is announced, -1 for none

static int same_window(void)
{
	return focused[0] && sel_window[0] && !strcmp(focused, sel_window);
}

// ---- the highlight as the clipboard --------------------------------------

// The highlight becomes the clipboard, offered as every common text type.
static void own_highlight(void)
{
	if (!sel || sel_owned)
		return;
	struct content c = { 0 };
	for (size_t i = 0; i < N_TEXT_MIMES; i++)
		add_blob(&c, text_mimes[i], sel, sel_len);
	clipboard_own(&c);
	sel_owned = 1;
}

// The gesture made a copy: the clipboard holds it, and that is said once per
// text. Not part of own_highlight(), which also runs on every drag step when
// there is no window to compare against.
static void announce(void)
{
	announce_at = -1;
	if (!sel || sel_announced)
		return;
	own_highlight();
	notify_copied(sel, sel_len);
	sel_announced = 1;
}

static void discard(void)
{
	release_at = -1;
	announce_at = -1;
	free(sel);
	sel = NULL;
	sel_len = 0;
	sel_live = 0;
	sel_pasted_over = 0;
	sel_owned = 0;
	sel_announced = 0;
}

// The gesture is over. A highlight that was pasted over was not meant to be
// copied; any other was.
static void end_gesture(void)
{
	if (!sel_pasted_over)
		announce();
	discard();
}

// ---- pastes --------------------------------------------------------------

void autocopy_paste(const char *mime, int fd)
{
	if (getenv("AUTOCOPY_DEBUG"))
		logf_("paste asks for %s in %s", mime, focused);
	if (sel && !sel_owned && is_text(mime) && clipboard_settled()) {
		if (sel_live && same_window() && releases_primary(sel_class)) {
			// Paste over the highlight: hand over what was copied before it.
			sel_pasted_over = 1;
			logf_("paste-over in %s: serving pre-selection clipboard", focused);
		} else {
			// Pasting the highlight somewhere else: it is the clipboard now.
			logf_("paste of selection in %s: committing", focused);
			write_blob(fd, sel, sel_len);
			end_gesture();
			return;
		}
	}
	clipboard_send(mime, fd);
}

// ---- gesture logic -------------------------------------------------------

void autocopy_primary(char *text, size_t len)
{
	int fresh = !sel;
	if (sel) {
		// Growing or shrinking a drag keeps the gesture. A different text, or
		// one from another window, is a new selection: the old one was made
		// to be copied, unless it was just pasted over.
		int extends = same_window() &&
			(memmem(text, len, sel, sel_len) || memmem(sel, sel_len, text, len));
		if (!extends) {
			end_gesture();
			fresh = 1;
		}
	}
	if (fresh) {
		if (!clipboard_owned() && focused[0] && releases_primary(focused_class))
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
	sel_owned = 0;
	sel_announced = 0;
	// No window to compare against: plain select-to-copy.
	if (!focused[0]) {
		own_highlight();
		discard();
		return;
	}
	if (!releases_primary(sel_class)) {
		// The first step of a drag takes the clipboard; the rest wait for
		// the drag to settle, so each step does not replace it again.
		if (fresh)
			own_highlight();
		announce_at = now_ms() + COPY_SETTLE_MS;
	}
}

// Primary cleared. Some toolkits clear and immediately re-set it on every
// step of a drag, so wait a moment: a re-set that extends the text continues
// the gesture, and only silence is a real release.
void autocopy_primary_clear(void)
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

// The user copied explicitly. Their clipboard wins over the highlight.
void autocopy_app_copied(void)
{
	if (sel) {
		logf_("clipboard set by an app: selection dropped");
		discard();
	}
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
	if (release_at < 0 || (announce_at >= 0 && announce_at < release_at))
		return announce_at;
	return release_at;
}

void autocopy_tick(void)
{
	long t = now_ms();
	if (release_at >= 0 && t >= release_at)
		finish_release();
	if (announce_at >= 0 && t >= announce_at)
		announce();
}
