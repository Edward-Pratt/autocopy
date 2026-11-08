#define _GNU_SOURCE
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "notify.h"
#include "xdg.h"

#define PREVIEW_CHARS 80
// Each character becomes at most 5 bytes ("&amp;"), plus "…" and the NUL.
#define PREVIEW_SIZE (PREVIEW_CHARS * 5 + 4)

// On while $XDG_CONFIG_HOME/autocopy/notify exists; autocopy-notify makes and
// removes it. Checked on every copy, so a change needs no restart.
int notify_enabled(void)
{
	static char path[4096];
	if (!path[0] && !xdg_path(path, sizeof path, "XDG_CONFIG_HOME", ".config", "autocopy/notify"))
		return 0;
	return access(path, F_OK) == 0;
}

// Length of the valid UTF-8 character at s (n bytes left), or 0. D-Bus
// rejects strings that are not valid UTF-8.
static size_t utf8_len(const unsigned char *s, size_t n)
{
	static const unsigned long min[] = { 0, 0, 0x80, 0x800, 0x10000 };
	unsigned c = s[0];
	size_t k = c < 0x80 ? 1 : c >= 0xc2 && c < 0xe0 ? 2 : c >= 0xe0 && c < 0xf0 ? 3 : c >= 0xf0 && c < 0xf5 ? 4 : 0;
	if (!k || k > n)
		return 0;
	unsigned long cp = k == 1 ? c : c & (0x7fu >> k);
	for (size_t i = 1; i < k; i++) {
		if ((s[i] & 0xc0) != 0x80)
			return 0;
		cp = cp << 6 | (s[i] & 0x3f);
	}
	// Overlong forms, surrogates and code points past U+10FFFF.
	if (cp < min[k] || (cp >= 0xd800 && cp < 0xe000) || cp > 0x10ffff)
		return 0;
	return k;
}

// The text on one line for a notification body: runs of whitespace and
// control characters as one space, markup characters escaped, invalid UTF-8
// as U+FFFD, and cut after PREVIEW_CHARS characters.
static void preview(char out[PREVIEW_SIZE], const char *text, size_t len)
{
	const unsigned char *s = (const unsigned char *)text;
	char *o = out;
	size_t chars = 0, k;
	int gap = 0;
	for (size_t i = 0; i < len; i += k ? k : 1) {
		k = utf8_len(s + i, len - i);
		if (k == 1 && (s[i] <= ' ' || s[i] == 0x7f)) {
			gap = chars > 0;
			continue;
		}
		if (chars + gap >= PREVIEW_CHARS) {
			o = stpcpy(o, "…");
			break;
		}
		if (gap) {
			*o++ = ' ';
			chars++;
			gap = 0;
		}
		if (!k)
			o = stpcpy(o, "\xef\xbf\xbd");
		else if (s[i] == '&')
			o = stpcpy(o, "&amp;");
		else if (s[i] == '<')
			o = stpcpy(o, "&lt;");
		else if (s[i] == '>')
			o = stpcpy(o, "&gt;");
		else
			o = mempcpy(o, s + i, k);
		chars++;
	}
	*o = 0;
}

void notify_copied(const char *text, size_t len)
{
	if (!notify_enabled() || !len)
		return;
	char body[PREVIEW_SIZE];
	preview(body, text, len);
	// "--" first: a highlight starting with "-" is not an option. The hint
	// makes servers that know it replace the last copy's notification.
	char *argv[] = {
		"notify-send", "--app-name=autocopy", "--urgency=low", "--transient", "--expire-time=1500",
		"--hint=string:x-canonical-private-synchronous:autocopy", "--", "Copied", body, NULL,
	};
	pid_t pid;
	int err = posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ);
	if (err)
		logf_("cannot run notify-send: %s", strerror(err));
}
