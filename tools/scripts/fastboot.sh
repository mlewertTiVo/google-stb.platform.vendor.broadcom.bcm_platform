#!/bin/bash
set -e

function usage {
   echo "$0 [-g] [-b] [-s] [-d] [-c] [-u] [-w <name>] [-z] [-D <ip-address|device-id>] [-i <image_src>]"
   echo ""
   echo "list partitions to update via fastboot. partitions in:"
   echo ""
   echo "     '-g': gpt"
   echo "     '-b': boot"
   echo "     '-s': system"
   echo "     '-d': userdata"
   echo "     '-c': cache"
   echo "     '-u': bootloader"
   echo "     '-w <name>': hwcfg image name"
   echo "     '-z': recovery"
   echo ""
   echo "'-D' - to flash on a specific device (when multiple devices are connected) otherwise"
	echo "       fastboot over usb would be used'"
   echo ""
   echo "'-i' - image location, if different from current directory"
   echo ""
   echo "defaults to:"
   echo ""
   echo "     '-b -s -d -i .'"
   echo ""
}

fastboot_target=usb
fastboot_cmd='fastboot -u'

update_boot_img=1
update_system=1
update_userdata=1
update_cache=0
update_recovery=0
update_gpt=0
update_hwcfg=0
update_bl=0
selinux=0
img_src='.'
if [ $# -gt 0 ]; then
	update_boot_img=0
	update_system=0
	update_userdata=0
fi

#flag to check if any of the $update_* variables are set
update_something=0
while getopts "hgbdcuw:zD:i:" tag; do
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
		update_bl=1
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
	D)
		fastboot_target=$OPTARG
		fastboot_cmd+=' -s '
		if [[ $OPTARG =~ '.' ]]; then
			fastboot_cmd+='tcp:'
		fi
		fastboot_cmd+=$fastboot_target
		;;
	i)
		img_src=$OPTARG
		;;
	h|*)
		usage
		exit 1
		;;
	esac
done

# if only argument set was to ask for fastboot, roll back default update.
if [[ $update_something -eq 0 ]]; then
	update_boot_img=1
	update_system=1
	update_userdata=1
	update_something=1
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
echo "partitions from $img_src updated in this run:"
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
if [ $update_bl -gt 0 ]; then
	echo "     bsu"
fi
echo ""

sleep 2

echo "running fastboot against $fastboot_target $fastboot_cmd"
# always update the gpt first if user wants to update gpt partition
if [ $update_gpt -gt 0 ]; then
	$fastboot_cmd flash gpt $img_src/gpt.bin
fi

# update the other paritions after gpt has been updated
if [ $update_boot_img -gt 0 ]; then
	$fastboot_cmd flash boot $img_src/boot.img
fi
if [ $update_hwcfg -gt 0 ]; then
	$fastboot_cmd flash hwcfg $hwcfg_img
fi
if [ $update_recovery -gt 0 ]; then
	$fastboot_cmd flash recovery $img_src/recovery.img
fi
if [ $update_bl -gt 0 ]; then
	$fastboot_cmd flash bootloader $img_src/bootloader.img
fi
if [ $update_system -gt 0 ]; then
	$fastboot_cmd flash system $img_src/system.img
fi
if [ $update_userdata -gt 0 ]; then
	$fastboot_cmd flash userdata $img_src/userdata.img
fi
if [ $update_cache -gt 0 ]; then
	$fastboot_cmd flash cache $img_src/cache.img
fi
$fastboot_cmd reboot
echo "done!!!"
exit 0
