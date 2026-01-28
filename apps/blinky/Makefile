#------------------------------------------------------------------------------
# File:         Makefile
#
# Description:  Builds the `blinky` app.
#------------------------------------------------------------------------------

TARGET          := blinky
SRC             := blinky.c

ARCH            ?= aarch64
BUILD_DIR       ?= build-$(ARCH)
BINDIR          ?= $(BUILD_DIR)/bin

CC              ?= cc
CFLAGS          ?= -O2 -Wall -Wextra -Werror

# --- libgpiod2 + pthread via pkg-config ---
PKG             ?= pkg-config
GPIOD_PKG       ?= libgpiod
CFLAGS          += $(shell $(PKG) --cflags $(GPIOD_PKG))
CFLAGS          += -Wno-error=unused-parameter
LDLIBS          += $(shell $(PKG) --libs $(GPIOD_PKG)) -pthread

TARGET_HOST     ?=
TARGET_SSH_OPTS ?=
TARGET_SUDO     ?= sudo -n
TARGET_PREFIX   ?= /usr/local

.PHONY: all clean install-remote uninstall-remote check-remote

all: $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET): $(SRC)
	@mkdir -p "$(BINDIR)"
	$(CC) $(CFLAGS) -o "$@" $< $(LDLIBS)

clean:
	@rm -rf "$(BUILD_DIR)"

check-remote:
	@[ -n "$(TARGET_HOST)" ] || { echo "ERROR: TARGET_HOST not set"; exit 1; }
	@echo "Remote: host=$(TARGET_HOST) prefix=$(TARGET_PREFIX) sudo='$(TARGET_SUDO)' ssh_opts='$(TARGET_SSH_OPTS)'"

install-remote: check-remote $(BINDIR)/$(TARGET)
	@echo ">> Installing $(TARGET) to $(TARGET_HOST):$(TARGET_PREFIX)/bin/$(TARGET)"
	@scp $(TARGET_SSH_OPTS) "$(BINDIR)/$(TARGET)" "$(TARGET_HOST):/tmp/$(TARGET).tmp"
	@ssh $(TARGET_SSH_OPTS) "$(TARGET_HOST)" '\
		set -e; \
		$(TARGET_SUDO) install -D -m 0755 "/tmp/$(TARGET).tmp" "$(TARGET_PREFIX)/bin/$(TARGET)"; \
		rm -f "/tmp/$(TARGET).tmp"'

uninstall-remote: check-remote
	@echo ">> Uninstalling $(TARGET) from $(TARGET_HOST):$(TARGET_PREFIX)/bin/$(TARGET)"
	@ssh $(TARGET_SSH_OPTS) "$(TARGET_HOST)" '\
		set -e; \
		if [ -e "$(TARGET_PREFIX)/bin/$(TARGET)" ]; then \
			$(TARGET_SUDO) rm -f "$(TARGET_PREFIX)/bin/$(TARGET)"; \
			echo "Removed $(TARGET_PREFIX)/bin/$(TARGET)"; \
		else \
			echo "Not present: $(TARGET_PREFIX)/bin/$(TARGET)"; \
		fi'
