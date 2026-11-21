// Window classes with paste-over: those seen releasing the primary selection,
// kept across restarts, and VS Code by rule.
#ifndef AUTOCOPY_RELEASERS_H
#define AUTOCOPY_RELEASERS_H

void load_releasers(void);
// class gets paste-over: learned, or VS Code.
int releases_primary(const char *class);
void add_releaser(const char *class);

#endif
