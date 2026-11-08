// Clipboard content held in memory: one blob of bytes per mime type.
#ifndef AUTOCOPY_CONTENT_H
#define AUTOCOPY_CONTENT_H

#include <stddef.h>

#define N_TEXT_MIMES 5
extern const char *const text_mimes[N_TEXT_MIMES];

struct blob {
	char *mime;
	char *data;
	size_t len;
};

struct content {
	struct blob *b;
	size_t n;
};

int is_text(const char *mime);
void free_content(struct content *c);
void add_blob(struct content *c, const char *mime, const char *data, size_t len);
struct blob *find_blob(struct content *c, const char *mime);
void write_blob(int fd, const char *data, size_t len);

#endif
