// The gesture logic: what the clipboard holds and what each paste gets.
#ifndef AUTOCOPY_AUTOCOPY_H
#define AUTOCOPY_AUTOCOPY_H

#include <wayland-client.h>

#include "ext-data-control-v1-client-protocol.h"

// Start watching the seat's clipboard and primary selection.
void autocopy_start(struct wl_display *display, struct ext_data_control_manager_v1 *manager,
	struct wl_seat *seat);
// When autocopy_tick has work to do, in now_ms() time, or -1 for never.
long autocopy_deadline(void);
void autocopy_tick(void);
// The focused window changed.
void autocopy_focus_changed(void);

#endif
