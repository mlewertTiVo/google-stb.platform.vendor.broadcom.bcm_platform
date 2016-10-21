#!/bin/bash

# This script unpacks the release tarball for patch releases

if [ $# -lt 2 ]; then
  echo "Usage: $(basename $0) <src_file> <dst_dir>"
  echo "      src_file  : Release package to be unpacked"
  echo "      dst_dir   : Destination where the release package is unpacked"
  exit 0
fi

if [ ! -f $1 ]; then
  echo "$s not found, exiting..."
  exit 0
fi

# Destination directory should exist by now
if [ ! -d $2 ]; then
  echo "Destination $2 does not exist, exiting..."
  exit 0
fi

# Abort on error
set -e

DST_DIR="$(cd $2 && pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP_DIR="tmp_bcmrel"

PATCH_LIST="$TMP_DIR/patches.txt"

# Untar release
tar -xvf $1 --directory $DST_DIR

cd $DST_DIR

# Apply patches, use 'git am' if possible to retain history
cd $DST_DIR
while read line; do
  cd $line
  git am $DST_DIR/$TMP_DIR/$line/*.patch > /dev/null 2>&1 || git apply $DST_DIR/$TMP_DIR/$line/*.patch
  cd $DST_DIR
done < $DST_DIR/$PATCH_LIST

echo "Release patch tarball unpacked to $DST_DIR"

# Remove temporary files
echo "Cleaning up..."
if [ -d $DST_DIR/$TMP_DIR ]; then
  rm -rf $DST_DIR/$TMP_DIR
fi

echo "Done!"
