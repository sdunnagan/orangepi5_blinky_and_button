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

# Tools
CPP ?= cpp
DTC ?= dtc

# Platform (for include path convenience)
ARCH       ?= arm64
DTS_VENDOR ?= rockchip

# Inputs/outputs
OVERLAY_SRC := gpio_button.overlay.dts
DTBO        := $(CURDIR)/gpio_button.overlay.dtbo

# cpp include paths so dt-bindings and dtsi includes resolve
DTS_CPPFLAGS ?= -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
	-I"$(KDIR)/include" \
	-I"$(KDIR)/arch/$(ARCH)/boot/dts" \
	-I"$(KDIR)/arch/$(ARCH)/boot/dts/$(DTS_VENDOR)"

.PHONY: all help clean

all: $(DTBO)

help:
	@echo "Targets:"
	@echo "  all (default) - build $(DTBO)"
	@echo "  clean         - remove $(DTBO)"
	@echo ""
	@echo "Env:"
	@echo "  KERNEL_SRC_DIR=/path/to/linux-src  (required)"
	@echo ""
	@echo "Example:"
	@echo "  $$ export KERNEL_SRC_DIR=\$$HOME/projects/linux"
	@echo "  $$ $(MAKE)"

$(DTBO): $(OVERLAY_SRC)
	@command -v $(CPP) >/dev/null || { echo "ERROR: cpp not found"; exit 1; }
	@command -v $(DTC) >/dev/null || { echo "ERROR: dtc not found"; exit 1; }
	@[ -n "$(KDIR)" ] || { echo "ERROR: Set KERNEL_SRC_DIR (needed for dt-bindings includes)"; exit 1; }
	@test -f "$(KDIR)/Makefile" || { echo "ERROR: KERNEL_SRC_DIR does not look like a kernel tree: $(KDIR)"; exit 1; }
	$(CPP) $(DTS_CPPFLAGS) "$<" | $(DTC) -@ -I dts -O dtb -o "$@"
	@echo "Built overlay: $@"

clean:
	@rm -f -- "$(DTBO)"
	@echo "Cleaned overlay artifacts."
