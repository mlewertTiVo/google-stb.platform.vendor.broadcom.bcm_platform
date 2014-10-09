#!/bin/bash

echo ""
echo "******** IMPORTANT ********"
echo "  This script assumes that the USB drive was formatted by format_android_usb_multipart_8GB.sh"
echo ""
sleep 2

mkdir -p /mnt/kernel
mkdir -p /mnt/rootfs
mkdir -p /mnt/system
mkdir -p /mnt/data
mkdir -p /mnt/cache

echo "Mounting all paritions in the USB stick to target system"
mount /dev/sda1 /mnt/kernel
mount /dev/sda2 /mnt/rootfs
mount /dev/sda3 /mnt/system
mount /dev/sda4 /mnt/data
mount /dev/sda5 /mnt/cache

echo "Cleaning up all paritions (except for the kernel parition)"
rm -rf /mnt/rootfs/*
rm -rf /mnt/system/*
rm -rf /mnt/data/*
rm -rf /mnt/cache/*

cd /mnt/work/

echo "Copying kernel..."
cp ./kernel /mnt/kernel/

echo "Copying rootfs over..."
cp -a ./boot/ramdisk/* /mnt/rootfs/

echo "Copying system partition over..."
mkdir -p ./boot/system
mount -t ext4 -o loop ./boot/system.raw.img ./boot/system
cp -a ./boot/system/* /mnt/system/
umount ./boot/system

echo "Copying data partition over..."
mkdir -p ./boot/data
mount -t ext4 -o loop ./boot/userdata.raw.img ./boot/data
cp -a ./boot/data/* /mnt/data/
umount ./boot/data

echo "Cleaning up..."
umount /mnt/kernel
umount /mnt/rootfs
umount /mnt/system
umount /mnt/data
umount /mnt/cache

echo "Done!!!"

