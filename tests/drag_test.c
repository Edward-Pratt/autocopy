// Tests for drag_started: make check.
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include "drag.h"

static int failed;

static void step(const char *text, const char *window, int want, const char *what)
{
	// drag_started keeps text when it returns 0, so give it its own copy.
	if (drag_started(strdup(text), strlen(text), window) != want) {
		printf("FAIL %s: \"%s\" in %s should give %d\n", what, text, window, want);
		failed = 1;
	}
}

int main(void)
{
	// A click: one highlight, then something else.
	step("[12:30]Friday, 13 November 2026 at 12:30", "w1", 0, "a click is held");
	step("Choose filesNo file chosen\nMessage #general", "w1", 0, "another click is held");
	// A drag: the first step is held, the next one that grows it starts the gesture.
	step("h", "w1", 0, "a drag's first step is held");
	step("he", "w1", 1, "a growing step is a drag");
	// Dragging backwards grows the start instead.
	step("lo", "w1", 0, "a backward drag's first step is held");
	step("llo", "w1", 1, "a step that keeps the end is a drag");
	// A click on text inside the held one is not a step.
	step("one two three", "w1", 0, "a click is held");
	step("two", "w1", 0, "a word from its middle is another click");
	// Shrinking counts too.
	step("hello", "w1", 0, "a new first step is held");
	step("hell", "w1", 1, "a shrinking step is a drag");
	// The same text set again is not a step.
	step("word", "w1", 0, "a double-clicked word is held");
	step("word", "w1", 0, "the same text again stays held");
	// A growing text in another window is not this drag.
	step("abc", "w1", 0, "held in w1");
	step("abcd", "w2", 0, "another window is a new highlight");
	step("abcde", "w2", 1, "which a drag there then starts");
	// Continuing a gesture: the same text, or a step that keeps one end.
	if (!drag_continues("abc", 3, "abc", 3) || !drag_continues("abcd", 4, "abc", 3) ||
		!drag_continues("ab", 2, "abc", 3) || !drag_continues("zabc", 4, "abc", 3)) {
		printf("FAIL drag_continues misses a step\n");
		failed = 1;
	}
	if (drag_continues("b", 1, "abc", 3) || drag_continues("xyz", 3, "abc", 3)) {
		printf("FAIL drag_continues takes a click inside the highlight\n");
		failed = 1;
	}
	if (!failed)
		printf("ok\n");
	return failed;
}
