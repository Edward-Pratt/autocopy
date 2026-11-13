// Tests for strip_discord_toolbar: make check.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "discord.h"

static int failed;

static void expect(const char *class, const char *in, const char *want)
{
	char *buf = strdup(in);
	size_t len = strip_discord_toolbar(class, buf, strlen(in));
	if (len != strlen(want) || memcmp(buf, want, len)) {
		printf("FAIL %s: %s\n  got:  %.*s\n  want: %s\n", class, in, (int)len, buf, want);
		failed = 1;
	}
	free(buf);
}

#define TOOLBAR "💀\nClick to react\n🔥\nClick to react\n👍\nClick to react\n" \
	"Quick Mention\nAdd Reaction\nReply\nForward\nMore"

int main(void)
{
	// The report: a double-click to reply highlighted only the toolbar.
	expect("vesktop", TOOLBAR, "");
	expect("discord", TOOLBAR "\n", "");
	// A message highlighted with the toolbar keeps the message.
	expect("vesktop", "Oh\n" TOOLBAR, "Oh");
	expect("vesktop", "Oh\r\n" TOOLBAR "\r\nAh\r\n", "Oh\nAh");
	expect("equibop", ":kekw:\nClick to react\nAdd Reaction\nEdit\nMore", "");
	// Spaces around labels, and blank lines inside the toolbar.
	expect("vesktop", "Oh\n  💀 \nClick to react \n\nAdd Reaction\n\tMore\n", "Oh");
	// A last message line before a bare "Click to react" is kept.
	expect("vesktop", "see you\nClick to react\nAdd Reaction\nMore", "see you");
	// A message that only says "Reply" or "More" is not a toolbar.
	expect("vesktop", "Reply\nMore", "Reply\nMore");
	expect("vesktop", "Add Reaction is a button", "Add Reaction is a button");
	// Other apps are left alone.
	expect("code", TOOLBAR, TOOLBAR);
	expect("firefox", "Oh", "Oh");
	if (!failed)
		printf("ok\n");
	return failed;
}
