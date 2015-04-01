Do the following to configure the system with Bluetooth.

1. Enable BTUSB support in the android/build/build-prescript file.

# Set to 'y' to enable Broadcom Bluetooth USB dongle (requires uinput and uhid in your kernel to be enabled)
export ANDROID_ENABLE_BTUSB=y

That's it! You're done!
