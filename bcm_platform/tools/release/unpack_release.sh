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

# Create direction if not already
if [ ! -d $2 ]; then
  mkdir -p $2
fi

DST_DIR="$(cd $2 && pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REFSW_TARBALL="refsw_rel_src.tgz"
REFSW_DIR_FILE=".rel.refsw_dir.txt"
REFSW_DIR="vendor/broadcom/refsw"
LOCAL_MANIFESTS_DIR=".repo/local_manifests"
OVERRIDES="overrides.xml"

# Untar release
tar -xvf $1 --directory $DST_DIR

if [ -f $DST_DIR/$REFSW_DIR_FILE ]; then
  # Override refsw directory if there is one
  REFSW_DIR="$(cat $DST_DIR/$REFSW_DIR_FILE)"
fi

# Untar refsw release
if [ ! -d $DST_DIR/$REFSW_DIR ]; then
  mkdir -p $DST_DIR/$REFSW_DIR
fi
tar -xvf $DST_DIR/$REFSW_TARBALL --directory $DST_DIR/$REFSW_DIR

# Move overrides to the intended location
if [ ! -d $DST_DIR/$LOCAL_MANIFESTS_DIR ]; then
  mkdir -p $DST_DIR/$LOCAL_MANIFESTS_DIR
fi
mv $DST_DIR/$OVERRIDES $DST_DIR/$LOCAL_MANIFESTS_DIR/

echo "Release tarball unpacked to $DST_DIR"

# Remove temporary files
echo "Cleaning up..."
if [ -f $DST_DIR/$REFSW_TARBALL ]; then
  rm $DST_DIR/$REFSW_TARBALL
fi
if [ -f $DST_DIR/$REFSW_DIR_FILE ]; then
  rm $DST_DIR/$REFSW_DIR_FILE
fi

echo "Done!"
