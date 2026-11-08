// Logging to stderr (the journal under systemd) and a monotonic clock.
#ifndef AUTOCOPY_LOG_H
#define AUTOCOPY_LOG_H

long now_ms(void);
void logf_(const char *fmt, ...);

#endif
