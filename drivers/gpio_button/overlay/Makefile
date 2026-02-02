# -----------------------------------------------------------------------------
# Build gpio_button device-tree overlay (.dtbo) for Orange Pi 5 Plus / RK3588.
#
# This Makefile ONLY builds the overlay. Applying it (merging into the boot DTB)
# is done manually on the target (orthanc) and documented elsewhere.
#
# Usage:
#   export KERNEL_SRC_DIR=/path/to/linux-src   # needed for dt-bindings includes
#   make
#
# Outputs (in this directory):
#   - gpio_button.overlay.dtbo
# -----------------------------------------------------------------------------

# Kernel source tree (needed so cpp can resolve #include <dt-bindings/...>)
KDIR ?= $(KERNEL_SRC_DIR)

CPP ?= cpp
DTC ?= dtc

ARCH       ?= arm64
DTS_VENDOR ?= rockchip

# Build both overlays
DTBOS := gpio_button.overlay.dtbo i2c_enable.overlay.dtbo

DTS_CPPFLAGS ?= -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
	-I"$(KDIR)/include" \
	-I"$(KDIR)/arch/$(ARCH)/boot/dts" \
	-I"$(KDIR)/arch/$(ARCH)/boot/dts/$(DTS_VENDOR)"

.PHONY: all help clean

all: $(DTBOS)

help:
	@echo "Targets:"
	@echo "  all (default) - build $(DTBOS)"
	@echo "  clean         - remove $(DTBOS)"
	@echo ""
	@echo "Env:"
	@echo "  KERNEL_SRC_DIR=/path/to/linux-src  (required)"
	@echo ""
	@echo "Example:"
	@echo "  $$ export KERNEL_SRC_DIR=\$$HOME/projects/linux"
	@echo "  $$ $(MAKE)"

# Generic rule: foo.overlay.dts -> foo.overlay.dtbo
%.overlay.dtbo: %.overlay.dts
	@command -v $(CPP) >/dev/null || { echo "ERROR: cpp not found"; exit 1; }
	@command -v $(DTC) >/dev/null || { echo "ERROR: dtc not found"; exit 1; }
	@[ -n "$(KDIR)" ] || { echo "ERROR: Set KERNEL_SRC_DIR (needed for dt-bindings includes)"; exit 1; }
	@test -f "$(KDIR)/Makefile" || { echo "ERROR: KERNEL_SRC_DIR does not look like a kernel tree: $(KDIR)"; exit 1; }
	$(CPP) $(DTS_CPPFLAGS) "$<" | $(DTC) -@ -I dts -O dtb -o "$@"
	@echo "Built overlay: $@"

clean:
	rm -f -- $(DTBOS)
	@echo "Cleaned overlay artifacts."
