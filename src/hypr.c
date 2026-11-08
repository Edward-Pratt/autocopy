#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "hypr.h"
#include "log.h"

char focused[64];
char focused_class[128];

// Copy one field of an IPC event or JSON reply, up to its delimiter.
static void copy_field(char *dst, size_t cap, const char *src)
{
	snprintf(dst, cap, "%.*s", (int)strcspn(src, ",\n\""), src);
}

static void set_focused(const char *addr)
{
	if (!strncmp(addr, "0x", 2))
		addr += 2;
	copy_field(focused, sizeof focused, addr);
}

static int hypr_socket(const char *name)
{
	const char *sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
	const char *rt = getenv("XDG_RUNTIME_DIR");
	if (!sig || !rt)
		return -1;
	struct sockaddr_un sa = { .sun_family = AF_UNIX };
	if ((size_t)snprintf(sa.sun_path, sizeof sa.sun_path, "%s/hypr/%s/%s", rt, sig, name) >= sizeof sa.sun_path)
		return -1;
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;
	if (connect(fd, (struct sockaddr *)&sa, sizeof sa) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

// Seed the focused window, then return the event socket (or -1).
int hypr_connect(void)
{
	int fd = hypr_socket(".socket.sock");
	if (fd < 0)
		return -1;
	static const char req[] = "j/activewindow";
	if (write(fd, req, sizeof req - 1) == (ssize_t)(sizeof req - 1)) {
		char buf[8192];
		size_t n = 0;
		ssize_t k;
		while (n < sizeof buf - 1 && (k = read(fd, buf + n, sizeof buf - 1 - n)) > 0)
			n += (size_t)k;
		buf[n] = 0;
		if (getenv("AUTOCOPY_DEBUG"))
			logf_("activewindow reply: %zu bytes", n);
		char *p = strstr(buf, "\"address\": \"");
		if (p)
			set_focused(p + strlen("\"address\": \""));
		if ((p = strstr(buf, "\"class\": \"")))
			copy_field(focused_class, sizeof focused_class, p + strlen("\"class\": \""));
	}
	close(fd);
	return hypr_socket(".socket2.sock");
}

int hypr_read(int fd, void (*on_focus)(void))
{
	static char buf[4096];
	static size_t n;
	ssize_t k = read(fd, buf + n, sizeof buf - n - 1);
	if (k <= 0) {
		logf_("Hyprland event socket closed: window checks off");
		focused[0] = 0;
		close(fd);
		return -1;
	}
	n += (size_t)k;
	buf[n] = 0;
	char *line = buf, *nl;
	while ((nl = strchr(line, '\n'))) {
		*nl = 0;
		// activewindow (class,title) comes just before activewindowv2.
		if (!strncmp(line, "activewindow>>", 14))
			copy_field(focused_class, sizeof focused_class, line + 14);
		if (!strncmp(line, "activewindowv2>>", 16)) {
			set_focused(line + 16);
			on_focus();
		}
		line = nl + 1;
	}
	n = strlen(line);
	memmove(buf, line, n + 1);
	return 0;
}
