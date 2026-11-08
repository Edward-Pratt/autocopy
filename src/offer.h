// Data offers from other clients: the mime types they list, and reading one.
#ifndef AUTOCOPY_OFFER_H
#define AUTOCOPY_OFFER_H

#include <stddef.h>
#include <wayland-client.h>

#include "ext-data-control-v1-client-protocol.h"

struct offer_info {
	char **mimes;
	size_t n;
};

void track_offer(struct ext_data_control_offer_v1 *offer);
int offer_has(struct offer_info *oi, const char *mime);
void destroy_offer(struct ext_data_control_offer_v1 *offer);
char *receive(struct wl_display *display, struct ext_data_control_offer_v1 *offer, const char *mime, size_t *len);

#endif
