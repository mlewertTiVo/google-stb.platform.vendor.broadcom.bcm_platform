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
  echo "Usage: $(basename $0) <output>"
  echo "      output: Output file name and location of the packed release, e.g. ./release.tgz"
  exit 0
fi

# The current directory should be the top of tree.  If not the script would
# bail at one point.
TOP_DIR=$(pwd)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BCG_XML=".repo/manifests/bcg.xml"
WHITE_LIST=".rel.white_list.txt"
BLACK_LIST=".rel.black_list.txt"
REFSW_DIR=".rel.refsw_dir.txt"
OVERRIDES="overrides.xml"
REFSW_TARBALL="refsw_rel_src.tgz"

# Helper to extract path from xml based on the specified attribute name/value
extract_path_from_xml()
{
  echo "cat //project[@$1='$2']/@path" | xmllint --shell $TOP_DIR/$BCG_XML | grep 'path=' | cut -d \" -f 2 >> $3
}

if [ -f $WHITE_LIST ]; then
  rm $WHITE_LIST
fi
if [ -f $BLACK_LIST ]; then
  rm $BLACK_LIST
fi
if [ -f $REFSW_DIR ]; then
  rm $REFSW_DIR
fi

if [ -f $TOP_DIR/$BCG_XML ]; then
  # Broadcom specifics
  extract_path_from_xml groups bcm $WHITE_LIST

  # bolt and kernel
  extract_path_from_xml groups bolt $WHITE_LIST
  extract_path_from_xml groups kernel $WHITE_LIST
  extract_path_from_xml name android/kernel-toolchains $WHITE_LIST

  # Android aosp overrides, generate overrides xml file for unpack
  extract_path_from_xml name android/platform/external/libusb $WHITE_LIST
  extract_path_from_xml name android/platform/external/libusb-compat $WHITE_LIST
  extract_path_from_xml name android/platform/external/bluetooth/bluedroid $WHITE_LIST
  extract_path_from_xml name android/packages/apps/Bluetooth $WHITE_LIST
  extract_path_from_xml name android/platform/hardware/libhardware $WHITE_LIST
  echo "<manifest>" > $OVERRIDES
  xmllint --xpath //remove-project .repo/manifests/bcg.xml >> $OVERRIDES
  echo "</manifest>" >> $OVERRIDES
  echo $OVERRIDES >> $WHITE_LIST

  # Misc tools
  extract_path_from_xml name android/busybox $WHITE_LIST

  # refsw is packed separately, store its location for unpack
  extract_path_from_xml groups refsw $REFSW_DIR
  cat $REFSW_DIR >> $BLACK_LIST
  echo $REFSW_DIR >> $WHITE_LIST
else
  echo "$BCG_XML not found, exiting..."
  exit 0
fi

# Tar up refsw separately, note black list contains the refsw location
cd $(cat $REFSW_DIR)
tar --exclude-from=rockford/release/exclude.txt -cvzf $TOP_DIR/$REFSW_TARBALL *
cd $TOP_DIR
echo $REFSW_TARBALL >> $WHITE_LIST


# Append custom white and black lists if present
if [ -f $SCRIPT_DIR/include.txt ]; then
  cat $SCRIPT_DIR/include.txt >> $WHITE_LIST
fi
if [ -f $SCRIPT_DIR/exclude.txt ]; then
  cat $SCRIPT_DIR/exclude.txt >> $BLACK_LIST
fi

# Tar up everything
tar --exclude-from=$BLACK_LIST -cvzf $1 --files-from=$WHITE_LIST

echo "Release tarball ($1) generated"

# Remove temporary files
echo "Cleaning up..."
rm $REFSW_TARBALL $BLACK_LIST $WHITE_LIST $OVERRIDES $REFSW_DIR

echo "Done!"
