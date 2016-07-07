#!/bin/bash

# This script unpacks the release tarball

if [ $# -lt 2 ]; then
  echo "Usage: $(basename $0) <src_file> <dst_dir> <addon_dir>"
  echo "      src_file  : Release package to be unpacked"
  echo "      dst_dir   : Destination where the release package is unpacked"
  echo "      addon_dir : Add-on folder in which additional packages are supplied"
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
if [ ! -d $3 ]; then
  echo "Add-on folder isn't specified..."
else
  ADDON_DIR="$(cd $3 && pwd)"
fi

# Abort on error
set -e

DST_DIR="$(cd $2 && pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP_DIR="tmp_bcmrel"

REFSW_TARBALL="$ADDON_DIR/refsw_rel_src.tgz"
REFSW_DIR_FILE="$TMP_DIR/refsw_dir.txt"
AOSP_LIST="$TMP_DIR/aosp_patches.txt"
BOLT_VER="$TMP_DIR/bolt_version.txt"
BOLT_DIR="$TMP_DIR/bolt_dir.txt"
REFSW_DIR="vendor/broadcom/refsw"
REFSW_PATCH="$TMP_DIR/refsw_patch.txt"
PR_TARBALL="$ADDON_DIR/playready_prebuilts.tgz"

# Untar release
tar -xvf $1 --directory $DST_DIR

if [ -f $DST_DIR/$REFSW_DIR_FILE ]; then
  # Override refsw directory if there is one
  REFSW_DIR="$(cat $DST_DIR/$REFSW_DIR_FILE)"
fi

# Untar add-ons
if [ -f $REFSW_TARBALL ]; then
  mkdir -p $DST_DIR/$REFSW_DIR;
  tar -C $DST_DIR/$REFSW_DIR -zxvf $REFSW_TARBALL
fi
if [ -f $PR_TARBALL ]; then
  tar -C $DST_DIR -zxvf $PR_TARBALL
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
while read line; do
  cd $line
  git am $DST_DIR/$TMP_DIR/$line/* > /dev/null 2>&1 || git apply $DST_DIR/$TMP_DIR/$line/*
  cd $DST_DIR
done < $DST_DIR/$AOSP_LIST

if [ -f $DST_DIR/$PR_PATCH_USER ]; then
  tar -zxvf $DST_DIR/$PR_PATCH_USER
fi
if [ -f $DST_DIR/$PR_PATCH_USERDBG ]; then
  tar -zxvf $DST_DIR/$PR_PATCH_USERDBG 
fi

# Copy version file
cp $BOLT_VER $(cat $BOLT_DIR)/version

echo "Release tarball unpacked to $DST_DIR"

# Remove temporary files
echo "Cleaning up..."
if [ -d $DST_DIR/$TMP_DIR ]; then
  rm -rf $DST_DIR/$TMP_DIR
fi

echo "Done!"
