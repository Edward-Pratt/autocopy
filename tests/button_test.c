// Tests for copying on the mouse button release: make check.
// autocopy.c runs against stand-ins for the clipboard, notifications, the
// window list and the clock, which record what it does.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "autocopy.h"
#include "clipboard.h"
#include "content.h"
#include "releasers.h"

// ---- stand-ins -----------------------------------------------------------

char focused[64];
char focused_class[128];
const char *const text_mimes[N_TEXT_MIMES] = { "text/plain;charset=utf-8", "text/plain", "UTF8_STRING", "STRING", "TEXT" };

static long clock_ms = 1000;
static int owns;        // times the clipboard was set
static int notes;       // notifications sent
static int sends;       // pastes answered from the clipboard as it is
static int ours;        // whether the clipboard is ours
static char owned[64];  // what the last clipboard_own held

long now_ms(void) { return clock_ms; }
void logf_(const char *fmt, ...) { (void)fmt; }
int releases_primary(const char *class) { return !strcmp(class, "gtk"); }
void add_releaser(const char *class) { (void)class; }
void notify_copied(const char *text, size_t len) { (void)text; (void)len; notes++; }

int is_text(const char *mime) { return !strncmp(mime, "text/plain", 10); }
void add_blob(struct content *c, const char *mime, const char *data, size_t len)
{
	(void)mime;
	if (!c->n)
		snprintf(owned, sizeof owned, "%.*s", (int)len, data);
	c->n++;
}
void write_blob(int fd, const char *data, size_t len) { (void)fd; (void)data; (void)len; }

void clipboard_own(struct content *c) { owns++; ours = 1; c->n = 0; }
void clipboard_snapshot(void) { ours = 1; }
void clipboard_send(const char *mime, int fd) { (void)mime; (void)fd; sends++; }
int clipboard_owned(void) { return ours; }
int clipboard_settled(void) { return 1; }

// ---- helpers -------------------------------------------------------------

static int failed;

static void reset(const char *window, const char *class)
{
	snprintf(focused, sizeof focused, "%s", window);
	snprintf(focused_class, sizeof focused_class, "%s", class);
	owns = notes = sends = ours = 0;
	owned[0] = 0;
}

static void wait_ms(long ms)
{
	clock_ms += ms;
	autocopy_tick();
}

static void highlight(const char *text) { autocopy_primary(strdup(text), strlen(text)); }

static void release(void)
{
	autocopy_button_released();
	wait_ms(50);
}

static void expect(int got, int want, const char *what)
{
	if (got != want) {
		printf("FAIL %s: got %d, want %d\n", what, got, want);
		failed = 1;
	}
}

// Ends whatever gesture a test left, so the next one starts clean.
static void finish(void)
{
	snprintf(focused, sizeof focused, "elsewhere");
	autocopy_focus_changed();
}

// ---- tests ---------------------------------------------------------------

int main(void)
{
	// Paste-over app: the release leaves the old clipboard, and a paste over
	// the highlight gets it as it is, every type included.
	reset("w1", "gtk");
	highlight("hello");
	release();
	expect(owns, 0, "paste-over: release does not set the clipboard");
	expect(notes, 0, "paste-over: release does not notify");
	autocopy_paste("text/plain", 3);
	expect(sends, 1, "paste-over: the paste gets the old clipboard");
	autocopy_primary_clear();
	wait_ms(200);
	expect(owns, 0, "paste-over: a pasted-over highlight is never copied");
	expect(notes, 0, "paste-over: a pasted-over highlight never notifies");

	// Paste-over app, no paste: the copy comes when the gesture ends, once.
	reset("w1", "gtk");
	highlight("hello");
	release();
	autocopy_primary_clear();
	wait_ms(200);
	expect(owns, 1, "paste-over: clicking away copies");
	expect(notes, 1, "paste-over: clicking away notifies once");

	// App without paste-over: the drag's first step takes the clipboard, the
	// release copies the whole highlight and notifies, once.
	reset("w2", "firefox");
	highlight("he");
	expect(owns, 1, "no paste-over: the first drag step takes the clipboard");
	highlight("hello");
	expect(owns, 1, "no paste-over: later drag steps do not");
	release();
	expect(owns, 2, "no paste-over: the release copies");
	expect(strcmp(owned, "hello") == 0, 1, "no paste-over: the release copies the whole highlight");
	expect(notes, 1, "no paste-over: the release notifies");
	release();
	finish();
	expect(owns, 2, "no paste-over: nothing copies again");
	expect(notes, 1, "no paste-over: nothing notifies again");

	// Without the bind the copy waits for the gesture to end.
	reset("w2", "firefox");
	highlight("he");
	highlight("hello");
	wait_ms(1000);
	expect(notes, 0, "no bind: a pause in the drag does not notify");
	finish();
	expect(notes, 1, "no bind: the end of the gesture notifies once");

	if (!failed)
		printf("ok\n");
	return failed;
}
