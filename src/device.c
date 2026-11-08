#define _GNU_SOURCE
#include <stdlib.h>

#include "autocopy.h"
#include "clipboard.h"
#include "content.h"
#include "device.h"
#include "log.h"
#include "offer.h"

static struct wl_display *display;
// The compositor announces the current selections as soon as the device
// exists. The primary one then is whatever was highlighted before autocopy
// started, not a new highlight, so it is not copied over the clipboard.
static int seeding;

static void device_data_offer(void *data, struct ext_data_control_device_v1 *dev,
	struct ext_data_control_offer_v1 *offer)
{
	(void)data;
	(void)dev;
	track_offer(offer);
}

static void device_selection(void *data, struct ext_data_control_device_v1 *dev,
	struct ext_data_control_offer_v1 *offer)
{
	(void)data;
	(void)dev;
	if (!clipboard_changed(offer))
		autocopy_app_copied();
}

static void device_primary_selection(void *data, struct ext_data_control_device_v1 *dev,
	struct ext_data_control_offer_v1 *offer)
{
	(void)data;
	(void)dev;
	if (!offer) {
		if (!seeding)
			autocopy_primary_clear();
		return;
	}
	struct offer_info *oi = ext_data_control_offer_v1_get_user_data(offer);
	const char *mime = NULL;
	for (size_t i = 0; i < N_TEXT_MIMES && !mime; i++)
		if (offer_has(oi, text_mimes[i]))
			mime = text_mimes[i];
	// Password fields mark the selection; leave them alone.
	if (mime && !seeding && !offer_has(oi, "x-kde-passwordManagerHint")) {
		size_t len;
		char *text = receive(display, offer, mime, &len);
		if (text && len)
			autocopy_primary(text, len);
		else
			free(text);
	}
	destroy_offer(offer);
}

static void device_finished(void *data, struct ext_data_control_device_v1 *dev)
{
	(void)data;
	(void)dev;
	logf_("compositor closed the data control device");
	exit(1);
}

static const struct ext_data_control_device_v1_listener device_listener = {
	.data_offer = device_data_offer,
	.selection = device_selection,
	.finished = device_finished,
	.primary_selection = device_primary_selection,
};

void device_start(struct wl_display *dpy, struct ext_data_control_manager_v1 *manager,
	struct wl_seat *seat)
{
	display = dpy;
	struct ext_data_control_device_v1 *device = ext_data_control_manager_v1_get_data_device(manager, seat);
	ext_data_control_device_v1_add_listener(device, &device_listener, NULL);
	clipboard_start(display, manager, device, autocopy_paste);
	seeding = 1;
	wl_display_roundtrip(display);
	seeding = 0;
}
