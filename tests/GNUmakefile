# Tests Makefile for Lumila AI
# Run: make test

PKG_CFLAGS = $(shell pkg-config --cflags glib-2.0 jansson libsoup-3.0)
PKG_LIBS   = $(shell pkg-config --libs glib-2.0 jansson libsoup-3.0)

CFLAGS  = -I../src -I../src/providers -Wall -Wextra -g $(PKG_CFLAGS)
LDLIBS  = $(PKG_LIBS)

TESTS = test_model_registry test_provider_base

.PHONY: all test clean

all: $(TESTS)

test: $(TESTS)
	@for t in $(TESTS); do \
		echo "--- Running $$t ---"; \
		./$$t || exit 1; \
		echo; \
	done
	@echo "=== All test suites passed ==="

test_model_registry: test_model_registry.c ../src/providers/model_registry.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_provider_base: test_provider_base.c ../src/providers/provider_base.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f $(TESTS)
