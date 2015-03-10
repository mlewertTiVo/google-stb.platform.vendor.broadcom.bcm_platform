#!/bin/bash

echo ""
echo "******** IMPORTANT ********"
echo "  This script assumes that the USB drive was formatted by format_android_usb_8GB.sh"
echo ""
sleep 2

mkdir -p /mnt/kernel
mkdir -p /mnt/rootfs

echo "Mounting all paritions in the USB stick to target system"
mount /dev/sda1 /mnt/kernel
mount /dev/sda2 /mnt/rootfs

echo "Cleaning up all paritions (except for the kernel parition)"
rm -rf /mnt/rootfs/*

cd /mnt/work/

echo "Copying kernel..."
cp ./kernel /mnt/kernel/

echo "Preparing target's rootfs..."
mount -t ext4 -o loop ./unpack_android_boot/system.raw.img ./unpack_android_boot/ramdisk/system
mount -t ext4 -o loop ./unpack_android_boot/userdata.raw.img ./unpack_android_boot/ramdisk/data

echo "Copying filesystem over..."
cp -a ./unpack_android_boot/ramdisk/* /mnt/rootfs/

echo "Cleaning up..."
umount ./unpack_android_boot/ramdisk/system
umount ./unpack_android_boot/ramdisk/data
umount /mnt/kernel
umount /mnt/rootfs

echo "Done!!!"

