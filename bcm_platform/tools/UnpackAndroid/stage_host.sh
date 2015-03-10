#!/bin/bash

tools_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ $# -lt 2 ]
then
  echo ""
  echo "Usage: stage_host.sh <image_dir> <staging_dir>"
  echo "           image_dir  : Path to the directory containing boot.img, system.img and userdata.img."
  echo "                        Ex. <workspace>/out/target/product/bcm_platform"
  echo "           staging_dir: Path to a directory where the filesystem will be staged to in"
  echo "                        your Linux machine. Make sure that this directory has NFS access."
  echo ""
  exit
fi

image_dir="$( cd $1 && pwd )"
staging_dir="$( cd $2 && pwd )"


echo "1) Cleaning up staging dir: $staging_dir"
echo "   ... removing the boot+bootloaders folder"
rm -rf $staging_dir/boot
rm -rf $staging_dir/loaders

echo "2) Copying: boot.img, recovery.img; Unpacking ramdisk..."
cd $staging_dir
$tools_dir/tools/split_boot $image_dir/boot.img
mv unpack_android_boot/boot.img-kernel kernel
cp $image_dir/boot.img boot.img
cp $image_dir/recovery.img recovery.img

echo "3) Copying: system images; Unsparse ext4's"
$tools_dir/tools/simg2img $image_dir/system.img ./unpack_android_boot/system.raw.img
cp $image_dir/system.img ./unpack_android_boot/system.img
$tools_dir/tools/simg2img $image_dir/userdata.img ./unpack_android_boot/userdata.raw.img
cp $image_dir/userdata.img ./unpack_android_boot/userdata.img
$tools_dir/tools/simg2img $image_dir/cache.img ./unpack_android_boot/cache.raw.img
cp $image_dir/cache.img ./unpack_android_boot/cache.img

echo "4) Copying stage_target* scripts, and tools."
cp $tools_dir/stage_target.sh $staging_dir
cp $tools_dir/stage_target_multipart.sh $staging_dir
cp $tools_dir/stage_target_bootimg.sh $staging_dir
cp $tools_dir/tools/fastboot_tcp $staging_dir

echo "5) Copying bootloaders."
mkdir -p $staging_dir/loaders
cp $image_dir/android_bsu.elf $staging_dir/loaders
cp $image_dir/bolt-*.bin $staging_dir/loaders

echo "!!! Done staging !!!"
