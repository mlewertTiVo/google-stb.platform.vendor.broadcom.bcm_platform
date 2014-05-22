Do the following to configure the system with Bluetooth.

1. Enable BTUSB support in the android/build/build-prescript file.

# Set to 'y' to enable Broadcom Bluetooth USB dongle (requires uinput and uhid in your kernel to be enabled)
export ANDROID_ENABLE_BTUSB=y

2. If you have the btusb source directory, place in somewhere on your system (where you have permissions to 
build it) and edit the following line in android/build/build-prescript.

# Location of your BTUSB source directory, if the btusb.ko file is not present, the make system will jump
# to this location and build it, then copy it to vendor/broadcom/bcm(chip_id)/brcm_btusb
export BROADCOM_BTUSB_SOURCE_PATH=/opt/brcm/drivers/btusb

If you don't have the source, place the btusb.ko (compiled for your kernel) in the
android/src/boradcom/jb/vendor/broadcom/bcm_platform/brcm_btusb/driver directory.

3. If you are building from source in step 2, edit the following line in android/build/build-prescript to 
point to the toolchain bin directory that was used to build your kernel.

# Location of the toolchain bin directory that was used to build the kernel
# This is used for build-in-place drivers like Bluetooth, Wifi and MOCA
export ANDROID_KERNEL_TOOLCHAIN_BIN_PATH=/opt/toolchains/stbgcc-4.5.3-2.4/bin

4. Edit the android/src/broadcom/jb/vendor/broadcom/bcm_platform/prebuilt/init.rc file to uncomment the following lines.

# Load the BT USB driver
    insmod /system/vendor/broadcom/btusb/driver/btusb.ko
    chmod 0666 /dev/btusb0

