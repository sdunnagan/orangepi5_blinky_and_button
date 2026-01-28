#------------------------------------------------------------------------------
# File:         Makefile
#
# Description:  Top-level makefile for building and installing drivers.
#
# Notes:
# - Export two env vars that point to your kernel workspace:
#     * KERNEL_SRC_DIR:   the kernel *source* tree (srctree)
#     * KERNEL_BUILD_DIR: the kernel *build* tree (objtree, i.e. O=)
# - Drivers will build as external modules against the O= tree.
# ------------------------------------------------------------------------------

# Discover driver subdirs by presence of a wrapper Makefile.
DRV_DIRS := $(patsubst %/Makefile,%,$(wildcard */Makefile))

# Kernel build paths.
KERNEL_SRC_DIR   ?=
KERNEL_BUILD_DIR ?=

ifndef KERNEL_SRC_DIR
$(error KERNEL_SRC_DIR not set; export it or pass on the make command line)
endif
ifndef KERNEL_BUILD_DIR
$(error KERNEL_BUILD_DIR not set; export it or pass on the make command line)
endif

# Remote defaults (propagated down to sub-makes).
TARGET_HOST     ?=
TARGET_SSH_OPTS ?=
TARGET_SUDO     ?= sudo

.PHONY: all drivers clean install uninstall install-remote uninstall-remote prepare-kbuild list $(DRV_DIRS)

all: prepare-kbuild $(DRV_DIRS)

drivers: all

# Minimal prep for external modules:
#  - Ensure autoconf.h exists (olddefconfig + modules_prepare)
#  - Warn if Module.symvers is missing (modversions may be unavailable)
prepare-kbuild:
	@[ -f "$(KERNEL_BUILD_DIR)/include/generated/autoconf.h" ] || \
	  $(MAKE) -C "$(KERNEL_SRC_DIR)" O="$(KERNEL_BUILD_DIR)" olddefconfig modules_prepare
	@[ -f "$(KERNEL_BUILD_DIR)/Module.symvers" ] || \
	  { echo "NOTE: $(KERNEL_BUILD_DIR)/Module.symvers missing; external modules may lack modversions."; \
	    echo "      Build the kernel or run 'make -C \"$(KERNEL_SRC_DIR)\" O=\"$(KERNEL_BUILD_DIR)\" modules' if needed."; }

$(DRV_DIRS):
	$(MAKE) -C $@ \
		KERNEL_SRC_DIR="$(KERNEL_SRC_DIR)" \
		KERNEL_BUILD_DIR="$(KERNEL_BUILD_DIR)" \
		TARGET_HOST="$(TARGET_HOST)" \
		TARGET_SSH_OPTS="$(TARGET_SSH_OPTS)" \
		TARGET_SUDO="$(TARGET_SUDO)"

install: prepare-kbuild
	@for d in $(DRV_DIRS); do \
		echo "INSTALL drivers/$$d"; \
		$(MAKE) -C $$d install \
			KERNEL_SRC_DIR="$(KERNEL_SRC_DIR)" \
			KERNEL_BUILD_DIR="$(KERNEL_BUILD_DIR)"; \
	done

# Each driver’s `install-remote` target copies .ko into
# /lib/modules/<KREL>/extra on the target and runs depmod.
install-remote: prepare-kbuild
	@for d in $(DRV_DIRS); do \
		echo "INSTALL-REMOTE drivers/$$d"; \
		$(MAKE) -C $$d install-remote \
			KERNEL_SRC_DIR="$(KERNEL_SRC_DIR)" \
			KERNEL_BUILD_DIR="$(KERNEL_BUILD_DIR)" \
			TARGET_HOST="$(TARGET_HOST)" \
			TARGET_SSH_OPTS="$(TARGET_SSH_OPTS)" \
			TARGET_SUDO="$(TARGET_SUDO)"; \
	done

uninstall:
	@for d in $(DRV_DIRS); do \
		echo "UNINSTALL drivers/$$d"; \
		$(MAKE) -C $$d uninstall \
			KERNEL_SRC_DIR="$(KERNEL_SRC_DIR)" \
			KERNEL_BUILD_DIR="$(KERNEL_BUILD_DIR)"; \
	done

uninstall-remote:
	@for d in $(DRV_DIRS); do \
		echo "UNINSTALL-REMOTE drivers/$$d"; \
		$(MAKE) -C $$d uninstall-remote \
			KERNEL_SRC_DIR="$(KERNEL_SRC_DIR)" \
			KERNEL_BUILD_DIR="$(KERNEL_BUILD_DIR)" \
			TARGET_HOST="$(TARGET_HOST)" \
			TARGET_SSH_OPTS="$(TARGET_SSH_OPTS)" \
			TARGET_SUDO="$(TARGET_SUDO)"; \
	done

clean:
	@for d in $(DRV_DIRS); do \
		echo "CLEAN drivers/$$d"; \
		$(MAKE) -C $$d clean \
			KERNEL_SRC_DIR="$(KERNEL_SRC_DIR)" \
			KERNEL_BUILD_DIR="$(KERNEL_BUILD_DIR)"; \
	done

list:
	@echo "Driver subdirectories:"; \
	for d in $(DRV_DIRS); do echo "  - $$d"; done
