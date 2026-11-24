// The mouse button release, which the compositor's bind sends as SIGUSR1.
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include "autocopy.h"
#include "button.h"
#include "xdg.h"

// Wayland does not tell other clients about mouse buttons, so the compositor
// does: a bind on the button's release sends SIGUSR1. The flag file says this
// autocopy understands it (to anything else SIGUSR1 is fatal), and goes when
// it exits.
static char button_flag[4096];

static void remove_button_flag(void)
{
	unlink(button_flag);
}

int button_start(void)
{
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	sigaddset(&set, SIGTERM);
	sigaddset(&set, SIGINT);
	if (sigprocmask(SIG_BLOCK, &set, NULL) < 0)
		return -1;
	int fd = signalfd(-1, &set, SFD_NONBLOCK | SFD_CLOEXEC);
	if (fd < 0) {
		sigprocmask(SIG_UNBLOCK, &set, NULL);
		return -1;
	}
	if (xdg_path(button_flag, sizeof button_flag, "XDG_RUNTIME_DIR", ".cache", "autocopy.button")) {
		FILE *f = fopen(button_flag, "w");
		if (f) {
			fclose(f);
			atexit(remove_button_flag);
		}
	}
	return fd;
}

// Returns 0 to go on, -1 when told to stop.
int button_read(int fd)
{
	struct signalfd_siginfo si;
	while (read(fd, &si, sizeof si) == sizeof si) {
		if (si.ssi_signo != SIGUSR1)
			return -1;
		autocopy_button_released();
	}
	return 0;
}
