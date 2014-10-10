#!/bin/bash

if [ $# -lt 1 ]
then
  echo "Usage: sudo ./quick_unpack.sh <workspace_dir> [sd<b-z>]"
  echo "       workspace_dir : Top level of Android compilation workspace (ie, parent directory"
  echo "                       of kernel and out)"
  echo "       sd<b-z>       : Any letter from b to z, corresponding to your usb drive."
  echo "                       'a' is not permitted because it is probably your main hard drive,"
  echo "                       and this script first deletes existing partitions."
  echo "       NOTE: This script assumes that the USB drive was formatted by format_android_usb_multipart_8GB.sh"
  exit
fi

if [ $2 ]
then
  ./unpack_android_multipart.sh $1/out/target/product/bcm_platform /media/rootfs /media/system /media/data /media/cache /media/kernel $2
else
  ./unpack_android_multipart.sh $1/out/target/product/bcm_platform /media/rootfs /media/system /media/data /media/cache /media/kernel
fi
