CC = gcc
PYTHON = python3
MODE ?= normal
BUILD = build/$(MODE)
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic

ifeq ($(MODE),normal)
CFLAGS += -O2
else ifeq ($(MODE),debug)
CFLAGS += -O0 -g3
else ifeq ($(MODE),sanitize)
CFLAGS += -O1 -g3 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address,undefined
else
$(error MODE must be normal, debug, or sanitize)
endif

.PHONY: all debug sanitize test clean
all: $(BUILD)/server $(BUILD)/client

debug:
	$(MAKE) MODE=debug all

sanitize:
	$(MAKE) MODE=sanitize all

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: src/%.c include/net_io.h Makefile | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/server: $(BUILD)/server.o $(BUILD)/net_io.o
	$(CC) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD)/client: $(BUILD)/client.o $(BUILD)/net_io.o
	$(CC) $^ $(LDFLAGS) $(LDLIBS) -o $@

test: all
	$(PYTHON) tests/test_network.py --bin-dir $(BUILD) $(TEST_ARGS)

# Fixed relative output path; no user-supplied path is recursively removed.
clean:
	rm -rf -- build

-include $(wildcard $(BUILD)/*.d)
