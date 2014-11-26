#!/bin/bash

if [ $# -lt 1 ]
then
  echo "Usage: sudo ./format_android_usb_multipart_8GB.sh sd<b-z>"
  echo "       <b-z>: Any letter from b to z, corresponding to your usb drive."
  echo "              'a' is not permitted because it is probably your main hard drive,"
  echo "              and this script first deletes existing partitions."
  echo "Example: sudo ./format_android_usb_multipart_8GB.sh sdf"
  echo "Note: Partition table requires a USB drive of 8GB or larger."
  exit
fi

if [ "$1" == "sda" ]
then
  echo "You have specified 'sda' which is likely your primary hard drive."
  echo "This script will DELETE and replace any exising partition. You "
  echo "must specify the location of your USB key that Android will be"
  echo "installed on. Exiting."
  exit
fi

umount /media/kernel
umount /media/rootfs
umount /media/system
umount /media/data

gdisk -l /dev/$1
sgdisk -g -o /dev/$1
sgdisk -a 1 -n 1:34:8388642 -t 1:0700 -c 1:"kernel" /dev/$1
sgdisk -a 1 -n 2:8388643:8798243      -c 2:"rootfs" /dev/$1
sgdisk -a 1 -n 3:8798244:10895396     -c 3:"system" /dev/$1
sgdisk -a 1 -n 4:10895397:19284005    -c 4:"data"   /dev/$1
sgdisk -a 1 -n 5:19284006:21381158    -c 5:"cache"  /dev/$1

mkfs.vfat -F 32 -n kernel /dev/${1}1
mkfs.ext4 -L       rootfs /dev/${1}2
mkfs.ext4 -L       system /dev/${1}3
mkfs.ext4 -L       data   /dev/${1}4
mkfs.ext4 -L       cache  /dev/${1}5

