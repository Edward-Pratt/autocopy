// Discord's message toolbar, kept out of highlights made in Discord.
#ifndef AUTOCOPY_DISCORD_H
#define AUTOCOPY_DISCORD_H

#include <stddef.h>

// The window class is Discord's, or a client built on it: Vesktop, Equibop.
int is_discord(const char *class);
// Remove the hover toolbar's labels from text highlighted in a window of
// class, in place. Returns the new length: 0 when nothing else was highlighted.
size_t strip_discord_toolbar(const char *class, char *text, size_t len);

#endif
