#!/bin/bash

if [ "$(id -u)" != "0" ]; then
  echo "Script must be run with sudo."
  exit
fi

function usage {
  echo "Usage: sudo ./unpack_android_multipart.sh [-S] [-i <image_dir>] [-b <boot_dir>] [-r <rootfs_dir>] [-k <kernel_dir>] [-s <system_dir>] [-d <data_dir>] [-c <cache_dir>] [-z <recovery_dir>] -p sd<b-z>"
  echo "  '-S': Enable SELinux"
  echo "  '-i': Image directory containing boot.img, system.img and userdata.img."
  echo "        Ex. out/target/product/bcm_platform"
  echo "  '-b': boot.img output directory. Ex. /media/kernel)"
  echo "  '-r': rootfs output directory (ex. /media/rootfs)"
  echo "  '-k': kernel output directory (ex. /media/kernel)"
  echo "  '-s': system.img output directory (ex. /media/system)"
  echo "  '-d': userdata.img output directory (ex. /media/data)"
  echo "  '-c': cache.img output directory (ex. /media/cache)"
  echo "  '-z': recovery.img output directory (ex. /media/recovery)"
  echo "  '-p': sd<b-z> corresponding to your usb drive."
  echo "        'a' is not permitted because it is probably your main hard drive,"
  echo "        and this script first deletes existing partitions."
  echo ""
  echo "defaults if only '-S' and '-p sd<a-z>' specified:"
  echo "  '-S -b /media/kernel -s /media/system -d /media/data -c /media/cache -z /media/recovery -p sd<b-z>'"
  echo ""
  echo "defaults if only '-p sd<a-z>' specified:"
  echo "  '-r /media/rootfs -k /media/kernel -s /media/system -d /media/data -c /media/cache -z /media/recovery -p sd<b-z>'"
  echo ""
  echo "SELinux Example:     sudo ./unpack_android_multipart.sh -S -p sdf"
  echo "Non-SELinux Example: sudo ./unpack_android_multipart.sh -p sdf"
  exit
}

selinux=0

while getopts "hSi:p:b:s:d:c:k:r:z:" tag; do
  case $tag in
    S)
      selinux=1
      ;;
    i)
      image_dir=$OPTARG
      ;;
    p)
      usb_dev=$OPTARG
      ;;
    b)
      boot_dir=$OPTARG
      ;;
    r)
      rootfs_dir=$OPTARG
      ;;
    s)
      system_dir=$OPTARG
      ;;
    d)
      data_dir=$OPTARG
      ;;
    c)
      cache_dir=$OPTARG
      ;;
    k)
      kernel_dir=$OPTARG
      ;;
    z)
      recovery_dir=$OPTARG
      ;;
    h|*)
      usage
      exit 1
      ;;
    esac
done

red='\033[0;31m'
no_colour='\033[0m' # No Color

