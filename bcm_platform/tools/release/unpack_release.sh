#!/bin/bash

# This script unpacks the release tarball

if [ $# -lt 2 ]; then
  echo "Usage: $(basename $0) <src_file> <dst>"
  echo "      src_file: Packed release to be unpacked"
  echo "      dst     : Location where the release is unpacked, should be the top of tree"
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

DST_DIR="$(cd $2 && pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP_DIR=".rel_tmp"

REFSW_TARBALL="$TMP_DIR/refsw_rel_src.tgz"
REFSW_DIR_FILE="$TMP_DIR/refsw_dir.txt"
AOSP_LIST="$TMP_DIR/aosp_patches.txt"
REFSW_DIR="vendor/broadcom/refsw"

# Untar release
tar -xvf $1 --directory $DST_DIR

# Apply aosp patches
cd $DST_DIR
while read line; do
  cd $line
  git am $DST_DIR/$TMP_DIR/$line/*
  cd $DST_DIR
done < $DST_DIR/$AOSP_LIST

if [ -f $DST_DIR/$REFSW_DIR_FILE ]; then
  # Override refsw directory if there is one
  REFSW_DIR="$(cat $DST_DIR/$REFSW_DIR_FILE)"
fi

# Untar refsw release
if [ ! -d $DST_DIR/$REFSW_DIR ]; then
  mkdir -p $DST_DIR/$REFSW_DIR
fi
tar -xvf $DST_DIR/$REFSW_TARBALL --directory $DST_DIR/$REFSW_DIR

echo "Release tarball unpacked to $DST_DIR"

# Remove temporary files
echo "Cleaning up..."
if [ -d $DST_DIR/$TMP_DIR ]; then
  rm -rf $DST_DIR/$TMP_DIR
fi

echo "Done!"
