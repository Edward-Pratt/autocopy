// The gesture logic: what the clipboard holds and what each paste gets.
#ifndef AUTOCOPY_AUTOCOPY_H
#define AUTOCOPY_AUTOCOPY_H

#include <stddef.h>

// The primary selection now holds text (autocopy takes ownership of it).
void autocopy_primary(char *text, size_t len);
// The primary selection was cleared.
void autocopy_primary_clear(void);
// An app set the clipboard itself, as Ctrl+C does.
void autocopy_app_copied(void);
// A paste asks the clipboard we hold for mime, to be written to fd.
void autocopy_paste(const char *mime, int fd);
// When autocopy_tick has work to do, in now_ms() time, or -1 for never.
long autocopy_deadline(void);
void autocopy_tick(void);
// The focused window changed.
void autocopy_focus_changed(void);

#endif
