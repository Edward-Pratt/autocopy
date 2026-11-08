// Paths under the XDG base directories.
#ifndef AUTOCOPY_XDG_H
#define AUTOCOPY_XDG_H

#include <stddef.h>

// $var/file, or $HOME/fallback/file when var is unset or empty. Returns 0
// (out empty) when neither is set.
int xdg_path(char *out, size_t cap, const char *var, const char *fallback, const char *file);

#endif
