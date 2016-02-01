#!/bin/bash
set -e

function usage {
   echo "stage_target_bootimg.sh [-g] [-b] [-s] [-d] [-c] [-u] [-w <name>] [-z] [-S] [-p <name>] [-F <ip-address|usb>]"
   echo ""
   echo "list partitions to update. partitions in:"
   echo ""
   echo "     '-g': gpt"
   echo "     '-b': boot"
   echo "     '-s': system"
   echo "     '-d': userdata"
   echo "     '-c': cache"
   echo "     '-u': bsu"
   echo "     '-w <name>': hwcfg image name"
   echo "     '-z': recovery"
   echo ""
   echo "'-S' - to preserve labeling for SELinux support (incurs longer copy time)."
   echo ""
   echo "'-F' - to flash images through fastboot (run script from host), if specified with '-S';"
   echo "       '-S' is ignored since implicit with '-F'"
   echo ""
   echo "defaults to:"
   echo ""
   echo "     '-b -s -d' - if '-S' is not specified."
   echo "     '-S -b -s -d' - if '-S' specified without explicit targets."
   echo "     '-F <ip-address|usb> -b -s -d' - if '-F' specified without explicit targets."
   echo ""
   echo "'-p <name>' - to specify the root of the partition flashing"
   echo "              eg: sda, mmcblk0p - defaults to 'sda'."
   echo ""
}

partition_root=sda
fastboot_target=usb
fastboot_cmd=fastboot

update_boot_img=1
update_system=1
update_userdata=1
# not needed by default because we assume using USB to boot instead of eMMC
# but if we need to boot from eMMC, all the following partitions should be updated
update_cache=0
update_recovery=0
update_gpt=0
update_hwcfg=0
update_bsu=0
selinux=0
fastboot=0
if [ $# -gt 0 ]; then
	update_boot_img=0
	update_system=0
	update_userdata=0
fi

#flag to check if any of the $update_* variables are set
update_something=0
while getopts "hgbsdcuw:zSp:F:" tag; do
	case $tag in
	g)
		update_gpt=1
		update_something=1
		;;
	b)
		update_boot_img=1
		update_something=1
		;;
	s)
		update_system=1
		update_something=1
		;;
	d)
		update_userdata=1
		update_something=1
		;;
	c)
		update_cache=1
		update_something=1
		;;
	u)
		update_bsu=1
		update_something=1
		;;
	w)
		update_hwcfg=1
		update_something=1
		hwcfg_img=$OPTARG
		;;
	z)
		update_recovery=1
		update_something=1
		;;
	S)
		selinux=1
		;;
	p)
		partition_root=$OPTARG
		;;
	F)
		fastboot=1
		fastboot_target=$OPTARG
		if [[ $fastboot_target != "usb" ]]; then
			fastboot_cmd="./fastboot_tcp -t $fastboot_target"
		fi
		;;
	h|*)
		usage
		exit 1
		;;
	esac
done

# if both selinux and fastboot are together, nuke selinux since fastboot will actually achieve the same outcome.
if [[ $selinux -gt 0 && $fastboot -gt 0 ]]; then
	selinux=0
fi

# if only argument set was to ask for selinux/fastboot, roll back default update.
if [[ $fastboot -gt 0 && $update_something -eq 0 ]]; then
	update_boot_img=1
	update_system=1
	update_userdata=1
	update_something=1
fi
if [[ $selinux -gt 0 && $update_something -eq 0 ]]; then
	update_boot_img=1
	update_system=1
	update_userdata=1
	update_something=1
fi

if [[ $fastboot -eq 0 && $update_gpt -gt 0 ]]; then
	echo "Not using fastboot: gpt flashing requires a reboot"
	update_boot_img=0
	update_system=0
	update_userdata=0
	update_cache=0
	update_recovery=0
	update_hwcfg=0
	update_bsu=0
fi

# if nothing to do, warn and exit.
if [[ $update_something -eq 0 ]]; then
	echo ""
	echo "nothing to do?"
	echo ""
	usage
	exit 1
fi

echo ""
echo "partitions updated in this run:"
if [ $update_gpt -gt 0 ]; then
	echo "     gpt (partition table)"
fi
if [ $update_boot_img -gt 0 ]; then
	echo "     boot image"
fi
if [ $update_system -gt 0 ]; then
	echo "     system"
fi
if [ $update_userdata -gt 0 ]; then
	echo "     userdata"
fi
if [ $update_cache -gt 0 ]; then
	echo "     cache"
fi
if [ $update_recovery -gt 0 ]; then
	echo "     recovery"
fi
if [ $update_hwcfg -gt 0 ]; then
	echo "     hwcfg ($hwcfg_img)"
fi
if [ $update_bsu -gt 0 ]; then
	echo "     bsu"
fi
echo ""
if [ $selinux -gt 0 ]; then
	echo "setting up for selinux usage..."
	echo ""
fi
if [ $fastboot -gt 0 ]; then
	echo "using fastboot to target '$fastboot_target'..."
	echo ""
fi

sleep 2

# for fastboot, since we run from host, do the whole thing here, we do not need to
# do anything more other than push the images.
#
if [ $fastboot -gt 0 ]; then
	echo "running fastboot against $fastboot_target"
	# always update the gpt first if user wants to update gpt partition
	if [ $update_gpt -gt 0 ]; then
		$fastboot_cmd flash gpt ./gpt.bin
	fi

	# update the other paritions after gpt has been updated
	if [ $update_boot_img -gt 0 ]; then
		$fastboot_cmd flash boot ./boot.img
	fi
	if [ $update_hwcfg -gt 0 ]; then
		$fastboot_cmd flash hwcfg $hwcfg_img
	fi
	if [ $update_recovery -gt 0 ]; then
		$fastboot_cmd flash recovery ./recovery.img
	fi
	if [ $update_bsu -gt 0 ]; then
		$fastboot_cmd flash bsu ./loaders/android_bsu.elf
	fi
	if [ $update_system -gt 0 ]; then
		$fastboot_cmd flash system ./unpack_android_boot/system.img
	fi
	if [ $update_userdata -gt 0 ]; then
		$fastboot_cmd flash userdata ./unpack_android_boot/userdata.img
	fi
	if [ $update_cache -gt 0 ]; then
		$fastboot_cmd flash cache ./unpack_android_boot/cache.img
	fi
	$fastboot_cmd reboot
	echo "done!!!"
	exit 0
