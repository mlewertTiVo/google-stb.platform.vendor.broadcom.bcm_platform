#!/bin/bash

if [ $# -lt 3 ]
  then
    echo "Usage: stage_host.sh <tools_dir> <image_dir> <staging_dir>"
    echo "       tools_dir: directory containing the UnpackAndroid tools"
    echo "       image_dir: directory containing boot.img, system.img and userdata.img. Ex. out/target/product/bcm_platform"
    echo "       staging_dir: directory where the filesystem will be staged to on your Linux machine; make sure that this directory has NFS access"
    exit
fi

if [ -e "$3/tools" ]
  then
    echo ""
    echo "!!! ERROR !!!"
    echo " A file or folder named \"tools\" already exists in $3"
    echo " Please remove it in $3 manually if possible, or provide another staging_dir."
    echo ""
    exit
fi

echo "1) Cleaning up staging dir: $3"
echo "   ... removing the tools folder"
rm -rf $3/tools
echo "   ... removing the boot folder"
rm -rf $3/boot

echo "2) Copying UnpackAndroid tools"
cp -rf $1 $3/tools

echo "3) Copying stage_target.sh"
cp ./stage_target.sh $3

echo "4) Copying kernel"
cp $2/kernel $3

echo "5) Unpacking ramdisk"
cd $3
./tools/split_boot $2/boot.img
./tools/simg2img $2/system.img ./boot/system.raw.img
./tools/simg2img $2/userdata.img ./boot/userdata.raw.img

rm -rf ./tools

echo "!!! Done staging !!!" 
