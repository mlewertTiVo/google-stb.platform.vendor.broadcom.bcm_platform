#!/bin/bash

# This script generates a release tarball based on the packages found within
# bcg.xml.  Additionally it packages separately refsw with the exclude list
# embedded.  Finally, the scripts looks at include.txt and exclude.txt for
# additionally files/directories to be included and excluded for the release.
#
# THe script is meant to be automatic for the most part, so the user only needs
# to specify the output file and location, and the rest should be taken care
# of.

if [ $# -lt 1 ]; then
  echo "Usage: $(basename $0) <output> [<aosp_baseline>]"
  echo "      output: Output file name and location of the packed release, e.g. ./release.tgz"
  echo "      aosp_baseline: AOSP baseline tag where patches is generated from."
  echo "                     If not specified, revision from default manifest will be used."
  exit 0
fi

# The current directory should be the top of tree.  If not the script would
# bail at one point.
TOP_DIR=$(pwd)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP_DIR=tmp_bcmrel

DFT_XML="$TOP_DIR/.repo/manifests/default.xml"
BCG_XML="$TOP_DIR/.repo/manifests/bcg.xml"
REFSW_DIR="$TMP_DIR/refsw_dir.txt"
BOLT_DIR="$TMP_DIR/bolt_dir.txt"
BOLT_VER="$TMP_DIR/bolt_version.txt"
REFSW_TARBALL="$TMP_DIR/refsw_rel_src.tgz"
AOSP_LIST="$TMP_DIR/aosp_patches.txt"

# TODO: Move these to a true temp directory but we can live this for now
WHITE_LIST="$TMP_DIR/white_list.txt"
BLACK_LIST="$TMP_DIR/black_list.txt"

if [ $# -ge 2 ]; then
  AOSP_BASELINE=$2
fi

# Helper to extract a value from xml based on the specified attribute name/value
extract_value_from_xml()
{
  echo "cat //project[@$1='$2']/@$4" | xmllint --shell $3 | grep $4= | cut -d \" -f 2
}

# Helper to extract path from bcg xml
extract_path_from_xml()
{
  extract_value_from_xml $1 $2 $BCG_XML path
}

# Helper to extract aosp patches per baseline
extract_patches_from_aosp()
{
  AOSP_PATH=$1

  # Generate aosp patches based on revision in default xml unless user overrided
  if [ -z $AOSP_BASELINE ]; then
    BASELINE=$(extract_value_from_xml path $AOSP_PATH $DFT_XML revision)
  else
    BASELINE=$AOSP_BASELINE
  fi
  echo $AOSP_PATH >> $AOSP_LIST
  mkdir -p $TOP_DIR/$TMP_DIR/$AOSP_PATH
  CURR_DIR=$(pwd)
  cd $AOSP_PATH
  git format-patch $BASELINE -o $TOP_DIR/$TMP_DIR/$AOSP_PATH
  cd $CURR_DIR
}

if [ -d $TMP_DIR ]; then
  rm -rf $TMP_DIR
fi
mkdir -p $TMP_DIR

if [ -f $BCG_XML ]; then
  # refsw is packed separately, store its location for unpack
  extract_path_from_xml groups refsw >> $REFSW_DIR
  cat $REFSW_DIR >> $BLACK_LIST

  # Broadcom specifics
  extract_path_from_xml groups bcm >> $WHITE_LIST

  # bolt, save its version for unpack
  extract_path_from_xml groups bolt >> $BOLT_DIR
  cat $BOLT_DIR >> $WHITE_LIST
  cd $(cat $BOLT_DIR)
  echo -n "$(git describe --dirty || git rev-parse --short HEAD)" >> $TOP_DIR/$BOLT_VER
  cd $TOP_DIR

  # kernel
  extract_path_from_xml groups kernel >> $WHITE_LIST
  extract_path_from_xml name android/kernel-toolchains >> $WHITE_LIST

  # Android aosp overrides, generate patches from a given baseline
  extract_patches_from_aosp external/libusb
  extract_patches_from_aosp external/libusb-compat
  extract_patches_from_aosp external/bluetooth/bluedroid
  extract_patches_from_aosp packages/apps/Bluetooth
  extract_patches_from_aosp hardware/libhardware
  extract_patches_from_aosp build

  # Misc tools
  extract_path_from_xml name android/busybox >> $WHITE_LIST
else
  echo "$BCG_XML not found, exiting..."
  exit 0
fi

# Tar up refsw separately, note black list contains the refsw location
cd $(cat $REFSW_DIR)
tar --exclude=*.git* --exclude-from=rockford/release/exclude.txt -cvzf $TOP_DIR/$REFSW_TARBALL *
cd $TOP_DIR

# Append custom white and black lists if present
if [ -f $SCRIPT_DIR/include.txt ]; then
  cat $SCRIPT_DIR/include.txt >> $WHITE_LIST
fi
if [ -f $SCRIPT_DIR/exclude.txt ]; then
  cat $SCRIPT_DIR/exclude.txt >> $BLACK_LIST
fi

# Tar up the entire temp directory except the white/black lists
echo $TMP_DIR >> $WHITE_LIST
echo $WHITE_LIST >> $BLACK_LIST
echo $BLACK_LIST >> $BLACK_LIST

# Tar up everything
tar --exclude=*.git* --exclude-from=$BLACK_LIST -cvzf $1 --files-from=$WHITE_LIST

# Be verbose what we are not tar'ing
echo "excludes..."
cat $BLACK_LIST

echo "Release tarball ($1) generated"

# Remove temporary files
echo "Cleaning up..."
rm -rf $TMP_DIR

echo "Done!"
