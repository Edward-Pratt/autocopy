#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include "xdg.h"

int xdg_path(char *out, size_t cap, const char *var, const char *fallback, const char *file)
{
	const char *dir = getenv(var), *home = getenv("HOME");
	if (dir && *dir)
		snprintf(out, cap, "%s/%s", dir, file);
	else if (home)
		snprintf(out, cap, "%s/%s/%s", home, fallback, file);
	else
		out[0] = 0;
	return out[0] != 0;
}
