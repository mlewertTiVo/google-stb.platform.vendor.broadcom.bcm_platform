#!/bin/bash

if [ $# -lt 3 ]
  then
    echo "Usage: unpack_android.sh <tools_dir> <image_dir> <out_dir>"
    echo "       tools_dir: directory containing the UnpackAndroid tools"
    echo "       image_dir: directory containing boot.img, system.img and userdata.img. Ex. out/target/product/bcm_platform"
    echo "       out_dir: EMPTY output directory. Ex. /media/rootfs"
    exit
fi

if [ -e "$3/tools" ] || [ -e "$3/boot" ]
  then
    echo ""
    echo "!!! ERROR !!!"
    echo " Either a file or folder named \"tools\" or \"boot\" already exists in $3"
    echo " Please remove those in $3 manually if possible, or provide another out_dir."
    echo ""
    exit
fi

cp -rf $1 $3/tools
cd $3

./tools/split_boot $2/boot.img
./tools/simg2img $2/system.img ./boot/system.raw.img
./tools/simg2img $2/userdata.img ./boot/userdata.raw.img
mount -t ext4 -o loop ./boot/system.raw.img ./boot/ramdisk/system
mount -t ext4 -o loop ./boot/userdata.raw.img ./boot/ramdisk/data

rsync -av ./boot/ramdisk/* $3

# Cleanup
umount ./boot/ramdisk/system
umount ./boot/ramdisk/data
rm -rf ./boot
rm -rf ./tools
