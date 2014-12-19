#!/bin/bash

if [ $# -lt 6 ]
then
  echo "Usage: sudo ./unpack_android_multipart.sh <image_dir> <rootfs_dir> <system_dir> <data_dir> <cache_dir> <kernel_dir> [sd<b-z>]"
  echo "       image_dir  : directory containing boot.img, system.img and userdata.img."
  echo "                    Ex. out/target/product/bcm_platform"
  echo "       rootfs_dir : EMPTY output directory for root partition. Ex. /media/rootfs"
  echo "       system_dir : EMPTY output directory for system partition. Ex. /media/system"
  echo "       data_dir   : EMPTY output directory for data partition. Ex. /media/data"
  echo "       cache_dir  : EMPTY output directory for cache partition. Ex. /media/cache"
  echo "       kernel_dir : EMPTY output directory for kernel partition. Ex. /media/kernel"
  echo "       sd<b-z>    : Any letter from b to z, corresponding to your usb drive."
  echo "                    'a' is not permitted because it is probably your main hard drive,"
  echo "                    and this script first deletes existing partitions."
  echo "       NOTE: This script assumes that the USB drive was formatted by format_android_usb_multipart_16GB.sh"
  exit
fi

echo ""
echo "******** IMPORTANT ********" 
echo "  This script assumes that the USB drive was formatted by format_android_usb_multipart_16GB.sh"
echo ""
sleep 2

if [ $7 ]
then
  echo "Path to usb device: /dev/$7 supplied..."
  if [ "$7" == "sda" ]
  then
    echo "You have specified 'sda' which is likely your primary hard drive."
    echo "This script will DELETE and replace any exising partition. You "
    echo "must specify the location of your USB key that Android will be"
    echo "installed on. Exiting."
    exit
  fi
# mount /media/rootfs
  if [ -e $2 ]
  then
    umount $2
  else
    mkdir -p $2
  fi
  mount /dev/${7}2 $2
# mount /media/system
  if [ -e $3 ]
  then
    umount $3
  else
    mkdir -p $3
  fi
  mount /dev/${7}3 $3
# mount /media/data
  if [ -e $4 ]
  then
    umount $4
  else
    mkdir -p $4
  fi
  mount /dev/${7}4 $4
# mount /media/cache
  if [ -e $5 ]
  then
    umount $5
  else
    mkdir -p $5
  fi
  mount /dev/${7}5 $5
  rm -rf $5/*
# mount /media/kernel
  if [ -e $6 ]
  then
    umount $6
  else
    mkdir -p $6
  fi
  mount /dev/${7}1 $6
  rm -rf $6/*
fi

# Cleanup in case last run was interrupted
umount ./boot/system
umount ./boot/data
rm -rf ./boot

./tools/split_boot $1/boot.img
mv boot/boot.img-kernel kernel
cp kernel $6
./tools/simg2img $1/system.img ./boot/system.raw.img
./tools/simg2img $1/userdata.img ./boot/userdata.raw.img

mkdir ./boot/system
mkdir ./boot/data
mount -t ext4 -o loop ./boot/system.raw.img ./boot/system
mount -t ext4 -o loop ./boot/userdata.raw.img ./boot/data

rsync -av --delete ./boot/ramdisk/* $2
rsync -av --delete ./boot/system/* $3
rsync -av --delete ./boot/data/* $4

# Cleanup
umount ./boot/system
umount ./boot/data
rm -rf ./boot
cd /
umount $2
umount $3
umount $4
umount $5
umount $6
rm -rf $2
rm -rf $3
rm -rf $4
rm -rf $5
rm -rf $6
