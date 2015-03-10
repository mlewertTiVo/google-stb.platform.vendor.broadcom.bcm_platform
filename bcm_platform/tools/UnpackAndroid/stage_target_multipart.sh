#!/bin/bash
set -e

function usage {
   echo "stage_host_multipart.sh [[k] [r] [s] [d] [c] [z]]"
   echo "list partitions to update. partitions in:"
   echo "     'k': kernel image"
   echo "     'r': ramdisk image (ie rootfs)"
   echo "     's': system image"
   echo "     'd': userdata image"
   echo "     'c': cache image"
   echo "     'z': recovery image"
   echo "defaults to: 'k r s d'"
}

update_kernel_img=1
update_ramdisk=1
update_system=1
update_data=1
# not needed by default...
update_cache=0
update_recovery=0
if [ $# -gt 0 ]; then
	echo "reset arguments, reading from command line..."
	update_kernel_img=0
	update_ramdisk=0
	update_system=0
	update_data=0
	update_cache=0
	update_recovery=0
fi

while [[ $# > 0 ]]
	do
	tag="$1"
	shift

	case $tag in
	k)
	update_kernel_img=1
	;;
	r)
	update_ramdisk=1
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
	z)
	update_recovery=1
	;;
	*)
	usage
	exit 1
	;;
esac
done

echo ""
echo "  Following partitions will be updated in this run:"
if [ $update_kernel_img -gt 0 ]; then
echo "     kernel image"
fi
if [ $update_ramdisk -gt 0 ]; then
echo "     ramdisk (rootfs)"
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
if [ $update_recovery -gt 0 ]; then
echo "     recovery"
fi
echo ""

echo ""
echo "******** IMPORTANT ********"
echo "  This script assumes that the USB drive was formatted by format_android_usb_multipart_16GB.sh"
echo ""
sleep 2

if [ $update_kernel_img -gt 0 ]; then
mkdir -p /mnt/kernel
fi
if [ $update_ramdisk -gt 0 ]; then
mkdir -p /mnt/rootfs
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
if [ $update_recovery -gt 0 ]; then
mkdir -p /mnt/recovery
fi

echo "Mounting all partitions in the USB stick to target system"
if [ $update_kernel_img -gt 0 ]; then
mount /dev/sda1 /mnt/kernel
fi
if [ $update_ramdisk -gt 0 ]; then
mount /dev/sda2 /mnt/rootfs
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
if [ $update_recovery -gt 0 ]; then
mount /dev/sda6 /mnt/recovery
fi

echo "Cleaning up all partitions (except for the kernel/recovery partitions)"
if [ $update_ramdisk -gt 0 ]; then
rm -rf /mnt/rootfs/*
fi
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

if [ $update_kernel_img -gt 0 ]; then
echo "Copying kernel..."
cp ./kernel /mnt/kernel/
fi

if [ $update_ramdisk -gt 0 ]; then
echo "Copying (exploded) rootfs..."
cp -a ./unpack_android_boot/ramdisk/* /mnt/rootfs/
fi

if [ $update_system -gt 0 ]; then
echo "Copying system partition..."
mkdir -p ./unpack_android_boot/system
mount -t ext4 -o loop ./unpack_android_boot/system.raw.img ./unpack_android_boot/system
cp -a ./unpack_android_boot/system/* /mnt/system/
umount ./unpack_android_boot/system
fi

if [ $update_data -gt 0 ]; then
echo "Copying data partition..."
mkdir -p ./unpack_android_boot/data
mount -t ext4 -o loop ./unpack_android_boot/userdata.raw.img ./unpack_android_boot/data
cp -a ./unpack_android_boot/data/* /mnt/data/
umount ./unpack_android_boot/data
fi

if [ $update_cache -gt 0 ]; then
echo "Copying cache partition..."
mkdir -p ./unpack_android_boot/cache
mount -t ext4 -o loop ./unpack_android_boot/cache.raw.img ./unpack_android_boot/cache
cp -a ./unpack_android_boot/cache/* /mnt/cache/
umount ./unpack_android_boot/cache
fi

if [ $update_recovery -gt 0 ]; then
echo "Copying recovery image..."
cp ./recovery.img /mnt/recovery/
fi

echo "Cleaning up..."
if [ $update_kernel_img -gt 0 ]; then
umount /mnt/kernel
fi
if [ $update_ramdisk -gt 0 ]; then
umount /mnt/rootfs
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
if [ $update_recovery -gt 0 ]; then
umount /mnt/recovery
fi

echo "Done!!!"

