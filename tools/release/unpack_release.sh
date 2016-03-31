#!/bin/bash

# This script unpacks the release tarball

if [ $# -lt 2 ]; then
  echo "Usage: $(basename $0) <src_file> <dst_dir>"
  echo "      src_file: Release package to be unpacked"
  echo "      dst_dir : Destination where the release package is unpacked"
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

REFSW_TARBALL="$TMP_DIR/refsw_rel_src.tgz"
REFSW_DIR_FILE="$TMP_DIR/refsw_dir.txt"
AOSP_LIST="$TMP_DIR/aosp_patches.txt"
BOLT_VER="$TMP_DIR/bolt_version.txt"
BOLT_DIR="$TMP_DIR/bolt_dir.txt"
REFSW_DIR="vendor/broadcom/refsw"
REFSW_PATCH="$TMP_DIR/refsw_patch.txt"
PR_PATCH_USER="$TMP_DIR/playready_prebuilts_user.tgz"
PR_PATCH_USERDBG="$TMP_DIR/playready_prebuilts_userdebug.tgz"

# Untar release
tar -xvf $1 --directory $DST_DIR

cd $DST_DIR

if [ -f $DST_DIR/$REFSW_DIR_FILE ]; then
  # Override refsw directory if there is one
  REFSW_DIR="$(cat $DST_DIR/$REFSW_DIR_FILE)"
fi

# Check if $REFSW_DIR already exists in $DST_DIR
if [ ! -d $DST_DIR/$REFSW_DIR ]; then
  echo -e \\n"$DST_DIR/$REFSW_DIR does not exist..."
  echo -e "Checking if $DST_DIR/$REFSW_TARBALL is included in the release tarball..."
  # Check if a copy of refsw has been packaged in the release
  if [ -f $DST_DIR/$REFSW_TARBALL ]; then
    echo "$DST_DIR/$REFSW_TARBALL is included in the release tarball. Use it..."
    # Untar refsw release
    if [ ! -d $DST_DIR/$REFSW_DIR ]; then
      mkdir -p $DST_DIR/$REFSW_DIR
    fi
    tar -xvf $DST_DIR/$REFSW_TARBALL --directory $DST_DIR/$REFSW_DIR
  else
    echo "$DST_DIR/$REFSW_TARBALL is NOT included in the release tarball. Exiting..."
    exit 0
  fi
else
  echo "Checking if $DST_DIR/$REFSW_DIR has legit contents in it..."
  if [ -d $DST_DIR/$REFSW_DIR/BSEAV ] && [ -d $DST_DIR/$REFSW_DIR/magnum ] && [ -d $DST_DIR/$REFSW_DIR/nexus ] && [ -d $DST_DIR/$REFSW_DIR/rockford ]; then
    echo "$DST_DIR/$REFSW_DIR appears to have the right content in it."
  else
    echo "$DST_DIR/$REFSW_DIR does NOT appear to have the right content in it."
    exit 0
  fi
fi

if [ -f $DST_DIR/$REFSW_PATCH ]; then
  cd $DST_DIR/$REFSW_DIR
  patch -p1 < $DST_DIR/$REFSW_PATCH
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
