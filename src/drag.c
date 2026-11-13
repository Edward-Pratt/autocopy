// A drag sets the primary selection again on every step, starting small and
// growing; a click sets it once. In Discord a click (double-clicking a message
// to reply, say) can highlight hidden text such as timestamps or the message
// box's labels, so there a highlight that arrives in one step is held here,
// never copied, and only becomes a gesture if the next one changes it.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drag.h"

static char *held;
static size_t held_len;
static char held_window[64];

// A drag step keeps the end the drag started from: the shorter text starts or
// ends the longer one, and they differ. The same text again is a re-set.
static int drag_step(const char *a, size_t alen, const char *b, size_t blen)
{
	if (alen == blen)
		return 0;
	if (alen > blen)
		return drag_step(b, blen, a, alen);
	return !memcmp(a, b, alen) || !memcmp(a, b + blen - alen, alen);
}

int drag_continues(const char *text, size_t len, const char *prev, size_t prev_len)
{
	return (len == prev_len && !memcmp(text, prev, len)) ||
		drag_step(text, len, prev, prev_len);
}

int drag_started(char *text, size_t len, const char *window)
{
	int changed = held && !strcmp(window, held_window) &&
		drag_step(text, len, held, held_len);
	free(held);
	held = NULL;
	held_len = 0;
	if (changed)
		return 1;
	held = text;
	held_len = len;
	snprintf(held_window, sizeof held_window, "%s", window);
	return 0;
}
