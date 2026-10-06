CC ?= cc
PREFIX ?= /usr/local
PKG_CONFIG ?= pkg-config
CPPFLAGS += -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -Isrc
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow
CURSES_CFLAGS := $(filter-out -D_XOPEN_SOURCE=%,$(shell $(PKG_CONFIG) --cflags ncursesw 2>/dev/null))
CURSES_LIBS := $(shell $(PKG_CONFIG) --libs ncursesw 2>/dev/null || echo -lncursesw)
LDLIBS += $(CURSES_LIBS) -pthread
SOURCES := $(wildcard src/*.c)
OBJECTS := $(patsubst src/%.c,build/%.o,$(SOURCES))

.PHONY: all clean test integration check sanitize install uninstall demo
all: termnav
termnav: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CURSES_CFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
build/test_fs: tests/test_fs_ops.c src/fs_ops.c src/preview.c src/async_ops.c src/trash.c src/usage.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ -pthread -o $@
build/test_features: tests/test_features.c src/fs_ops.c src/trash.c src/usage.c src/history.c src/commands.c src/async_ops.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ -pthread -o $@
build/test_sixel: tests/test_sixel.c src/sixel.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $@
build/app_test_main.o: src/main.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CURSES_CFLAGS) $(CFLAGS) -MMD -MP -Dmain=termnav_main -c $< -o $@
build/test_app: tests/test_app.c build/app_test_main.o $(filter-out build/main.o,$(OBJECTS))
	$(CC) $(CPPFLAGS) $(CURSES_CFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
test: build/test_fs build/test_features build/test_sixel build/test_app
	./build/test_fs
	./build/test_features
	./build/test_sixel
	./build/test_app
integration: termnav
	python3 tests/test_home.py
	python3 tests/test_tui.py
	python3 tests/test_features.py
	python3 tests/test_image_preview.py
	python3 tests/test_preview_helper.py
check: test integration
sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' check
install: termnav
	install -Dm755 termnav $(DESTDIR)$(PREFIX)/bin/termnav
	install -Dm644 scripts/preview_helper.py $(DESTDIR)$(PREFIX)/share/termnav/preview_helper.py
	install -Dm644 docs/termnav.1 $(DESTDIR)$(PREFIX)/share/man/man1/termnav.1
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/termnav $(DESTDIR)$(PREFIX)/share/man/man1/termnav.1 $(DESTDIR)$(PREFIX)/share/termnav/preview_helper.py
demo: termnav
	python3 scripts/create_demo.py
	./termnav --home test-playground
clean:
	rm -rf build termnav
-include $(OBJECTS:.o=.d) build/app_test_main.d
