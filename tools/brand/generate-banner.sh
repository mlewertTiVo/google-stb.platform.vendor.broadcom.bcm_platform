#!/bin/bash

# Script to generate a leanback banner with your app's title
# (run in your app's main directory, not here!)
# For example:
#   ../../tools/brand/generate-banner.sh "My Application"
#   ls res/drawable-xhdpi/banner.png
#
# For details about leanback banners, see
# http://developer.android.com/training/tv/start/start.html

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BROADCOM_LOGO=$DIR/Broadcom.svg

if [ "$1" == "" ]; then
    echo "usage: $0 \"title\""
    exit 1
fi

# Generate a leanback launcher banner
generate_banner()
{
    # Adjustable
    local title="$1"
    local fontSize=28
    # Fixed
    local density=xhdpi
    local width=320
    local height=180
    local font="Arial-Regular"
    local directory="res/drawable-$density"
    local output="$directory/banner.png"

    echo "Generating $output (${width}x${height})..."
    mkdir -p $directory
    convert \
     -resize $width \
     -gravity center \
     -background white \
     -extent ${width}x${height} \
     $BROADCOM_LOGO \
     -font $font \
     -pointsize $fontSize \
     -fill "#231F20" \
     -gravity South \
     -annotate +0+25 "$title" \
     $output
}

# Generate the leanback launcher banner
generate_banner "$1"