function cleanup {
  if [ $# -ne 0 ]; then
    echo -e "${red}!!!!!!!!!!!!!!!!!!!!!!! $1 !!!!!!!!!!!!!!!!!!!!!!!${no_colour}"
  fi

  # Unmount everything in case system has automount enabled
  umount ${usb_dir}1 &> /dev/null
  umount ${usb_dir}2 &> /dev/null
  umount ${usb_dir}3 &> /dev/null
  umount ${usb_dir}4 &> /dev/null
  umount ${usb_dir}5 &> /dev/null
  umount ${usb_dir}6 &> /dev/null
  umount ${usb_dir}7 &> /dev/null
  umount ${usb_dir}8 &> /dev/null

  # Cleanup in case last run was interrupted
  umount $boot_dir &> /dev/null
  umount $rootfs_dir &> /dev/null
  umount $kernel_dir &> /dev/null
  umount $system_dir &> /dev/null
  umount $data_dir   &> /dev/null
  umount $cache_dir  &> /dev/null
  umount $recovery_dir &> /dev/null
  rm -rf $boot_dir &> /dev/null
  rm -rf $rootfs_dir &> /dev/null
  rm -rf $kernel_dir &> /dev/null
  rm -rf $system_dir &> /dev/null
  rm -rf $data_dir   &> /dev/null
  rm -rf $cache_dir  &> /dev/null
  rm -rf $recovery_dir &> /dev/null
  umount ./unpack_android_raw_images/system &> /dev/null
  umount ./unpack_android_raw_images/data   &> /dev/null
  umount ./unpack_android_raw_images/cache  &> /dev/null
  rm -rf ./unpack_android_raw_images
  rm -rf ./unpack_android_boot
}

if [ -z $usb_dev ]; then
  { cleanup "USB drive must be specified, ex '-p sdf'"; exit; }
  exit
fi

if [ $selinux -eq 1 ]; then
  if [ -z "$boot_dir" ]; then
    boot_dir=/media/kernel
  fi
else
  if [ -z "$rootfs_dir" ]; then
    rootfs_dir=/media/rootfs
  fi
  if [ -z "$kernel_dir" ]; then
    kernel_dir=/media/kernel
  fi
fi

if [ -z "$system_dir" ]; then
  system_dir=/media/system
fi
if [ -z "$data_dir" ]; then
  data_dir=/media/data
fi
if [ -z "$cache_dir" ]; then
  cache_dir=/media/cache
fi
if [ -z "$recovery_dir" ]; then
  recovery_dir=/media/recovery
fi

if [ -z "$image_dir" ]; then
  # auto-detect workspace_dir
  script_dir=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
  workspace_dir=${script_dir%vendor/broadcom/bcm_platform/tools/UnpackAndroid}
  image_dir="${workspace_dir}out/target/product/bcm_platform"
fi

usb_dir=/dev/$usb_dev
if [ "$usb_dir" == "sda" ]; then
  echo "You have specified 'sda' which is likely your primary hard drive."
  echo "This script will DELETE and replace any exising partition. You "
  echo "must specify the location of your USB key that Android will be"
  echo "installed on. Exiting."
  exit
fi

echo "selinux:      $selinux"
echo "image_dir:    $image_dir"
echo "usb_dir:      $usb_dir"
echo "boot_dir:     $boot_dir"
echo "rootfs_dir:   $rootfs_dir"
echo "kernel_dir:   $kernel_dir"
echo "system_dir:   $system_dir"
echo "data_dir:     $data_dir"
echo "cache_dir:    $cache_dir"
echo "recovery_dir: $recovery_dir"

if [ ! -e $usb_dir ]; then
  { cleanup "$usb_dir does not exist"; exit; }
fi

cleanup

if [ $selinux -eq 0 ]; then
  # mount rootfs
  mkdir -p $rootfs_dir
  mount ${usb_dir}2 $rootfs_dir || { cleanup "rootfs mount failed"; exit; }
  # mount kernel
  mkdir -p $kernel_dir
  mount ${usb_dir}1 $kernel_dir || { cleanup "kernel mount failed"; exit; }
  rm $kernel_dir/kernel   &> /dev/null
  rm $kernel_dir/boot.img &> /dev/null
  mkdir -p $system_dir
  mount ${usb_dir}3 $system_dir || { cleanup "system mount failed"; exit; }
  # mount data
  mkdir -p $data_dir
  mount ${usb_dir}4 $data_dir || { cleanup "data mount failed";   exit; }
  # mount cache
  mkdir -p $cache_dir
  mount ${usb_dir}5 $cache_dir || { cleanup "cache mount failed";  exit; }
  rm -rf $cache_dir/*
else
  # mount boot
  mkdir -p $boot_dir
  mount ${usb_dir}1 $boot_dir || { cleanup "boot mount failed"; exit; }
  rm $kernel_dir/kernel   &> /dev/null
  rm $kernel_dir/boot.img &> /dev/null
fi

#mount recovery
mkdir -p $recovery_dir
mount ${usb_dir}5 $recovery_dir || { cleanup "recovery mount failed";  exit; }

mkdir ./unpack_android_raw_images
./tools/simg2img $image_dir/system.img ./unpack_android_raw_images/system.raw.img
./tools/simg2img $image_dir/userdata.img ./unpack_android_raw_images/userdata.raw.img
./tools/simg2img $image_dir/cache.img ./unpack_android_raw_images/cache.raw.img

if [ $selinux -eq 0 ]; then
  echo "Extracting boot.img"
  ./tools/split_boot $image_dir/boot.img
  echo "Copying kernel.img"
  cp unpack_android_boot/boot.img-kernel $kernel_dir/kernel || { cleanup "kernel copy failed";  exit; }
  echo "Copying recovery.img"
  cp $image_dir/recovery.img $recovery_dir || { cleanup "recovery.img copy failed";  exit; }

  mkdir ./unpack_android_raw_images/system
  mkdir ./unpack_android_raw_images/data
  mkdir ./unpack_android_raw_images/cache

  mount -t ext4 -o loop ./unpack_android_raw_images/system.raw.img ./unpack_android_raw_images/system || { cleanup "raw system mount failed";  exit; }
  mount -t ext4 -o loop ./unpack_android_raw_images/userdata.raw.img ./unpack_android_raw_images/data || { cleanup "raw data mount failed";  exit; }
  mount -t ext4 -o loop ./unpack_android_raw_images/cache.raw.img ./unpack_android_raw_images/cache || { cleanup "raw cache mount failed";  exit; }

  echo "Copying rootfs"
  rsync -av --delete ./unpack_android_boot/ramdisk/ $rootfs_dir
  echo "Copying system"
  rsync -av --delete ./unpack_android_raw_images/system/ $system_dir
  echo "Copying data"
  rsync -av --delete ./unpack_android_raw_images/data/ $data_dir
  echo "Copying cache"
  rsync -av --delete ./unpack_android_raw_images/cache/ $cache_dir

  # Cleanup
  echo "Cleaning up"
  umount ./unpack_android_raw_images/system
  umount ./unpack_android_raw_images/data
  umount ./unpack_android_raw_images/cache
  rm -rf ./unpack_android_boot
else
  echo "Copying boot.img"
  cp $image_dir/boot.img $boot_dir || { cleanup "boot.img copy failed";  exit; }
  echo "Copying recovery.img"
  cp $image_dir/recovery.img $recovery_dir || { cleanup "recovery.img copy failed";  exit; }

  echo "Flashing system.img"
  if [ -e ${usb_dir}3 ]; then
    dd if=./unpack_android_raw_images/system.raw.img of=${usb_dir}3 bs=4M
  else
    { cleanup "system partition does not exist"; exit; }
  fi

  echo "Flashing userdata.img"
  if [ -e ${usb_dir}4 ]; then
    dd if=./unpack_android_raw_images/userdata.raw.img of=${usb_dir}4 bs=4M
  else
    { cleanup "data partition does not exist"; exit; }
  fi

  echo "Flashing cache.img"
  if [ -e ${usb_dir}5 ]; then
    dd if=./unpack_android_raw_images/cache.raw.img of=${usb_dir}5 bs=4M
  else
    { cleanup "cache partition does not exist"; exit; }
  fi

  echo "Cleaning up"
fi

# Cleanup
rm -rf ./unpack_android_raw_images
cleanup
