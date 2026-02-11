# Kernel driver development sandbox for the Orange Pi 5 Plus

## Overview

This repository contains a deliberately layered GPIO bring-up and validation project for the **Orange Pi 5 Plus**, combining direct userspace GPIO access with a custom kernel GPIO driver.

It is designed as a practical development and validation environment for:

- GPIO bring-up and validation on real hardware  
- Device Tree overlays and DTB merging  
- Writing, building, and deploying kernel modules  
- Userspace GPIO interaction using **libgpiod v2**  
- Coordinating kernel and userspace components with **systemd**

The project targets **aarch64 Fedora systems** running either upstream or custom kernels and is intended as a reusable sandbox for ongoing kernel development rather than a polished end-user application.

---

## Project Structure

The project consists of two userspace applications and one kernel module:

- **blinky**  
  A minimal userspace application that toggles an LED directly via the GPIO character device using **libgpiod v2**.  
  It serves as a sanity check for GPIO wiring, permissions, and the kernel GPIO stack.

- **gpio_button (kernel module)**  
  A custom GPIO driver defined via a Device Tree overlay.  
  It handles a button interrupt in kernel space and controls an associated LED.

- **button**  
  A userspace application that communicates with the `gpio_button` driver, exercising the full path from hardware interrupt, through the kernel driver, and up into userspace.

---

## Breadboard Wiring

This project uses a **Pi Cobbler** breakout board to connect the Orange Pi 5 Plus 40-pin header to a breadboard.  
The Pi Cobbler used here is silkscreened for Raspberry Pi.

All GPIO signals operate at **3.3 V**. The 5 V pin is not used. A common ground rail is shared by all components.

### Ground and Power Rails

- Breadboard ground rail → Pi Cobbler **GND** (third pin down, right column)
- Pi Cobbler **3v3** (first pin left column) used only for the pull-up resistor

### Yellow LED (libgpiod-controlled)

- LED cathode → ground
- LED anode → resistor → Pi Cobbler **TXD** (GPIO output)

### Red LED (kernel-driver-controlled)

- LED cathode → ground
- LED anode → resistor → Pi Cobbler **#25**

### Button and Pull-Up

- Button terminal → ground
- Button terminal → Pi Cobbler **#24**
- Pull-up resistor between **#24** and **3v3**

The button is active-low.

---

## Install a Custom Upstream Kernel on Orange Pi 5 Plus

Install packages on the build server:
```sh
$ sudo dnf install libgpiod-devel dtc
```

Install packages on the Orange Pi 5 Plus:
```sh
# sudo dnf install -y libgpiod libgpiod-utils dtc uboot-tools
```

Prepare an upstream kernel build workspace:
```sh
$ export KERNEL_SRC_DIR="/home/$UESR/projects/upstream_kernel_op5"
$ export KERNEL_BUILD_DIR="/home/$USER/projects/build_upstream_kernel_op5"
$ git clone https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git "$KERNEL_SRC_DIR"
$ mkdir $KERNEL_BUILD_DIR
```

Copy the kernel config from the Orange Pi 5 Plus:
```sh
$ ssh <orangepi5plus_host_name> 'cat /boot/config-$(uname -r)' > "$KERNEL_BUILD_DIR/.config"
```

Build and install your custom upstream kernel:
```sh
$ upstream_kernel_builder -s <kernel_id> -t <orangepi5plus_host_name> -a aarch64 -f -i grub -b
```

---

## Build the Project

```sh
$ make
$ make dt-overlay
$ tree
```

---

## Install Applications and Driver

```sh
$ TARGET_HOST=<orangepi5plus_host_name>
$ make install-remote   TARGET_HOST=$USER@$TARGET_HOST   TARGET_PREFIX=/usr/local   TARGET_SSH_OPTS="-o StrictHostKeyChecking=no"   TARGET_SUDO="sudo -n"
```

Verify:
```sh
$ which blinky
$ which button
$ modinfo gpio_button
$ lsmod | grep gpio_button
```

---

## Merge the Custom Device Tree Overlay

```sh
$ scp drivers/gpio_button/overlay/gpio_button.overlay.dtbo $USER@$TARGET_HOST:~
$ scp drivers/gpio_button/overlay/i2c_enable.overlay.dtbo $USER@$TARGET_HOST:~
```

Merge the custom device tree overlay on the target:
```sh
$ BASE=/boot/dtb/rockchip/rk3588-orangepi-5-plus.dtb
$ sudo cp -a "$BASE" "$BASE.$(date +%F-%H%M%S)"
$ TMP=/tmp/rk3588-orangepi-5-plus.dtb.$(date +%s)
$ sudo fdtoverlay \
  -i "$BASE" \
  -o "$TMP" \
  "/home/$USER/gpio_button.overlay.dtbo" \
  "/home/$USER/i2c_enable.overlay.dtbo"
$ ls -lh "$TMP"
$ sudo install -m 0644 "$TMP" "$BASE"

```

Verify:
```sh
$ sudo dtc -I dtb -O dts -o - /boot/dtb/rockchip/rk3588-orangepi-5-plus.dtb \
      | grep -n "gpio-button" -n
$ sudo dtc -I dtb -O dts -o - /boot/dtb/rockchip/rk3588-orangepi-5-plus.dtb \
      | sed -n '/i2c@feaa0000 {/,/};/p' | head -n 40
```

Edit the BLS entry to add:
```
devicetree /boot/dtb/rockchip/rk3588-orangepi-5-plus.dtb
```

Reboot and verify:
```sh
$ cat /sys/firmware/devicetree/base/gpio-button/compatible
$ sudo dtc -I fs -O dts -o /tmp/running.dts /sys/firmware/devicetree/base
$ grep -nA5 -B2 "gpio-button" /tmp/running.dts | sed -n '1,120p'
```

---

## Manual GPIO Testing

```sh
$ sudo usermod -a -G gpio $USER
$ sudo reboot
$ gpioinfo --version
$ gpiodetect
$ sudo gpioset -c gpiochip1 2=1
$ gpioset -c gpiochip1 1=1
$ sudo gpioget -c gpiochip3 14
```

---

## Manual Application Testing

```sh
$ blinky -D -c gpiochip1 -l 1 -i 250
$ sudo button
```

---

## Uninstall

```sh
$ pkill blinky
$ pkill button
$ sudo rmmod gpio_button
$ make uninstall-remote   TARGET_HOST=$USER@$TARGET_HOST   TARGET_PREFIX=/usr/local   TARGET_SSH_OPTS="-o StrictHostKeyChecking=no"   TARGET_SUDO="sudo -n"
$ make clean
```
