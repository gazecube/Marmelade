CC ?= cc
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2 -g
CPPFLAGS ?=

CPPFLAGS += -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -D_POSIX_C_SOURCE=200809L

MOTIF_CFLAGS := $(shell $(PKG_CONFIG) --cflags xm xt x11 2>/dev/null)
MOTIF_LIBS := $(shell $(PKG_CONFIG) --libs xm xt x11 2>/dev/null)
XPM_CFLAGS := $(shell $(PKG_CONFIG) --cflags xpm 2>/dev/null)
XPM_LIBS := $(shell $(PKG_CONFIG) --libs xpm 2>/dev/null)
XFT_CFLAGS := $(shell $(PKG_CONFIG) --cflags xft fontconfig 2>/dev/null)
XFT_LIBS := $(shell $(PKG_CONFIG) --libs xft fontconfig 2>/dev/null)
ifeq ($(strip $(MOTIF_LIBS)),)
MOTIF_LIBS := -lXm -lXt -lX11
endif
ifeq ($(strip $(XPM_LIBS)),)
XPM_LIBS := -lXpm
endif
ifneq ($(strip $(XFT_LIBS)),)
CPPFLAGS += -DMARMELADE_USE_XFT=1
endif

TARGET := marmelade
VERSION := $(shell sed -n '1p' VERSION)
SOURCES := src/main.c src/app_state.c src/json.c src/ui_icons.c src/icons.c src/browser.c src/sidebar.c src/view.c src/artwork.c src/grid.c src/player.c src/bridge_client.c src/utf8_text.c
OBJECTS := $(SOURCES:.c=.o)

.PHONY: all clean run bridge check dist

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(MOTIF_LIBS) $(XPM_LIBS) $(XFT_LIBS)

src/utf8_text.o: src/utf8_text.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MOTIF_CFLAGS) $(XFT_CFLAGS) -std=c99 -Wall -Wextra -Wpedantic -c -o $@ $<

src/%.o: src/%.c src/bridge_client.h src/app_internal.h src/utf8_text.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MOTIF_CFLAGS) $(XPM_CFLAGS) $(XFT_CFLAGS) -include src/utf8_text.h -std=c99 -Wall -Wextra -Wpedantic -c -o $@ $<

run: $(TARGET)
	./$(TARGET)

bridge:
	node bridge/server.mjs

check:
	node --check bridge/server.mjs
	node --check bridge/chromium.mjs
	node test/bridge-smoke.mjs
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc -std=c99 -Wall -Wextra -Wpedantic -o test/bridge-reuse test/bridge-reuse.c src/bridge_client.c
	node test/bridge-reuse.mjs
	rm -f test/bridge-reuse

clean:
	rm -f $(TARGET) $(OBJECTS) test/bridge-reuse

dist:
	tar -czf ../motif-apple-music-$(VERSION).tar.gz \
		--exclude='motif-apple-music/*.o' \
		--exclude='motif-apple-music/motif-apple-music' \
		--exclude='motif-apple-music/test/bridge-reuse' \
		-C .. motif-apple-music