fi

if [ $selinux -eq 0 ]; then
	if [ $update_system -gt 0 ]; then
		mkdir -p /mnt/system
	fi
	if [ $update_userdata -gt 0 ]; then
		mkdir -p /mnt/data
	fi
	if [ $update_cache -gt 0 ]; then
		mkdir -p /mnt/cache
	fi
fi

# always update the gpt first if user wants to update gpt partition
if [ $update_gpt -gt 0 ]; then
	dd if=/mnt/work/gpt.bin of=/dev/${partition_root}
	# Tried: echo 1 > /sys/block/${partition_root}/device/rescan
	# Tried: mdev -s
	echo "Please reboot with the new partition table before flashing anything else."
	exit 1
fi

# Arguments:    $1=name
# Return value: stdout=index
# Usage:        myindex=$(get_partition_index mypartition)
function get_partition_index {
	index=$(fdisk -l /dev/sda | grep $1 | awk {'print $1'})
	if [ "$index" == "" ]; then
		echo "Unable to find partition $1" >&2
		exit 1
	fi
	echo $index
}

partition_boot=$(get_partition_index boot)
partition_system=$(get_partition_index system)
partition_userdata=$(get_partition_index userdata)
partition_cache=$(get_partition_index cache)
partition_recovery=$(get_partition_index recovery)
partition_hwcfg=$(get_partition_index hwcfg)
partition_bsu=$(get_partition_index bsu)

# update the other paritions after gpt has been updated
if [ $selinux -eq 0 ]; then
	echo "mounting partitions /dev/${partition_root}X to target system"
	if [ $update_system -gt 0 ]; then
		mount -t ext4 /dev/${partition_root}${partition_system} /mnt/system
	fi
	if [ $update_userdata -gt 0 ]; then
		mount -t ext4 /dev/${partition_root}${partition_userdata} /mnt/data
	fi
	if [ $update_cache -gt 0 ]; then
		mount -t ext4 /dev/${partition_root}${partition_cache} /mnt/cache
	fi
	if [ $update_system -gt 0 ]; then
		rm -rf /mnt/system/*
	fi
	if [ $update_userdata -gt 0 ]; then
		rm -rf /mnt/data/*
	fi
	if [ $update_cache -gt 0 ]; then
		rm -rf /mnt/cache/*
	fi
fi

cd /mnt/work/

if [ $update_boot_img -gt 0 ]; then
	echo "copying boot partition..."
	dd if=./boot.img of=/dev/${partition_root}${partition_boot} bs=4M
fi

if [ $update_system -gt 0 ]; then
	echo "copying system partition..."
	if [ $selinux -gt 0 ]; then
		dd if=./unpack_android_boot/system.raw.img of=/dev/${partition_root}${partition_system} bs=4M
	else
		mkdir -p ./unpack_android_boot/system
		mount -t ext4 -o loop ./unpack_android_boot/system.raw.img ./unpack_android_boot/system
		cp -a -p ./unpack_android_boot/system/* /mnt/system/
		umount ./unpack_android_boot/system
	fi
fi

if [ $update_userdata -gt 0 ]; then
	echo "copying userdata partition..."
	if [ $selinux -gt 0 ]; then
		dd if=./unpack_android_boot/userdata.raw.img of=/dev/${partition_root}${partition_userdata} bs=4M
	else
		mkdir -p ./unpack_android_boot/data
		mount -t ext4 -o loop ./unpack_android_boot/userdata.raw.img ./unpack_android_boot/data
		cp -a -p ./unpack_android_boot/data/* /mnt/data/
		umount ./unpack_android_boot/data
	fi
fi

if [ $update_cache -gt 0 ]; then
	echo "copying cache partition..."
	if [ $selinux -gt 0 ]; then
		dd if=./unpack_android_boot/cache.raw.img of=/dev/${partition_root}${partition_cache} bs=4M
	else
		mkdir -p ./unpack_android_boot/cache
		mount -t ext4 -o loop ./unpack_android_boot/cache.raw.img ./unpack_android_boot/cache
		cp -a -p ./unpack_android_boot/cache/* /mnt/cache/
		umount ./unpack_android_boot/cache
	fi
fi

if [ $update_recovery -gt 0 ]; then
	echo "copying recovery partition..."
	dd if=./recovery.img of=/dev/${partition_root}${partition_recovery} bs=4M
fi

if [ $update_hwcfg -gt 0 ]; then
	echo "copying hwcfg partition..."
	dd if=./$hwcfg_img of=/dev/${partition_root}${partition_hwcfg}
fi

if [ $update_bsu -gt 0 ]; then
	echo "copying bsu partition..."
	dd if=./loaders/android_bsu.elf of=/dev/${partition_root}${partition_bsu}
fi

echo "clean up..."
if [ $selinux -eq 0 ]; then
	if [ $update_system -gt 0 ]; then
		umount /mnt/system
	fi
	if [ $update_userdata -gt 0 ]; then
		umount /mnt/data
	fi
	if [ $update_cache -gt 0 ]; then
		umount /mnt/cache
	fi
fi

echo "done!!!"

