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

red='\033[0;31m'
no_colour='\033[0m' # No Color

function exit_msg {
  if [ $# -ne 0 ]
  then
    echo -e "${red}!!!!!!!!!!!!!!!!!!!!!!! $1 !!!!!!!!!!!!!!!!!!!!!!!${no_colour}"
  fi
}

USB_DEV_DIR=/dev/$1
umount ${USB_DEV_DIR}1 &> /dev/null
umount ${USB_DEV_DIR}2 &> /dev/null
umount ${USB_DEV_DIR}3 &> /dev/null
umount ${USB_DEV_DIR}4 &> /dev/null
umount ${USB_DEV_DIR}5 &> /dev/null
umount ${USB_DEV_DIR}6 &> /dev/null
umount ${USB_DEV_DIR}7 &> /dev/null
umount ${USB_DEV_DIR}8 &> /dev/null
umount ${USB_DEV_DIR}9 &> /dev/null

if [ ! -e $USB_DEV_DIR ]
then
  { exit_msg "$USB_DEV_DIR does not exist"; exit; }
fi

gdisk -l $USB_DEV_DIR
sgdisk -g -o $USB_DEV_DIR || { exit_msg "sgdisk failed"; exit; }
bs=512
gpt_size=$((            1024*1024/bs)) # 1MB alignment
kernel_size=$((  4*1024*1024*1024/bs)) # Enough space to fit kernel + sdcard
rootfs_size=$((     200*1024*1024/bs)) # This will be replaced by boot.img
system_size=$((    1024*1024*1024/bs)) # As per BoardConfig.mk
data_size=$((    4*1024*1024*1024/bs)) # As per BoardConfig.mk
cache_size=$((      256*1024*1024/bs)) # As per BoardConfig.mk
recovery_size=$((   128*1024*1024/bs)) # As per BoardConfig.mk
hwcfg_size=$((       20*1024*1024/bs)) # Config files, ex Playready drm.bin
boot_size=$((        32*1024*1024/bs)) # Enough space to store boot.img
misc_size=$((         1*1024*1024/bs)) # Small Misc Partition for recovery.img

gpt_first=0; gpt_last=$((gpt_first+gpt_size-1))
kernel_first=$((gpt_last+1)); kernel_last=$((kernel_first+kernel_size-1))
rootfs_first=$((kernel_last+1)); rootfs_last=$((rootfs_first+rootfs_size-1))
system_first=$((rootfs_last+1)); system_last=$((system_first+system_size-1))
data_first=$((system_last+1)); data_last=$((data_first+data_size-1))
cache_first=$((data_last+1)); cache_last=$((cache_first+cache_size-1))
recovery_first=$((cache_last+1)); recovery_last=$((recovery_first+recovery_size-1))
hwcfg_first=$((recovery_last+1)); hwcfg_last=$((hwcfg_first+hwcfg_size-1))
boot_first=$((hwcfg_last+1)); boot_last=$((boot_first+boot_size-1))
misc_first=$((boot_last+1)); misc_last=$((misc_first+misc_size-1))

sgdisk -n 1:$kernel_first:$kernel_last -t 1:0700 -c 1:"kernel"   $USB_DEV_DIR \
  || { exit_msg "kernel sgdisk failed";   exit; }
sgdisk -n 2:$rootfs_first:$rootfs_last           -c 2:"rootfs"   $USB_DEV_DIR \
  || { exit_msg "rootfs sgdisk failed";   exit; }
sgdisk -n 3:$system_first:$system_last           -c 3:"system"   $USB_DEV_DIR \
  || { exit_msg "system sgdisk failed";   exit; }
sgdisk -n 4:$data_first:$data_last               -c 4:"data"     $USB_DEV_DIR \
  || { exit_msg "data sgdisk failed";     exit; }
sgdisk -n 5:$cache_first:$cache_last             -c 5:"cache"    $USB_DEV_DIR \
  || { exit_msg "cache sgdisk failed";    exit; }
sgdisk -n 6:$recovery_first:$recovery_last       -c 6:"recovery" $USB_DEV_DIR \
  || { exit_msg "recovery sgdisk failed"; exit; }
sgdisk -n 7:$hwcfg_first:$hwcfg_last             -c 7:"hwcfg"    $USB_DEV_DIR \
  || { exit_msg "hwcfg sgdisk failed";    exit; }
sgdisk -n 8:$boot_first:$boot_last               -c 8:"boot"     $USB_DEV_DIR \
  || { exit_msg "boot sgdisk failed";     exit; }
sgdisk -n 9:$misc_first:$misc_last               -c 9:"misc"     $USB_DEV_DIR \
  || { exit_msg "misc sgdisk failed";     exit; }

mkfs.vfat -F 32 -n kernel   ${USB_DEV_DIR}1 || { exit_msg "kernel mkfs failed";   exit; }
mkfs.ext4 -L       rootfs   ${USB_DEV_DIR}2 || { exit_msg "rootfs mkfs failed";   exit; }
mkfs.ext4 -L       system   ${USB_DEV_DIR}3 || { exit_msg "system mkfs failed";   exit; }
mkfs.ext4 -L       data     ${USB_DEV_DIR}4 || { exit_msg "data mkfs failed";     exit; }
mkfs.ext4 -L       cache    ${USB_DEV_DIR}5 || { exit_msg "cache mkfs failed";    exit; }
mkfs.vfat -F 32 -n recovery ${USB_DEV_DIR}6 || { exit_msg "recovery mkfs failed"; exit; }

# It is important to check if the partition exists, because "dd"
# will create a file there if it does not, and create a persistent
# problem for the user.
if [ -e ${USB_DEV_DIR}7 ]
then
  # The old contents will still exist even though we re-created
  # the partition table. Wipe the partitions that will not get
  # overwritten by the Android image later.
  dd if=/dev/zero of=${USB_DEV_DIR}7 bs=$bs count=$hwcfg_size

  # Instantiate the hwcfg partition for those that will not
  # be using it, otherwise it will fail to mount and also
  # cause the internal memory (emulated SD card) to not mount.
  mkdir cramfstmp
  mkfs.cramfs -n hwcfg cramfstmp ${USB_DEV_DIR}7
  rmdir cramfstmp
else
  { exit_msg "hwcfg mkfs failed"; exit; }
fi

gdisk -l $USB_DEV_DIR
