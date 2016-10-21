#!/bin/bash

function HELP {
  echo -e \\n"Usage: $(basename $0) <patch_file> <output>"
  echo "     output : Output file name and location of the packed release, e.g. ./release.tgz"
  exit 1
}


# The current directory should be the top of tree.  If not the script would
# bail at one point.
TOP_DIR=$(pwd)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP_DIR=tmp_bcmrel

AOSP_LIST="$TMP_DIR/patches.txt"
WHITE_LIST="$TMP_DIR/white_list.txt"
BLACK_LIST="$TMP_DIR/black_list.txt"
UNPACK_PATCH_FILE="unpack_release_patch.sh"

if [ $# -lt 2 ]; then
  HELP
  exit 1
fi

echo -e \\n"Release packaging will start in 5 seconds with the following options..."\\n
echo -e "   Release package name : $2"\\n
for i in {5..1}; do echo -en "Starting in $i \r"; sleep 1; done;
echo -e ""\\n

# Helper to extract a value from xml based on the specified attribute name/value
extract_value_from_xml()
{
  echo "cat //$1[@$2='$3']/@$5" | xmllint --shell $4 | grep $5= | cut -d \" -f 2
}

# Helper to extract path from bcg xml
extract_path_from_xml()
{
  extract_value_from_xml project $1 $2 $BCG_XML path
}

# Helper to extract aosp patches per baseline
extract_patches_from_aosp()
{
  AOSP_PATH=$1
  mkdir -p $TOP_DIR/$TMP_DIR/$AOSP_PATH
  cp $AOSP_PATH/*.patch $TOP_DIR/$TMP_DIR/$AOSP_PATH
}

create_patches_for_all_aosp_overrides()
{
  while read line; do
    echo "Creating patches for $line..."
    extract_patches_from_aosp $line
  done < $AOSP_LIST
}

if [ -d $TMP_DIR ]; then
  rm -rf $TMP_DIR
fi
mkdir -p $TMP_DIR

# Copy patch list and unpack script
cp $1 $AOSP_LIST
cp $SCRIPT_DIR/$UNPACK_PATCH_FILE $TMP_DIR/

# Android aosp overrides, generate patches from a given baseline
create_patches_for_all_aosp_overrides

# Tar up the entire temp directory except the white/black lists
echo $TMP_DIR >> $WHITE_LIST
echo $WHITE_LIST >> $BLACK_LIST
echo $BLACK_LIST >> $BLACK_LIST

# Tar up everything
tar --exclude=*.git* --exclude-from=$BLACK_LIST -cvzf $2.tgz --files-from=$WHITE_LIST

# Be verbose what we are not tar'ing
echo "Excludes..."
cat $BLACK_LIST

echo "Release tarball ($2) generated"

# Remove temporary files
echo "Cleaning up..."
rm -rf $TMP_DIR

echo "Done!"
