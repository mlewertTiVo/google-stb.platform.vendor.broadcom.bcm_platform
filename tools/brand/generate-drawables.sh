#!/bin/bash

# Script to re-generate our common icons (do not run in each app)

# To avoid duplicating PNG files, please append the common res subdirectory to
# your LOCAL_RESOURCE_DIR in Android.mk. For example:
#   LOCAL_RESOURCE_DIR := $(TOP)/frameworks/support/v17/leanback/res
#   LOCAL_RESOURCE_DIR += $(LOCAL_PATH)/res
#   LOCAL_RESOURCE_DIR += $(BCM_VENDOR_STB_ROOT)/bcm_platform/tools/brand/res

BROADCOM_LOGO=Broadcom.svg

# Create (non-leanback) launcher icons (used in TvSettings)

# Generate an icon by density and size
# $1 density
# $1 size
# Exit status: 0 on success, 1 on error
generate_ic_launcher_by_density_size()
{
    local density=$1
    local size=$2
    local directory="res/drawable-$density"
    local output="$directory/ic_launcher.png"

    echo "Generating $output (${size}x${size})..."
    mkdir -p $directory
    convert -resize $size $BROADCOM_LOGO $output
}

# Generate an icon by density
# $1 density
# Exit status: 0 on success, 1 on error
generate_ic_launcher_by_density()
{
    local density=$1
    local size
    case $density in
    ldpi)
        size=36
        ;;
    mdpi)
        size=48
        ;;
    tvdpi)
        size=64
        ;;
    hdpi)
        size=72
        ;;
    xhdpi)
        size=96
        ;;
    xxhdpi)
        size=144
        ;;
    xxxhdpi)
        size=192
        ;;
    *)
        echo "Unknown density $density" >&2
        return 1
        ;;
    esac
    generate_ic_launcher_by_density_size $density $size
}

# Generate an icon for each density
for density in ldpi mdpi tvdpi hdpi xhdpi xxhdpi xxxhdpi
do
    generate_ic_launcher_by_density $density
done

