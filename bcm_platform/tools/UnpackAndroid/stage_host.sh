#!/bin/bash

tools_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ $# -lt 2 ]
then
  echo ""
  echo "Usage: stage_host.sh <image_dir> <staging_dir>"
  echo "           image_dir  : Absolute path to the directory containing boot.img, system.img and userdata.img."
  echo "                        Ex. <workspace>/out/target/product/bcm_platform"
  echo "           staging_dir: Absolute path to a directory where the filesystem will be staged to in"
  echo "                        your Linux machine. Make sure that this directory has NFS access."
  echo ""
  exit
fi

echo "1) Cleaning up staging dir: $2"
echo "   ... removing the boot+bootloaders folder"
rm -rf $2/boot
rm -rf $2/loaders

echo "2) Unpacking ramdisk and copy kernel"
cd $2
$tools_dir/tools/split_boot $1/boot.img
mv boot/boot.img-kernel kernel
cp $1/boot.img boot.img

echo "3) Unsparse ext4's"
$tools_dir/tools/simg2img $1/system.img ./boot/system.raw.img
$tools_dir/tools/simg2img $1/userdata.img ./boot/userdata.raw.img

echo "4) Copying stage_target* scripts."
cp $tools_dir/stage_target.sh $2
cp $tools_dir/stage_target_multipart.sh $2
cp $tools_dir/stage_target_bootimg.sh $2

echo "5) Copying bootloaders."
mkdir -p $2/loaders
cp $1/android_bsu.elf $2/loaders
cp $1/bolt-ba.bin $2/loaders
cp $1/bolt-bb.bin $2/loaders

echo "!!! Done staging !!!" 
