PROTO_DIR := $(shell pkg-config --variable=pkgdatadir wayland-protocols)
PROTO := $(PROTO_DIR)/staging/ext-data-control/ext-data-control-v1.xml
CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra $(shell pkg-config --cflags wayland-client)
LDLIBS += $(shell pkg-config --libs wayland-client)

autocopy: src/autocopy.c src/ext-data-control-v1-protocol.c src/ext-data-control-v1-client-protocol.h
	$(CC) $(CFLAGS) -Isrc -o $@ src/autocopy.c src/ext-data-control-v1-protocol.c $(LDLIBS)

src/ext-data-control-v1-client-protocol.h: $(PROTO)
	wayland-scanner client-header $< $@

src/ext-data-control-v1-protocol.c: $(PROTO)
	wayland-scanner private-code $< $@

clean:
	rm -f autocopy src/ext-data-control-v1-protocol.c src/ext-data-control-v1-client-protocol.h

.PHONY: clean
