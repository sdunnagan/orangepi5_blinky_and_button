#------------------------------------------------------------------------------
# File:         Makefile
#
# Description:  Top-level makefile for building and installing apps.
#------------------------------------------------------------------------------

# Discover app subdirs (each must contain its own Makefile)
APP_DIRS := $(patsubst %/Makefile,%,$(wildcard */Makefile))

# By default, assume binary name == dir name (blinky -> blinky, button -> button).
# Override on the command line if an app emits a differently named binary:
#   make uninstall APP_BINS="blinky my-special-name"
APP_BINS ?= $(notdir $(APP_DIRS))

# Per-arch out-of-tree build dir for apps
ARCH ?= $(shell uname -m)
BUILD_DIR ?= build-$(ARCH)
export BUILD_DIR ARCH


# Prefixes for install/uninstall paths
PREFIX        ?= /usr/local
TARGET_PREFIX ?= /usr/local

# Remote knobs (typically passed from the top-level Makefile)
TARGET_HOST     ?=
TARGET_SSH_OPTS ?=
TARGET_SUDO     ?= sudo

.PHONY: all clean install install-remote uninstall uninstall-remote $(APP_DIRS)

# Build everything by recursing
all: $(APP_DIRS)

$(APP_DIRS):
	$(MAKE) -C $@

# Local install: delegate to each app’s own install target.
install:
	@for d in $(APP_DIRS); do \
		echo "INSTALL apps/$$d"; \
		$(MAKE) -C $$d BUILD_DIR=$(BUILD_DIR) ARCH=$(ARCH) install; \
	done

# Remote install: delegate to each app’s own install-remote target.
install-remote:
	@for d in $(APP_DIRS); do \
		echo "INSTALL-REMOTE apps/$$d"; \
		$(MAKE) -C $$d BUILD_DIR=$(BUILD_DIR) ARCH=$(ARCH) install-remote \
			TARGET_HOST="$(TARGET_HOST)" \
			TARGET_SSH_OPTS="$(TARGET_SSH_OPTS)" \
			TARGET_SUDO="$(TARGET_SUDO)" \
			TARGET_PREFIX="$(TARGET_PREFIX)"; \
	done

# Local uninstall: remove /usr/local/bin/<app> files here.
uninstall:
	@for b in $(APP_BINS); do \
		echo "UNINSTALL local apps/$$b from $(PREFIX)/bin/$$b"; \
		rm -f "$(PREFIX)/bin/$$b" || true; \
	done

# Remote uninstall: remove binaries on the target host.
uninstall-remote:
	@[ -n "$(TARGET_HOST)" ] || (echo "ERROR: TARGET_HOST not set"; exit 1)
	@for b in $(APP_BINS); do \
		echo "UNINSTALL-REMOTE apps/$$b from $(TARGET_HOST):$(TARGET_PREFIX)/bin/$$b"; \
		ssh $(TARGET_SSH_OPTS) $(TARGET_HOST) '$(TARGET_SUDO) rm -f "$(TARGET_PREFIX)/bin/'"$$b"'" || true'; \
	done

# Clean each app dir
clean:
	@for d in $(APP_DIRS); do \
		echo "CLEAN apps/$$d"; \
		$(MAKE) -C $$d BUILD_DIR=$(BUILD_DIR) ARCH=$(ARCH) clean; \
	done
