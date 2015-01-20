#!/bin/bash

if [ "$(id -u)" != "0" ]; then
  echo "Script must be run with sudo."
  exit
fi

if [ $# -lt 1 ]
then
  echo "Usage: sudo ./quick_unpack_android_multipart.sh [workspace_dir] [sd<b-z>]"
  echo "       workspace_dir : Top level of Android compilation workspace (ie, parent directory"
  echo "                       of kernel and out)"
  echo "       sd<b-z>       : Any letter from b to z, corresponding to your usb drive."
  echo "                       'a' is not permitted because it is probably your main hard drive,"
  echo "                       and this script first deletes existing partitions."
  echo "       NOTE: This script assumes that the USB drive was formatted by format_android_usb_multipart_16GB.sh"
  exit
fi

if [ $2 ]
then
  workspace_dir=$1
  usb_dir=$2
else
  if [ ${#1} -eq 3 ] # 1st argument is the USB drive
  then
    # auto-detect workspace_dir
    script_dir=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
    workspace_dir=${script_dir%vendor/broadcom/bcm_platform/tools/UnpackAndroid}
    usb_dir=$1
  else # 1st argument is workspace_dir
    workspace_dir=$1
  fi
fi

./unpack_android_multipart.sh $workspace_dir/out/target/product/bcm_platform /media/rootfs /media/system /media/data /media/cache /media/kernel $usb_dir
