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
echo "   ... removing the boot folder"
rm -rf $2/boot

echo "2) Copying kernel"
cp $1/kernel $2

echo "3) Unpacking ramdisk"
cd $2
$tools_dir/tools/split_boot $1/boot.img
$tools_dir/tools/simg2img $1/system.img ./boot/system.raw.img
$tools_dir/tools/simg2img $1/userdata.img ./boot/userdata.raw.img

echo "4) Copying stage_target.sh and stage_target_multipart.sh"
cp $tools_dir/stage_target.sh $2
cp $tools_dir/stage_target_multipart.sh $2

echo "!!! Done staging !!!" 
