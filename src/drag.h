// Highlights that count only once a drag changes them.
#ifndef AUTOCOPY_DRAG_H
#define AUTOCOPY_DRAG_H

#include <stddef.h>

// A new highlight in window. Returns 1 if it is the held one changing, which
// makes it a drag: the held one is dropped and text is the caller's. Returns
// 0 otherwise: text is held in its place, and this module frees it.
int drag_started(char *text, size_t len, const char *window);
// text is prev set again, or a drag step from it.
int drag_continues(const char *text, size_t len, const char *prev, size_t prev_len);

#endif
