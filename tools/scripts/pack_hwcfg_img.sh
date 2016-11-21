#!/bin/bash

if [ $# -lt 1 ]
then
  echo "Usage: ./pack_hwcfg_img.sh <path>"
  echo "       <path>: Root directory to pack into hwcfg image"
  exit
fi

mkfs.cramfs $1 hwcfg.img
