#!/bin/bash

if [ "$(id -u)" != "0" ]; then
  echo "Script must be run with sudo."
  exit
fi

if [ $# -lt 1 ]
then
  echo "Usage: sudo ./format_android_usb_multipart_16GB.sh sd<b-z>"
  echo "       <b-z>: Any letter from b to z, corresponding to your usb drive."
  echo "              'a' is not permitted because it is probably your main hard drive,"
  echo "              and this script first deletes existing partitions."
  echo "Example: sudo ./format_android_usb_multipart_16GB.sh sdf"
  echo "Note: Partition table requires a USB drive of 16GB or larger."
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
umount /media/cache
umount /media/recovery

gdisk -l /dev/$1
sgdisk -g -o /dev/$1
bs=512
gpt_size=$((            1024*1024/bs)) # 1MB alignment
kernel_size=$((  4*1024*1024*1024/bs)) # Enough space to fit kernel + sdcard
rootfs_size=$((     200*1024*1024/bs)) # This will be replaced by boot.img
system_size=$((    1024*1024*1024/bs)) # As per BoardConfig.mk
data_size=$((    4*1024*1024*1024/bs)) # As per BoardConfig.mk
cache_size=$((      256*1024*1024/bs)) # As per BoardConfig.mk
recovery_size=$((   128*1024*1024/bs)) # As per BoardConfig.mk
hwcfg_size=$((       20*1024*1024/bs)) # Config files, ex Playready drm.bin
pst_size=$((          1*1024*1024/bs)) # Persistent data (for GTS), arbitrary size

gpt_first=0; gpt_last=$((gpt_first+gpt_size-1))
kernel_first=$((gpt_last+1)); kernel_last=$((kernel_first+kernel_size-1))
rootfs_first=$((kernel_last+1)); rootfs_last=$((rootfs_first+rootfs_size-1))
system_first=$((rootfs_last+1)); system_last=$((system_first+system_size-1))
data_first=$((system_last+1)); data_last=$((data_first+data_size-1))
cache_first=$((data_last+1)); cache_last=$((cache_first+cache_size-1))
recovery_first=$((cache_last+1)); recovery_last=$((recovery_first+recovery_size-1))
hwcfg_first=$((recovery_last+1)); hwcfg_last=$((hwcfg_first+hwcfg_size-1))
pst_first=$((hwcfg_last+1)); pst_last=$((pst_first+pst_size-1))

sgdisk -n 1:$kernel_first:$kernel_last -t 1:0700 -c 1:"kernel"   /dev/$1
sgdisk -n 2:$rootfs_first:$rootfs_last           -c 2:"rootfs"   /dev/$1
sgdisk -n 3:$system_first:$system_last           -c 3:"system"   /dev/$1
sgdisk -n 4:$data_first:$data_last               -c 4:"data"     /dev/$1
sgdisk -n 5:$cache_first:$cache_last             -c 5:"cache"    /dev/$1
sgdisk -n 6:$recovery_first:$recovery_last       -c 6:"recovery" /dev/$1
sgdisk -n 7:$hwcfg_first:$hwcfg_last             -c 7:"hwcfg"    /dev/$1
sgdisk -n 8:$pst_first:$pst_last                 -c 8:"pst"      /dev/$1

mkfs.vfat -F 32 -n kernel   /dev/${1}1
mkfs.ext4 -L       rootfs   /dev/${1}2
mkfs.ext4 -L       system   /dev/${1}3
mkfs.ext4 -L       data     /dev/${1}4
mkfs.ext4 -L       cache    /dev/${1}5
mkfs.vfat -F 32 -n recovery /dev/${1}6
dd if=/dev/zero          of=/dev/${1}7 bs=$bs count=$hwcfg_size
dd if=/dev/zero          of=/dev/${1}8 bs=$bs count=$pst_size

gdisk -l /dev/$1

