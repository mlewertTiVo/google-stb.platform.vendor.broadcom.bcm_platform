#!/bin/bash
set -e

function usage {
   echo "stage_host_bootimg.sh [[b] [s] [d] [c] [z]] [S]"
   echo ""
   echo "list partitions to update. partitions in:"
   echo "     'b': boot image"
   echo "     's': system image"
   echo "     'd': userdata image"
   echo "     'c': cache image"
   echo "     'z': recovery image"
   echo "     'S': for usage with selinux enabled"
   echo ""
   echo "to run with SELinux support, specify 'S'"
   echo ""
   echo "defaults to:"
   echo "     if without [S]: 'b s d'"
   echo "     if [S] alone, equivalent to: 'S b s d'"
   echo ""
}

# those partition are coming from the format script.
partition_boot=1
partition_system=3
partition_data=4
partition_cache=5
partition_recovery=6

update_boot_img=1
update_system=1
update_data=1
# not needed by default...
update_cache=0
update_recovery=0
selinux=0
if [ $# -gt 0 ]; then
	echo "reset arguments, reading from command line..."
	update_boot_img=0
	update_system=0
	update_data=0
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
	z)
	update_recovery=1
	;;
	S)
	selinux=1
	;;
	*)
	usage
	exit 1
	;;
esac
done

# if only argument set was to ask for selinux, roll back default update.
if [[ $selinux -gt 0 && $update_boot_img -eq 0 && $update_system -eq 0 && $update_data -eq 0 && $update_cache -eq 0 && $update_recovery -eq 0 ]]; then
	update_boot_img=1
	update_system=1
	update_data=1
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

echo ""
echo "******** IMPORTANT ********"
echo " expected partition layout numbering is: "
echo ""
echo "   boot:     ${partition_boot}"
echo "   system:   ${partition_system}"
echo "   data:     ${partition_data}"
echo "   cache:    ${partition_cache}"
echo "   recovery: ${partition_recovery}"
echo ""
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

echo "Mounting needed partitions in the USB stick to target system"
if [ $update_boot_img -gt 0 ]; then
	mount /dev/sda${partition_boot} /mnt/kernel
fi
if [ $update_recovery -gt 0 ]; then
	mount /dev/sda${partition_recovery} /mnt/recovery
fi
if [ $selinux -eq 0 ]; then
	if [ $update_system -gt 0 ]; then
		mount /dev/sda${partition_system} /mnt/system
	fi
	if [ $update_data -gt 0 ]; then
		mount /dev/sda${partition_data} /mnt/data
	fi
	if [ $update_cache -gt 0 ]; then
		mount /dev/sda${partition_cache} /mnt/cache
	fi
	echo "Cleaning up all partitions (except for the kernel/recovery partitions)"
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
echo "Copying boot.img..."
cp ./boot.img /mnt/kernel/
fi

if [ $update_system -gt 0 ]; then
	echo "Copying system partition..."
	if [ $selinux -gt 0 ]; then
		dd if=./boot/system.raw.img of=/dev/sda${partition_system}
	else
		mkdir -p ./boot/system
		mount -t ext4 -o loop ./boot/system.raw.img ./boot/system
		cp -a -p ./boot/system/* /mnt/system/
		umount ./boot/system
	fi
fi

if [ $update_data -gt 0 ]; then
	echo "Copying data partition..."
	if [ $selinux -gt 0 ]; then
		dd if=./boot/userdata.raw.img of=/dev/sda${partition_data}
	else
		mkdir -p ./boot/data
		mount -t ext4 -o loop ./boot/userdata.raw.img ./boot/data
		cp -a -p ./boot/data/* /mnt/data/
		umount ./boot/data
	fi
fi

if [ $update_cache -gt 0 ]; then
	echo "Copying cache partition..."
	if [ $selinux -gt 0 ]; then
		dd if=./boot/cache.raw.img of=/dev/sda${partition_cache}
	else
		mkdir -p ./boot/cache
		mount -t ext4 -o loop ./boot/cache.raw.img ./boot/cache
		cp -a --preserve=context ./boot/cache/* /mnt/cache/
		umount ./boot/cache
	fi
fi

if [ $update_recovery -gt 0 ]; then
echo "Copying recovery.img (in recovery partition and kernel partition)..."
cp ./recovery.img /mnt/recovery/
cp ./recovery.img /mnt/kernel/
fi

echo "Cleaning up..."
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

echo "Done!!!"

