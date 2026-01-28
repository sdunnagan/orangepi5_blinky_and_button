#------------------------------------------------------------------------------
# File:      drivers/gpio_button/Makefile
#
# Drivers:  Builds/installs the gpio_button module.
#------------------------------------------------------------------------------

KDIR   ?= $(KERNEL_SRC_DIR)
KBUILD ?= $(KERNEL_BUILD_DIR)

TARGET_HOST     ?=
TARGET_SSH_OPTS ?=
TARGET_SUDO     ?= sudo

MODULE_NAME ?= gpio_button

OVERLAY_DIR        ?= overlay
DTC                ?= dtc
FDTO               ?= fdtoverlay

DTS_SOURCE         ?= $(OVERLAY_DIR)/gpio_button.overlay.dts
DTBO               ?= $(OVERLAY_DIR)/gpio_button.overlay.dtbo

BOARD_DTB_SUBDIR   ?= arch/arm64/boot/dts
BOARD_DTB_NAME     ?= rockchip/rk3399-rockpro64.dtb
BOARD_DTB_REL      := $(BOARD_DTB_SUBDIR)/$(BOARD_DTB_NAME)

BASE_SYM_DTB       ?= $(OVERLAY_DIR)/rk3399-rockpro64.base-with-symbols.dtb
MERGED_DTB         ?= $(OVERLAY_DIR)/rk3399-rockpro64.with-gpio_button-merged.dtb

TARGET_CUSTOM_DIR  ?= /boot/dtb-custom
TARGET_CUSTOM_DTB  ?= $(TARGET_CUSTOM_DIR)/rk3399-rockpro64-gpio-button.dtb
TARGET_GRUB2_CFG   ?= /boot/grub2/custom.cfg
TARGET_EFI_CFG     ?= /boot/efi/EFI/fedora/custom.cfg
TARGET_41_CUSTOM   ?= /etc/grub.d/41_custom

.PHONY: all clean kernelrelease \
        install uninstall \
        install-remote uninstall-remote \
        check-env check-prepped check-overlay-tools

all: check-env check-prepped
	@echo "Building $(MODULE_NAME) against O=$(KBUILD)"
	env -u KBUILD_EXTMOD \
	$(MAKE) -C "$(KDIR)" O="$(KBUILD)" M="$(CURDIR)" modules

clean: check-env
	env -u KBUILD_EXTMOD \
	$(MAKE) -C "$(KDIR)" O="$(KBUILD)" M="$(CURDIR)" clean
	@rm -f *.o *.ko *.mod.c Module.symvers modules.order .*.cmd

kernelrelease: check-env
	@$(MAKE) -s -C "$(KDIR)" O="$(KBUILD)" kernelrelease

install: all
	env -u KBUILD_EXTMOD \
	$(MAKE) -C "$(KDIR)" O="$(KBUILD)" M="$(CURDIR)" modules_install
	@KREL="$$( $(MAKE) -s -C "$(KDIR)" O="$(KBUILD)" kernelrelease )"; \
	echo "depmod $$KREL"; depmod "$$KREL" || true

uninstall: check-env
	@KREL="$$( $(MAKE) -s -C "$(KDIR)" O="$(KBUILD)" kernelrelease )"; \
	echo "Removing /lib/modules/$$KREL/extra/$(MODULE_NAME).ko"; \
	rm -f "/lib/modules/$$KREL/extra/$(MODULE_NAME).ko"; \
	echo "depmod $$KREL"; depmod "$$KREL" || true

install-remote: all
	@[ -n "$(TARGET_HOST)" ] || (echo "ERROR: TARGET_HOST not set"; exit 1)
	@KREL="$$( $(MAKE) -s -C "$(KDIR)" O="$(KBUILD)" kernelrelease )"; \
	echo "REMOTE install $(MODULE_NAME).ko for $$KREL to $(TARGET_HOST)"; \
	scp $(TARGET_SSH_OPTS) "$(MODULE_NAME).ko" $(TARGET_HOST):/tmp/$(MODULE_NAME).ko.$$KREL; \
	ssh $(TARGET_SSH_OPTS) $(TARGET_HOST) 'KREL='"$$KREL"' ; set -e; \
	  $(TARGET_SUDO) install -D -m 0644 "/tmp/$(MODULE_NAME).ko.$$KREL" "/lib/modules/$$KREL/extra/$(MODULE_NAME).ko"; \
	  $(TARGET_SUDO) depmod -a "$$KREL"; \
	  $(TARGET_SUDO) rm -f "/tmp/$(MODULE_NAME).ko.$$KREL"'

uninstall-remote: check-env
	@[ -n "$(TARGET_HOST)" ] || (echo "ERROR: TARGET_HOST not set"; exit 1)
	@KREL="$$( $(MAKE) -s -C "$(KDIR)" O="$(KBUILD)" kernelrelease )"; \
	echo "REMOTE uninstall $(MODULE_NAME).ko for $$KREL from $(TARGET_HOST)"; \
	ssh $(TARGET_SSH_OPTS) $(TARGET_HOST) '$(TARGET_SUDO) rm -f "/lib/modules/'"$$KREL"'/extra/$(MODULE_NAME).ko" || true'; \
	ssh $(TARGET_SSH_OPTS) $(TARGET_HOST) '$(TARGET_SUDO) depmod '"$$KREL"' || true'

check-env:
	@[ -n "$(KDIR)" ]   || (echo "ERROR: KERNEL_SRC_DIR not set"   >&2; exit 1)
	@[ -n "$(KBUILD)" ] || (echo "ERROR: KERNEL_BUILD_DIR not set" >&2; exit 1)

check-prepped:
	@[ -f "$(KBUILD)/include/generated/autoconf.h" ] || \
	  (echo "ERROR: $(KBUILD) not prepared; run: make -C '$(KDIR)' O='$(KBUILD)' olddefconfig modules_prepare" >&2; exit 2)
	@[ -f "$(KBUILD)/Module.symvers" ] || \
	  (echo "WARNING: $(KBUILD)/Module.symvers missing; modversions/deps may be incomplete" >&2; exit 0)

# -----------------------------------------------------------------------------
# Overlay (compile-time) helpers — no remote or GRUB, just local build/merge.
.PHONY: dtb-overlay dtb-merge

dtb-overlay:
	$(MAKE) -C overlay overlay

dtb-merge:
	$(MAKE) -C overlay merge
