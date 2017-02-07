#!/bin/bash

# This script unpacks the release tarball

if [ $# -lt 2 ]; then
  echo "Usage: $(basename $0) <src_file> <dst_dir> <addon_dir>"
  echo "      src_file  : Release package to be unpacked"
  echo "      dst_dir   : Destination where the release package is unpacked"
  echo "      addon_dir : Add-on directory in which additional packages are supplied"
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

# Check if Add-on directory is provided
if [ $# -lt 3 ] || [ ! -d $3 ]; then
  ADDON_DIR=""
  echo "No add-ons"
else
  ADDON_DIR="$(cd $3 && pwd)"
  echo "Add-on directory: $ADDON_DIR"
fi

# Abort on error
set -e

DST_DIR="$(cd $2 && pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP_DIR="tmp_bcmrel"

REFSW_TARBALL="$ADDON_DIR/refsw_rel_src.tgz"
REFSW_DIR_FILE="$TMP_DIR/refsw_dir.txt"
AOSP_LIST="$TMP_DIR/aosp_patches.txt"
BOLT_TARBALL="$TMP_DIR/bolt-customer.tgz"
BOLT_DIR="$TMP_DIR/bolt_dir.txt"
REFSW_DIR="vendor/broadcom/refsw"
REFSW_PATCH="$TMP_DIR/refsw_patch.txt"

# Untar release
tar -xvf $1 --directory $DST_DIR

if [ -f $DST_DIR/$REFSW_DIR_FILE ]; then
  # Override refsw directory if there is one
  REFSW_DIR="$(cat $DST_DIR/$REFSW_DIR_FILE)"
fi

# Untar add-ons
if [ ! -z $ADDON_DIR ]; then
  for file in $ADDON_DIR/*.tgz; do
    echo "Untar add-on $file..."
    if [ $file == $REFSW_TARBALL ]; then
      mkdir -p $DST_DIR/$REFSW_DIR;
      tar -C $DST_DIR/$REFSW_DIR -zxvf $file
    else
      tar -C $DST_DIR -zxvf $file
    fi
  done
fi

cd $DST_DIR

# Check if $REFSW_DIR content
if [ -d $DST_DIR/$REFSW_DIR ]; then
  echo "Checking if $DST_DIR/$REFSW_DIR has legit contents in it..."
  if [ -d $DST_DIR/$REFSW_DIR/BSEAV ] && [ -d $DST_DIR/$REFSW_DIR/magnum ] && [ -d $DST_DIR/$REFSW_DIR/nexus ] && [ -d $DST_DIR/$REFSW_DIR/rockford ]; then
    echo "$DST_DIR/$REFSW_DIR appears to have the right content in it."
  else
    echo "$DST_DIR/$REFSW_DIR does NOT appear to have the right content in it."
    exit 0
  fi
  if [ -f $DST_DIR/$REFSW_PATCH ]; then
    cd $DST_DIR/$REFSW_DIR
    patch -p1 < $DST_DIR/$REFSW_PATCH
  fi
fi

# Apply aosp patches, use 'git am' if possible to retain history
cd $DST_DIR
if [ -f $DST_DIR/$AOSP_LIST ]; then
  while read line; do
    cd $line
    git am $DST_DIR/$TMP_DIR/$line/* > /dev/null 2>&1 || git apply $DST_DIR/$TMP_DIR/$line/*
    cd $DST_DIR
  done < $DST_DIR/$AOSP_LIST
fi

if [ -f $DST_DIR/$PR_PATCH_USER ]; then
  tar -zxvf $DST_DIR/$PR_PATCH_USER
fi
if [ -f $DST_DIR/$PR_PATCH_USERDBG ]; then
  tar -zxvf $DST_DIR/$PR_PATCH_USERDBG 
fi

# Unpack bolt and move it to the destination
tar -C $TMP_DIR -zxvf $DST_DIR/$BOLT_TARBALL
mv $TMP_DIR/bolt-v* $(cat $BOLT_DIR)

echo "Release tarball unpacked to $DST_DIR"

# Remove temporary files
echo "Cleaning up..."
if [ -d $DST_DIR/$TMP_DIR ]; then
  rm -rf $DST_DIR/$TMP_DIR
fi

echo "Done!"
