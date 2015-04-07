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
AOSP_NAME_LIST="$TMP_DIR/aosp_override_list.txt"

if [ $# -ge 2 ]; then
  AOSP_BASELINE=$2
fi

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

# Helper to extract name of those "remove-projects" from bcg xml
extract_remove_project_name_from_xml()
{
  echo "cat //remove-project/@name" | xmllint --shell $BCG_XML | grep name= | cut -d \" -f 2 > $AOSP_NAME_LIST
  while read line; do
    extract_value_from_xml project name $line $DFT_XML path >> $AOSP_LIST
  done < $AOSP_NAME_LIST
}

# Helper to extract aosp patches per baseline
extract_patches_from_aosp()
{
  AOSP_PATH=$1
  mkdir -p $TOP_DIR/$TMP_DIR/$AOSP_PATH
  CURR_DIR=$(pwd)
  cd $AOSP_PATH

  # Generate aosp patches based on revision in default xml unless user overrided
  if [ -z $AOSP_BASELINE ]; then
    # First find out the remote either from the line or from default.  Once
    # the remote is found, then find out the fetch location of the baseline.
    REMOTE=$(extract_value_from_xml project path $AOSP_PATH $DFT_XML remote)
    if [ -z $REMOTE ]; then
      REMOTE=$(echo "cat //default/@remote" | xmllint --shell $DFT_XML | grep remote= | cut -d \" -f 2)
    fi
    REMOTE_FETCH=$(extract_value_from_xml remote name $REMOTE $DFT_XML fetch)

    # Finally get the aosp baseline
    PROJ_NAME=$(extract_value_from_xml project path $AOSP_PATH $DFT_XML name)
    BASELINE=$(extract_value_from_xml project path $AOSP_PATH $DFT_XML revision)
    if [ -z $BASELINE ]; then
      BASELINE=$(echo "cat //default/@revision" | xmllint --shell $DFT_XML | grep revision= | cut -d \" -f 2)
    fi
  else
    REMOTE=$(extract_value_from_xml project path $AOSP_PATH $BCG_XML remote)
    REMOTE_FETCH=$(extract_value_from_xml remote name $REMOTE $BCG_XML fetch)
    PROJ_NAME=$(extract_value_from_xml project path $AOSP_PATH $BCG_XML name)
    BASELINE=$AOSP_BASELINE
  fi
  # Strip the redundant '/', if any
  REMOTE_FETCH="${REMOTE_FETCH%/}"
  git fetch $REMOTE_FETCH/$PROJ_NAME $BASELINE
  git format-patch FETCH_HEAD -o $TOP_DIR/$TMP_DIR/$AOSP_PATH
  cd $CURR_DIR
}

create_patches_for_all_aosp_overrides()
{
  extract_remove_project_name_from_xml
  while read line; do
    echo "Creating patches for $line..."
    extract_patches_from_aosp $line
  done < $AOSP_LIST
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
  create_patches_for_all_aosp_overrides

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
echo "Excludes..."
cat $BLACK_LIST

echo "Release tarball ($1) generated"

# Remove temporary files
echo "Cleaning up..."
rm -rf $TMP_DIR

echo "Done!"
