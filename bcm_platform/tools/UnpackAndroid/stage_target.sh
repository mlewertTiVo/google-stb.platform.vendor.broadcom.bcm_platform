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
mount -t ext4 -o loop ./boot/system.raw.img ./boot/ramdisk/system
mount -t ext4 -o loop ./boot/userdata.raw.img ./boot/ramdisk/data

echo "Copying filesystem over..."
cp -a ./boot/ramdisk/* /mnt/rootfs/

echo "Cleaning up..."
umount ./boot/ramdisk/system
umount ./boot/ramdisk/data
umount /mnt/kernel
umount /mnt/rootfs

echo "Done!!!"

