// autocopy's entry point: connect to Wayland and Hyprland, then run the loop.
#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <wayland-client.h>

#include "autocopy.h"
#include "ext-data-control-v1-client-protocol.h"
#include "hypr.h"
#include "log.h"
#include "releasers.h"

static struct wl_seat *seat;
static struct ext_data_control_manager_v1 *manager;

// ---- registry ------------------------------------------------------------

static void registry_global(void *data, struct wl_registry *reg, uint32_t name,
	const char *iface, uint32_t version)
{
	(void)data;
	(void)version;
	if (!strcmp(iface, wl_seat_interface.name) && !seat)
		seat = wl_registry_bind(reg, name, &wl_seat_interface, 1);
	else if (!strcmp(iface, ext_data_control_manager_v1_interface.name))
		manager = wl_registry_bind(reg, name, &ext_data_control_manager_v1_interface, 1);
}

static void registry_global_remove(void *data, struct wl_registry *reg, uint32_t name)
{
	(void)data;
	(void)reg;
	(void)name;
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

// ---- main ----------------------------------------------------------------

static int run(struct wl_display *display, int hypr)
{
	int wfd = wl_display_get_fd(display);
	for (;;) {
		while (wl_display_prepare_read(display) != 0)
			wl_display_dispatch_pending(display);
		wl_display_flush(display);

		int timeout = -1;
		long deadline = autocopy_deadline();
		if (deadline >= 0) {
			long left = deadline - now_ms();
			timeout = left > 0 ? (int)left : 0;
		}
		struct pollfd pf[2] = { { wfd, POLLIN, 0 }, { hypr, POLLIN, 0 } };
		if (poll(pf, hypr >= 0 ? 2 : 1, timeout) < 0 && errno != EINTR) {
			wl_display_cancel_read(display);
			break;
		}
		if (pf[0].revents & POLLIN)
			wl_display_read_events(display);
		else
			wl_display_cancel_read(display);
		if (wl_display_dispatch_pending(display) < 0)
			break;
		if (hypr >= 0 && (pf[1].revents & (POLLIN | POLLHUP)) &&
			hypr_read(hypr, autocopy_focus_changed) < 0)
			hypr = -1;
		autocopy_tick();
	}
	logf_("Wayland connection lost");
	return 1;
}

int main(void)
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	setvbuf(stderr, NULL, _IOLBF, 0);

	struct wl_display *display = wl_display_connect(NULL);
	if (!display) {
		logf_("cannot connect to the Wayland display");
		return 1;
	}
	struct wl_registry *reg = wl_display_get_registry(display);
	wl_registry_add_listener(reg, &registry_listener, NULL);
	wl_display_roundtrip(display);
	if (!seat || !manager) {
		logf_("compositor lacks ext-data-control-v1 (need Hyprland 0.47+, Sway with wlroots 0.19+)");
		return 1;
	}
	autocopy_start(display, manager, seat);

	load_releasers();
	int hypr = hypr_connect();
	if (hypr < 0)
		logf_("no Hyprland IPC: every paste commits the highlight");
	else
		logf_("watching selections; focused window %s", focused[0] ? focused : "(none)");
	return run(display, hypr);
}
