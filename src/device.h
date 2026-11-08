// The seat's data-control device: its clipboard and primary selection events,
// handed to the gesture logic in autocopy.c.
#ifndef AUTOCOPY_DEVICE_H
#define AUTOCOPY_DEVICE_H

#include <wayland-client.h>

#include "ext-data-control-v1-client-protocol.h"

// Start watching the seat's clipboard and primary selection.
void device_start(struct wl_display *display, struct ext_data_control_manager_v1 *manager,
	struct wl_seat *seat);

#endif
