#!/bin/bash

echo "Copying kernel..."
cp /mnt/work/kernel /mnt/usb0/

echo "Preparing target's filesystem..."
rm -rf /mnt/usb/*
cd /mnt/work/

mount -t ext4 -o loop ./boot/system.raw.img ./boot/ramdisk/system
mount -t ext4 -o loop ./boot/userdata.raw.img ./boot/ramdisk/data

echo "Copying filesystem over..."
cp -a ./boot/ramdisk/* /mnt/usb/

umount ./boot/ramdisk/system
umount ./boot/ramdisk/data

echo "Done!!!"

