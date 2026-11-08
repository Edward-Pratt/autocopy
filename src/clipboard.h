// The clipboard autocopy holds: a kept copy of another app's, or the highlight.
#ifndef AUTOCOPY_CLIPBOARD_H
#define AUTOCOPY_CLIPBOARD_H

#include <wayland-client.h>

#include "content.h"
#include "ext-data-control-v1-client-protocol.h"

// Paste requests for the clipboard we hold go to on_paste.
void clipboard_start(struct wl_display *display, struct ext_data_control_manager_v1 *manager,
	struct ext_data_control_device_v1 *device, void (*on_paste)(const char *mime, int fd));
// Take ownership of the clipboard with content c (moved into clip).
void clipboard_own(struct content *c);
void clipboard_snapshot(void);
// Hand a paste request what we hold, as mime.
void clipboard_send(const char *mime, int fd);
int clipboard_owned(void);
// Whether a read now is a paste rather than a watcher reacting to our set.
int clipboard_settled(void);
// The compositor announced a new clipboard. Returns whether it is ours.
int clipboard_changed(struct ext_data_control_offer_v1 *offer);

#endif
