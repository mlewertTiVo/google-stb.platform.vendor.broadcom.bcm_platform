#!/bin/bash

# Script to generate backup banners for Broadcom Test app that are not yet
# installed.
generate_backup_banner()
{
    # Adjustable
    local banner="$1"
    local fontSize=24
    # Fixed
    local density=xhdpi
    local font="Arial-Regular"
    local directory="res/drawable-$density"
    local input="../../tools/brand/$directory/ic_${banner}_banner.png"
    local output="$directory/ic_${banner}_backup_banner.png"

    echo "Generating $output from $input"
    mkdir -p $directory
    convert \
     $input \
     -font $font \
     -pointsize $fontSize \
     -fill white \
     -undercolor '#00000080' \
     -gravity North \
     -annotate +0+5 "Not Installed" \
     $output
}

# Generate our backup banners
generate_backup_banner "adjust_screen_offset"
generate_backup_banner "ota_updater"
generate_backup_banner "sideband_viewer"
generate_backup_banner "uri_player"
