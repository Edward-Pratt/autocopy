// Double-clicking a message to reply (Vencord's MessageClickActions) also
// highlights the hover toolbar beside it, whose hidden labels come out as
// text: each quick reaction's emoji and "Click to react", then "Quick
// Mention", "Add Reaction", "Reply", "Forward", "More". Copied as is, that
// would replace the clipboard. The labels are matched in English only.
#define _GNU_SOURCE
#include <string.h>

#include "discord.h"

static const char *const labels[] = {
	"Click to react", "Quick Mention", "Add Reaction", "Add Super Reaction",
	"Reply", "Forward", "Edit", "More", "Create Thread",
};

#define MAX_EMOJI_LINE 64 // longest quick reaction line

struct line {
	const char *s;
	size_t len;
};

// Discord itself, or a client built on it: Vesktop, Equibop.
static int is_discord(const char *class)
{
	return strcasestr(class, "discord") || strcasestr(class, "vesktop") ||
		strcasestr(class, "equibop");
}

// The line without the spaces and tabs around it.
static struct line trimmed(struct line l)
{
	while (l.len && (*l.s == ' ' || *l.s == '\t'))
		l.s++, l.len--;
	while (l.len && (l.s[l.len - 1] == ' ' || l.s[l.len - 1] == '\t'))
		l.len--;
	return l;
}

static int line_is(struct line l, const char *s)
{
	l = trimmed(l);
	return l.len == strlen(s) && !memcmp(l.s, s, l.len);
}

// A quick reaction's text: an emoji (no ASCII letters or digits), or :name:
// for a custom one. Anything else is message text.
static int is_reaction(struct line l)
{
	l = trimmed(l);
	if (!l.len || l.len > MAX_EMOJI_LINE)
		return 0;
	if (l.len > 2 && l.s[0] == ':' && l.s[l.len - 1] == ':')
		return 1;
	for (size_t i = 0; i < l.len; i++)
		if ((l.s[i] >= '0' && l.s[i] <= '9') || ((l.s[i] | 32) >= 'a' && (l.s[i] | 32) <= 'z'))
			return 0;
	return 1;
}

static int is_label(struct line l)
{
	for (size_t i = 0; i < sizeof labels / sizeof *labels; i++)
		if (line_is(l, labels[i]))
			return 1;
	return 0;
}

// Line i can be part of a toolbar: a label, a reaction before its "Click to
// react", or a blank line between them.
static int toolbar_line(const struct line *ls, size_t n, size_t i)
{
	return is_label(ls[i]) || !trimmed(ls[i]).len ||
		(i + 1 < n && is_reaction(ls[i]) && line_is(ls[i + 1], "Click to react"));
}

// Split text into lines without their "\r\n" or "\n"; at most cap of them.
static size_t split_lines(char *text, size_t len, struct line *ls, size_t cap)
{
	size_t n = 0;
	const char *p = text, *end = text + len;
	while (p < end && n < cap) {
		const char *nl = memchr(p, '\n', (size_t)(end - p));
		const char *e = nl ? nl : end;
		ls[n].s = p;
		ls[n].len = (size_t)(e - p);
		if (ls[n].len && p[ls[n].len - 1] == '\r')
			ls[n].len--;
		n++;
		p = nl ? nl + 1 : end;
	}
	return p < end ? 0 : n; // too many lines: leave the text alone
}

// Mark the lines to drop: each run of toolbar lines that holds "Add Reaction",
// so a message that only says "Reply" or "More" is kept.
static int mark_toolbars(const struct line *ls, size_t n, char *drop)
{
	int any = 0;
	for (size_t i = 0; i < n;) {
		size_t j = i;
		int toolbar = 0;
		while (j < n && toolbar_line(ls, n, j))
			toolbar |= line_is(ls[j++], "Add Reaction");
		for (size_t k = i; k < j; k++)
			drop[k] = (char)toolbar;
		any |= toolbar;
		i = j > i ? j : i + 1;
	}
	return any;
}

size_t strip_discord_toolbar(const char *class, char *text, size_t len)
{
	enum { MAX_LINES = 1024 };
	struct line ls[MAX_LINES];
	char drop[MAX_LINES] = { 0 };
	if (!is_discord(class) || !memmem(text, len, "Add Reaction", 12))
		return len;
	size_t n = split_lines(text, len, ls, MAX_LINES);
	if (!n || !mark_toolbars(ls, n, drop))
		return len;
	// Join what is kept with "\n", dropping blank lines at either end.
	size_t out = 0, first = 0, last = n;
	while (first < n && (drop[first] || !ls[first].len))
		first++;
	while (last > first && (drop[last - 1] || !ls[last - 1].len))
		last--;
	for (size_t i = first; i < last; i++) {
		if (drop[i])
			continue;
		if (out)
			text[out++] = '\n';
		memmove(text + out, ls[i].s, ls[i].len);
		out += ls[i].len;
	}
	return out;
}
