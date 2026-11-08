#define _GNU_SOURCE
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "offer.h"

#define MAX_BLOB (64u << 20)      // per mime type
#define RECEIVE_TIMEOUT_MS 1000   // a stalled clipboard owner must not hang us

static void offer_mime(void *data, struct ext_data_control_offer_v1 *offer, const char *mime)
{
	(void)offer;
	struct offer_info *oi = data;
	oi->mimes = realloc(oi->mimes, (oi->n + 1) * sizeof *oi->mimes);
	oi->mimes[oi->n++] = strdup(mime);
}

static const struct ext_data_control_offer_v1_listener offer_listener = { .offer = offer_mime };

void track_offer(struct ext_data_control_offer_v1 *offer)
{
	ext_data_control_offer_v1_add_listener(offer, &offer_listener, calloc(1, sizeof(struct offer_info)));
}

int offer_has(struct offer_info *oi, const char *mime)
{
	for (size_t i = 0; i < oi->n; i++)
		if (!strcmp(oi->mimes[i], mime))
			return 1;
	return 0;
}

void destroy_offer(struct ext_data_control_offer_v1 *offer)
{
	struct offer_info *oi = ext_data_control_offer_v1_get_user_data(offer);
	for (size_t i = 0; i < oi->n; i++)
		free(oi->mimes[i]);
	free(oi->mimes);
	free(oi);
	ext_data_control_offer_v1_destroy(offer);
}

// Read one mime type of an offer we do not own. Blocks the loop for at most
// RECEIVE_TIMEOUT_MS; nothing we own is ever read this way, so no deadlock.
char *receive(struct wl_display *display, struct ext_data_control_offer_v1 *offer, const char *mime, size_t *len)
{
	int p[2];
	*len = 0;
	if (pipe2(p, O_CLOEXEC) < 0)
		return NULL;
	ext_data_control_offer_v1_receive(offer, mime, p[1]);
	close(p[1]);
	wl_display_flush(display);

	char *buf = NULL;
	size_t cap = 0, n = 0;
	long deadline = now_ms() + RECEIVE_TIMEOUT_MS;
	for (;;) {
		long left = deadline - now_ms();
		struct pollfd pf = { p[0], POLLIN, 0 };
		if (left <= 0 || poll(&pf, 1, (int)left) <= 0)
			break;
		if (n + 65536 > cap) {
			cap = cap ? cap * 2 : 65536;
			buf = realloc(buf, cap);
		}
		ssize_t k = read(p[0], buf + n, cap - n);
		if (k <= 0)
			break;
		n += (size_t)k;
		if (n > MAX_BLOB)
			break;
	}
	close(p[0]);
	*len = n;
	if (getenv("AUTOCOPY_DEBUG"))
		logf_("receive %s: %zu bytes", mime, n);
	return buf;
}
