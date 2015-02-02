#!/bin/bash

if [ "$(id -u)" != "0" ]; then
  echo "Script must be run with sudo."
  exit
fi

if [ $# -lt 1 ]
then
  echo "Usage: sudo ./flash_hwcfg_image.sh sd<b-z> [image_path]"
  echo "       <b-z>: Any letter from b to z, corresponding to your usb drive."
  echo "              'a' is not permitted because it is probably your main hard drive,"
  echo "              and this script first deletes existing partitions."
  echo "       [image_path]: Optional. Path to hwcfg.img if not located in current directory."
  echo "Example: sudo ./flash_hwcfg_image.sh sdf"
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

HWCFG_IMG=hwcfg.img

if [ $2 ]
then
  HWCFG_IMG=$2
fi

dd if=$HWCFG_IMG of=/dev/${1}7
