// Window classes seen releasing the primary selection, kept across restarts.
#ifndef AUTOCOPY_RELEASERS_H
#define AUTOCOPY_RELEASERS_H

void load_releasers(void);
int releases_primary(const char *class);
void add_releaser(const char *class);

#endif
