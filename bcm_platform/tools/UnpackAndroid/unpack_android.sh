#!/bin/bash

tools_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ $# -lt 3 ]
  then
    echo ""
    echo "Usage: unpack_android.sh <image_dir> <rootfs_dir> <kernel_dir>"
    echo "       image_dir : Absolute path to the directory containing boot.img, system.img and userdata.img."
    echo "                   Ex. <workspace>/out/target/product/bcm_platform"
    echo "       rootfs_dir: Absolute path to an EMPTY output directory where the rootfs will be unpacked to."
    echo "                   Ex. /media/rootfs"
    echo "       kernel_dir: Absolute path to an output directory where the kernel image will be copied to."
    echo "                   Ex. /media/kernel"
    echo ""
    exit
fi

if [ -e "$2/boot" ]
  then
    echo ""
    echo "!!! ERROR !!!"
    echo " Either a file or folder named \"boot\" already exists in $2"
    echo " Please remove those in $2 manually if possible, or provide another rootfs_dir."
    echo ""
    exit
fi

cp $1/kernel $3

cd $2
$tools_dir/tools/split_boot $1/boot.img
$tools_dir/tools/simg2img $1/system.img ./boot/system.raw.img
$tools_dir/tools/simg2img $1/userdata.img ./boot/userdata.raw.img
mount -t ext4 -o loop ./boot/system.raw.img ./boot/ramdisk/system
mount -t ext4 -o loop ./boot/userdata.raw.img ./boot/ramdisk/data

rsync -av ./boot/ramdisk/* $2

# Cleanup
umount ./boot/ramdisk/system
umount ./boot/ramdisk/data
rm -rf ./boot
