PROTO_DIR := $(shell pkg-config --variable=pkgdatadir wayland-protocols)
PROTO := $(PROTO_DIR)/staging/ext-data-control/ext-data-control-v1.xml
CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra $(shell pkg-config --cflags wayland-client)
LDLIBS += $(shell pkg-config --libs wayland-client)

SRC := src/main.c src/autocopy.c src/clipboard.c src/content.c src/device.c src/discord.c src/hypr.c src/log.c src/notify.c src/offer.c src/releasers.c src/xdg.c
GEN := src/ext-data-control-v1-protocol.c src/ext-data-control-v1-client-protocol.h

autocopy: $(SRC) $(wildcard src/*.h) $(GEN)
	$(CC) $(CFLAGS) -Isrc -o $@ $(SRC) src/ext-data-control-v1-protocol.c $(LDLIBS)

src/ext-data-control-v1-client-protocol.h: $(PROTO)
	wayland-scanner client-header $< $@

src/ext-data-control-v1-protocol.c: $(PROTO)
	wayland-scanner private-code $< $@

check: tests/discord_test.c src/discord.c src/discord.h
	$(CC) $(CFLAGS) -Isrc -o tests/discord_test tests/discord_test.c src/discord.c
	./tests/discord_test

clean:
	rm -f autocopy tests/discord_test $(GEN)

.PHONY: check clean
