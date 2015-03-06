#!/bin/bash

if [ "$(id -u)" != "0" ]; then
  echo "Script must be run with sudo."
  exit
fi

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

IMAGE_DIR=$1
ROOTFS_DIR=$2
SYSTEM_DIR=$3
DATA_DIR=$4
CACHE_DIR=$5
KERNEL_DIR=$6

red='\033[0;31m'
no_colour='\033[0m' # No Color

function cleanup {
  if [ $# -ne 0 ]
  then
    echo -e "${red}!!!!!!!!!!!!!!!!!!!!!!! $1 !!!!!!!!!!!!!!!!!!!!!!!${no_colour}"
  fi

  umount $ROOTFS_DIR &> /dev/null
  umount $SYSTEM_DIR &> /dev/null
  umount $DATA_DIR   &> /dev/null
  umount $CACHE_DIR  &> /dev/null
  umount $KERNEL_DIR &> /dev/null
  rm -rf $ROOTFS_DIR &> /dev/null
  rm -rf $SYSTEM_DIR &> /dev/null
  rm -rf $DATA_DIR   &> /dev/null
  rm -rf $CACHE_DIR  &> /dev/null
  rm -rf $KERNEL_DIR &> /dev/null
}

if [ $7 ]
then
  USB_DEV_DIR=/dev/$7
  echo "Path to usb device: $USB_DEV_DIR supplied..."
  if [ "$7" == "sda" ]
  then
    echo "You have specified 'sda' which is likely your primary hard drive."
    echo "This script will DELETE and replace any exising partition. You "
    echo "must specify the location of your USB key that Android will be"
    echo "installed on. Exiting."
    exit
  fi

  if [ ! -e $USB_DEV_DIR ]
  then
    { cleanup "$USB_DEV_DIR does not exist"; exit; }
  fi

  umount ${USB_DEV_DIR}1 &> /dev/null
  umount ${USB_DEV_DIR}2 &> /dev/null
  umount ${USB_DEV_DIR}3 &> /dev/null
  umount ${USB_DEV_DIR}4 &> /dev/null
  umount ${USB_DEV_DIR}5 &> /dev/null
  umount ${USB_DEV_DIR}6 &> /dev/null
  umount ${USB_DEV_DIR}7 &> /dev/null
  umount ${USB_DEV_DIR}8 &> /dev/null
  umount $ROOTFS_DIR     &> /dev/null
  umount $SYSTEM_DIR     &> /dev/null
  umount $DATA_DIR       &> /dev/null
  umount $CACHE_DIR      &> /dev/null
  umount $KERNEL_DIR     &> /dev/null
  rm -rf $ROOTFS_DIR
  rm -rf $SYSTEM_DIR
  rm -rf $DATA_DIR
  rm -rf $CACHE_DIR
  rm -rf $KERNEL_DIR

# mount rootfs
  mkdir -p $ROOTFS_DIR
  mount ${USB_DEV_DIR}2 $ROOTFS_DIR || { cleanup "rootfs mount failed"; exit; }
# mount system
  mkdir -p $SYSTEM_DIR
  mount ${USB_DEV_DIR}3 $SYSTEM_DIR || { cleanup "system mount failed"; exit; }
# mount data
  mkdir -p $DATA_DIR
  mount ${USB_DEV_DIR}4 $DATA_DIR   || { cleanup "data mount failed";   exit; }
# mount cache
  mkdir -p $CACHE_DIR
  mount ${USB_DEV_DIR}5 $CACHE_DIR  || { cleanup "cache mount failed";  exit; }
  rm -rf $CACHE_DIR/*
# mount kernel
  mkdir -p $KERNEL_DIR
  mount ${USB_DEV_DIR}1 $KERNEL_DIR || { cleanup "kernel mount failed"; exit; }
  rm -rf $KERNEL_DIR/*
fi

# Cleanup in case last run was interrupted
umount ./boot/system &> /dev/null
umount ./boot/data   &> /dev/null
rm -rf ./boot

./tools/split_boot $IMAGE_DIR/boot.img
cp boot/boot.img-kernel $KERNEL_DIR/kernel
./tools/simg2img $IMAGE_DIR/system.img ./boot/system.raw.img
./tools/simg2img $IMAGE_DIR/userdata.img ./boot/userdata.raw.img

mkdir ./boot/system
mkdir ./boot/data
mount -t ext4 -o loop ./boot/system.raw.img ./boot/system
mount -t ext4 -o loop ./boot/userdata.raw.img ./boot/data

rsync -av --delete ./boot/ramdisk/ $ROOTFS_DIR
rsync -av --delete ./boot/system/ $SYSTEM_DIR
rsync -av --delete ./boot/data/ $DATA_DIR

# Cleanup
umount ./boot/system
umount ./boot/data
rm -rf ./boot
cd /
cleanup
