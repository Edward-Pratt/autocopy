// Desktop notifications for copies, turned on and off by autocopy-notify.
#ifndef AUTOCOPY_NOTIFY_H
#define AUTOCOPY_NOTIFY_H

#include <stddef.h>

int notify_enabled(void);
// Say on the desktop that text is now the clipboard, with a one-line preview.
void notify_copied(const char *text, size_t len);

#endif
