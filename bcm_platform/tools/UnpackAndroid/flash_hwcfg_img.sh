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

HWCFG_IMG=hwcfg.img

if [ $2 ]
then
  HWCFG_IMG=$2
fi

# It is important to check if the partition exists, because "dd"
# will create a file there if it does not, and create a persistent
# problem for the user.
if [ -e ${USB_DEV_DIR}7 ]
then
  dd if=$HWCFG_IMG of=${USB_DEV_DIR}7
else
  { exit_msg "hwcfg partition ${USB_DEV_DIR}7 does not exist"; exit; }
fi
