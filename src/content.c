#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "content.h"

const char *const text_mimes[N_TEXT_MIMES] = {
	"text/plain;charset=utf-8", "text/plain", "UTF8_STRING", "STRING", "TEXT",
};

int is_text(const char *mime)
{
	for (size_t i = 0; i < N_TEXT_MIMES; i++)
		if (!strcmp(mime, text_mimes[i]))
			return 1;
	return !strncmp(mime, "text/plain", 10);
}

void free_content(struct content *c)
{
	for (size_t i = 0; i < c->n; i++) {
		free(c->b[i].mime);
		free(c->b[i].data);
	}
	free(c->b);
	c->b = NULL;
	c->n = 0;
}

void add_blob(struct content *c, const char *mime, const char *data, size_t len)
{
	c->b = realloc(c->b, (c->n + 1) * sizeof *c->b);
	c->b[c->n].mime = strdup(mime);
	c->b[c->n].data = memcpy(malloc(len ? len : 1), data, len);
	c->b[c->n].len = len;
	c->n++;
}

struct blob *find_blob(struct content *c, const char *mime)
{
	for (size_t i = 0; i < c->n; i++)
		if (!strcmp(c->b[i].mime, mime))
			return &c->b[i];
	if (is_text(mime))
		for (size_t i = 0; i < c->n; i++)
			if (is_text(c->b[i].mime))
				return &c->b[i];
	return NULL;
}

// Hand data to a paste request. The reader may be slow or huge, so a child
// does the writing while the event loop carries on.
void write_blob(int fd, const char *data, size_t len)
{
	pid_t pid = fork();
	if (pid == 0) {
		while (len) {
			ssize_t k = write(fd, data, len);
			if (k <= 0)
				break;
			data += k;
			len -= k;
		}
		_exit(0);
	}
	close(fd);
}
