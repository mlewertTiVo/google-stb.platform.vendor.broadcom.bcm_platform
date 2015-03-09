#!/bin/bash
set -e

function usage {
   echo "stage_host_bootimg.sh [-b] [-s] [-d] [-c] [-z] [-S] [-p <name>]"
   echo ""
   echo "list partitions to update. partitions in:"
   echo ""
   echo "     '-b': boot"
   echo "     '-s': system"
   echo "     '-d': userdata"
   echo "     '-c': cache"
   echo "     '-z': recovery"
   echo ""
   echo "'-S' - to run with SELinux support (incurs longer copy time)."
   echo ""
   echo "defaults to:"
   echo ""
   echo "     '-b -s -d' - if '-S' is not specified."
   echo "     '-S -b -s -d' - if '-S' specified without explicit partitions."
   echo ""
   echo "'-p <name>' - to specify the root of the partition flashing"
   echo "              eg: sda, mmcblk0p - defaults to 'sda'."
   echo ""
}

# those partition are coming from the format script (usb) and/or the gpt (emmc).
partition_boot=1
partition_system=3
partition_data=4
partition_cache=5
partition_recovery=6

partition_root=sda

update_boot_img=1
update_system=1
update_data=1
# not needed by default...
update_cache=0
update_recovery=0
selinux=0
if [ $# -gt 0 ]; then
	update_boot_img=0
	update_system=0
	update_data=0
fi

while getopts "hbsdczSp:" tag; do
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
	z)
		update_recovery=1
		;;
	S)
		selinux=1
		;;
	p)
		partition_root=$OPTARG
		;;
	h|*)
		usage
		exit 1
		;;
	esac
done

echo ""
echo "******** WARNING ********"
echo ""
echo " expected partition layout is: "
echo ""
echo "   boot:     /dev/${partition_root}${partition_boot}"
echo "   system:   /dev/${partition_root}${partition_system}"
echo "   data:     /dev/${partition_root}${partition_data}"
echo "   cache:    /dev/${partition_root}${partition_cache}"
echo "   recovery: /dev/${partition_root}${partition_recovery}"
echo ""
echo "******** WARNING ********"

# if only argument set was to ask for selinux, roll back default update.
if [[ $selinux -gt 0 && $update_boot_img -eq 0 && $update_system -eq 0 && $update_data -eq 0 && $update_cache -eq 0 && $update_recovery -eq 0 ]]; then
	update_boot_img=1
	update_system=1
	update_data=1
fi

# if nothing to do, warn and exit.
if [[ $update_boot_img -eq 0 && $update_system -eq 0 && $update_data -eq 0 && $update_cache -eq 0 && $update_recovery -eq 0 ]]; then
	echo ""
	echo "nothing to do?"
	echo ""
	usage
	exit 1
fi

echo ""
echo "partitions updated in this run:"
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
if [ $update_recovery -gt 0 ]; then
echo "     recovery"
fi
echo ""
if [ $selinux -gt 0 ]; then
echo "setting up for selinux usage..."
echo ""
fi

sleep 2

if [ $update_recovery -gt 0 ]; then
	# cheat-sheet: make sure kernel is mounted as well...
	update_boot_img=1
fi

if [ $update_boot_img -gt 0 ]; then
	mkdir -p /mnt/kernel
fi
if [ $update_recovery -gt 0 ]; then
	mkdir -p /mnt/recovery
fi
if [ $selinux -eq 0 ]; then
	if [ $update_system -gt 0 ]; then
		mkdir -p /mnt/system
	fi
	if [ $update_data -gt 0 ]; then
		mkdir -p /mnt/data
	fi
	if [ $update_cache -gt 0 ]; then
		mkdir -p /mnt/cache
	fi
fi

echo "mounting partitions /dev/${partition_root}X to target system"
if [ $update_boot_img -gt 0 ]; then
	mount /dev/${partition_root}${partition_boot} /mnt/kernel
fi
if [ $update_recovery -gt 0 ]; then
	mount /dev/${partition_root}${partition_recovery} /mnt/recovery
fi
if [ $selinux -eq 0 ]; then
	if [ $update_system -gt 0 ]; then
		mount /dev/${partition_root}${partition_system} /mnt/system
	fi
	if [ $update_data -gt 0 ]; then
		mount /dev/${partition_root}${partition_data} /mnt/data
	fi
	if [ $update_cache -gt 0 ]; then
		mount /dev/${partition_root}${partition_cache} /mnt/cache
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
fi

cd /mnt/work/

if [ $update_boot_img -gt 0 ]; then
	echo "copying boot.img..."
	cp ./boot.img /mnt/kernel/
fi

if [ $update_system -gt 0 ]; then
	echo "copying system partition..."
	if [ $selinux -gt 0 ]; then
		dd if=./boot/system.raw.img of=/dev/${partition_root}${partition_system} bs=4M
	else
		mkdir -p ./boot/system
		mount -t ext4 -o loop ./boot/system.raw.img ./boot/system
		cp -a -p ./boot/system/* /mnt/system/
		umount ./boot/system
	fi
fi

if [ $update_data -gt 0 ]; then
	echo "copying data partition..."
	if [ $selinux -gt 0 ]; then
		dd if=./boot/userdata.raw.img of=/dev/${partition_root}${partition_data} bs=4M
	else
		mkdir -p ./boot/data
		mount -t ext4 -o loop ./boot/userdata.raw.img ./boot/data
		cp -a -p ./boot/data/* /mnt/data/
		umount ./boot/data
	fi
fi

if [ $update_cache -gt 0 ]; then
	echo "copying cache partition..."
	if [ $selinux -gt 0 ]; then
		dd if=./boot/cache.raw.img of=/dev/${partition_root}${partition_cache} bs=4M
	else
		mkdir -p ./boot/cache
		mount -t ext4 -o loop ./boot/cache.raw.img ./boot/cache
		cp -a -p ./boot/cache/* /mnt/cache/
		umount ./boot/cache
	fi
fi

if [ $update_recovery -gt 0 ]; then
	echo "copying recovery.img (in recovery partition and kernel partition)..."
	cp ./recovery.img /mnt/recovery/
	cp ./recovery.img /mnt/kernel/
fi

echo "clean up..."
if [ $update_boot_img -gt 0 ]; then
	umount /mnt/kernel
fi
if [ $update_recovery -gt 0 ]; then
	umount /mnt/recovery
fi
if [ $selinux -eq 0 ]; then
	if [ $update_system -gt 0 ]; then
		umount /mnt/system
	fi
	if [ $update_data -gt 0 ]; then
		umount /mnt/data
	fi
	if [ $update_cache -gt 0 ]; then
		umount /mnt/cache
	fi
fi

echo "done!!!"

