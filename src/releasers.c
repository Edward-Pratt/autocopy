#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "log.h"
#include "releasers.h"
#include "xdg.h"

#define MAX_RELEASERS 256

// Window classes seen releasing the primary selection (a clear with nothing
// after it), saved to releasers_path. Firefox never does: it keeps primary
// after the user clicks into another field, so there a live highlight says
// nothing about what a paste is aimed at.
static char releasers[MAX_RELEASERS][128];
static size_t n_releasers;
static char releasers_path[4096];

int releases_primary(const char *class)
{
	for (size_t i = 0; i < n_releasers; i++)
		if (!strcmp(releasers[i], class))
			return 1;
	return 0;
}

// Firefox (any build: firefox-esr, org.mozilla.firefox) keeps primary after a
// click elsewhere, yet it was once seen clearing it with nothing after, cause
// unknown. One such clear must not turn paste-over on for good, so it is never
// learned, and a line for it in the file is ignored.
static int keeps_primary(const char *class)
{
	return strcasestr(class, "firefox") != NULL;
}

static int remember_releaser(const char *class)
{
	if (!class[0] || n_releasers == MAX_RELEASERS || releases_primary(class) || keeps_primary(class))
		return 0;
	snprintf(releasers[n_releasers++], sizeof *releasers, "%s", class);
	return 1;
}

// $XDG_STATE_HOME/autocopy/releasers, one class per line.
void load_releasers(void)
{
	xdg_path(releasers_path, sizeof releasers_path, "XDG_STATE_HOME", ".local/state", "autocopy/releasers");
	FILE *f = releasers_path[0] ? fopen(releasers_path, "r") : NULL;
	if (!f)
		return;
	char line[sizeof *releasers];
	while (fgets(line, sizeof line, f)) {
		line[strcspn(line, "\n")] = 0;
		remember_releaser(line);
	}
	fclose(f);
}

void add_releaser(const char *class)
{
	if (keeps_primary(class)) {
		// Logged so the cause of these rare clears can be found.
		logf_("%s let go of the primary selection, but it keeps highlights: paste-over stays off", class);
		return;
	}
	if (!remember_releaser(class))
		return;
	logf_("%s releases the primary selection: paste-over on", class);
	if (!releasers_path[0])
		return;
	// mkdir -p for the two directories that may be missing.
	char dir[sizeof releasers_path];
	snprintf(dir, sizeof dir, "%s", releasers_path);
	*strrchr(dir, '/') = 0;
	char *up = strrchr(dir, '/');
	*up = 0;
	mkdir(dir, 0755);
	*up = '/';
	mkdir(dir, 0755);
	FILE *f = fopen(releasers_path, "a");
	if (!f) {
		logf_("cannot save %s: %s", releasers_path, strerror(errno));
		return;
	}
	fprintf(f, "%s\n", class);
	fclose(f);
}
