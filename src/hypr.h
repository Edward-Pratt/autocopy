// The focused window, followed through Hyprland's IPC sockets.
#ifndef AUTOCOPY_HYPR_H
#define AUTOCOPY_HYPR_H

extern char focused[64];        // window address, empty when unknown
extern char focused_class[128];

// Seed the focused window, then return the event socket (or -1).
int hypr_connect(void);
// Read pending events; on_focus runs after each focus change. Returns -1 once
// the socket closes, after closing it.
int hypr_read(int fd, void (*on_focus)(void));

#endif
