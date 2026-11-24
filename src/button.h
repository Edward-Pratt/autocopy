// The mouse button release, which the compositor's bind sends as SIGUSR1.
#ifndef AUTOCOPY_BUTTON_H
#define AUTOCOPY_BUTTON_H

// Take SIGUSR1, SIGTERM and SIGINT through a signalfd and create the flag
// file. Returns the fd, or -1 when signals stay as they were.
int button_start(void);
// Handle pending signals. Returns 0 to go on, -1 when told to stop.
int button_read(int fd);
// In a forked child that stays autocopy: SIGTERM and SIGINT stop it again.
// SIGUSR1 stays blocked, since the bind's pkill reaches it too.
void button_child(void);

#endif
