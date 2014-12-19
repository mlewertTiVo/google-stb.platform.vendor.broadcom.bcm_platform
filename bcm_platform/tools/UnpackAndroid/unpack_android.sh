#!/bin/bash

tools_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ $# -lt 3 ]
then
  echo "Usage: unpack_android.sh <image_dir> <rootfs_dir> <kernel_dir> [sd<b-z>]"
  echo "       image_dir  : Absolute path to the directory containing boot.img, system.img and userdata.img."
  echo "                    Ex. <workspace>/out/target/product/bcm_platform"
  echo "       rootfs_dir : Absolute path to an EMPTY output directory where the rootfs will be unpacked to."
  echo "                    Ex. /media/rootfs"
  echo "       kernel_dir : Absolute path to an output directory where the kernel image will be copied to."
  echo "                    Ex. /media/kernel"
  echo "       sd<b-z>    : Any letter from b to z, corresponding to your usb drive."
  echo "                    'a' is not permitted because it is probably your main hard drive,"
  echo "                    and this script first deletes existing partitions."
  echo "       NOTE: This script assumes that the USB drive was formatted by format_android_usb_8GB.sh"
  exit
fi

if [ -e "$2/boot" ]
  then
    echo ""
    echo "!!! ERROR !!!"
    echo " Either a file or folder named \"boot\" already exists in $2"
    echo " Please remove those in $2 manually if possible, or provide another rootfs_dir."
    echo ""
    exit
fi

echo ""
echo "******** IMPORTANT ********"
echo "  This script assumes that the USB drive was formatted by format_android_usb_8GB.sh"
echo ""
sleep 2

if [ $4 ]
then
  echo "Path to usb device: /dev/$4 supplied..."
  if [ "$4" == "sda" ]
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
  mount /dev/${4}2 $2
# mount /media/kernel
  if [ -e $3 ]
  then
    umount $3
  else
    mkdir -p $3
  fi
  mount /dev/${4}1 $3
fi

cd $2
$tools_dir/tools/split_boot $1/boot.img
mv boot/boot.img-kernel kernel
cp kernel $3
$tools_dir/tools/simg2img $1/system.img ./boot/system.raw.img
$tools_dir/tools/simg2img $1/userdata.img ./boot/userdata.raw.img
mount -t ext4 -o loop ./boot/system.raw.img ./boot/ramdisk/system
mount -t ext4 -o loop ./boot/userdata.raw.img ./boot/ramdisk/data

rsync -av --delete ./boot/ramdisk/* $2

# Cleanup
umount ./boot/ramdisk/system
umount ./boot/ramdisk/data
rm -rf ./boot
cd /
umount $2
umount $3
rm -rf $2
rm -rf $3
