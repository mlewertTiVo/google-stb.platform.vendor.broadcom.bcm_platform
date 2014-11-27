#!/bin/bash
set -e

function usage {
   echo "stage_host_bootimg.sh [[b] [s] [d] [c]]"
   echo "list partitions to update. partitions in:"
   echo "     'b': boot image"
   echo "     's': system image"
   echo "     'd': userdata image"
   echo "     'c': cache image"
   echo "defaults to: 'b s d c'"
}

update_boot_img=1
update_system=1
update_data=1
# 'cache' is not hooked up fully...
update_cache=0
if [ $# -gt 0 ]; then
	echo "reset arguments, reading from command line..."
	update_boot_img=0
	update_system=0
	update_data=0
	update_cache=0
fi

while [[ $# > 0 ]]
	do
	tag="$1"
	shift

	case $tag in
	b)
	update_boot_img=1
	;;
	s)
	update_system=1
	;;
	d)
	update_data=1
	;;
	c)
	update_cache=1
	;;
	*)
	usage
	exit 1
	;;
esac
done

echo ""
echo "  Following partitions will be updated in this run:"
if [ $update_boot_img -gt 0 ]; then
echo "     boot image"
fi
if [ $update_system -gt 0 ]; then
echo "     system"
fi
if [ $update_data -gt 0 ]; then
echo "     data (userdata)"
fi
if [ $update_cache -gt 0 ]; then
echo "     cache"
fi
echo ""

echo ""
echo "******** IMPORTANT ********"
echo "  This script assumes that the USB drive was formatted by format_android_usb_multipart_8GB.sh"
echo ""
sleep 2

if [ $update_boot_img -gt 0 ]; then
mkdir -p /mnt/kernel
fi
if [ $update_system -gt 0 ]; then
mkdir -p /mnt/system
fi
if [ $update_data -gt 0 ]; then
mkdir -p /mnt/data
fi
if [ $update_cache -gt 0 ]; then
mkdir -p /mnt/cache
fi

echo "Mounting all paritions in the USB stick to target system"
if [ $update_boot_img -gt 0 ]; then
mount /dev/sda1 /mnt/kernel
fi
if [ $update_system -gt 0 ]; then
mount /dev/sda3 /mnt/system
fi
if [ $update_data -gt 0 ]; then
mount /dev/sda4 /mnt/data
fi
if [ $update_cache -gt 0 ]; then
mount /dev/sda5 /mnt/cache
fi

echo "Cleaning up all paritions (except for the kernel parition)"
if [ $update_system -gt 0 ]; then
rm -rf /mnt/system/*
fi
if [ $update_data -gt 0 ]; then
rm -rf /mnt/data/*
fi
if [ $update_cache -gt 0 ]; then
rm -rf /mnt/cache/*
fi

cd /mnt/work/

if [ $update_boot_img -gt 0 ]; then
echo "Copying boot.img..."
cp ./boot.img /mnt/kernel/
fi

if [ $update_system -gt 0 ]; then
echo "Copying system partition over..."
mkdir -p ./boot/system
mount -t ext4 -o loop ./boot/system.raw.img ./boot/system
cp -a ./boot/system/* /mnt/system/
umount ./boot/system
fi

if [ $update_data -gt 0 ]; then
echo "Copying data partition over..."
mkdir -p ./boot/data
mount -t ext4 -o loop ./boot/userdata.raw.img ./boot/data
cp -a ./boot/data/* /mnt/data/
umount ./boot/data
fi

echo "Cleaning up..."
if [ $update_boot_img -gt 0 ]; then
umount /mnt/kernel
fi
if [ $update_system -gt 0 ]; then
umount /mnt/system
fi
if [ $update_data -gt 0 ]; then
umount /mnt/data
fi
if [ $update_cache -gt 0 ]; then
umount /mnt/cache
fi

echo "Done!!!"

