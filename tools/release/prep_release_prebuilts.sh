#!/bin/bash

# This script packages those prebuilt binaries that are required by the
# release tarball.  This script is expected to run on a workspace that
# contains the compiled out/target folder for the intended user or userdebug 
# targets.  The script would then extract the required binaries from the 
# workspace and create a tgz.
#
# The script is meant to be automatic for the most part, so the user only needs
# to specify the output file and location, and the rest should be taken care
# of.

function HELP {
  echo -e \\n"Usage: $(basename $0) <user|userdebug> <output>"
  echo "       output         : path to where the packaged release_prebuilts"
  echo "                        tarballs will be copied to"
  echo "       user|userdebug : Specify if the workspace contains binaries"
  echo "                        from a 'user' image or a 'userdebug/eng' image"
  echo "                        For user image, use the 'user' option, otherwise"
  echo "                        userdebug and eng image, use the 'userdebug' option"
  exit 1
}


# The current directory should be the top of tree.  If not the script would
# bail at one point.
TOP_DIR=$(pwd)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP_DIR=tmp_bcm_rel_prebuilts
RP=release_prebuilts
VENDOR_BRCM=vendor/broadcom

if [ $# -lt 2 ]; then
  HELP
  exit 1
fi
VAR=$1
OUTPUT=$2

if [ -d $TMP_DIR ]; then
  rm -rf $TMP_DIR
fi

echo "staging release_prebuilts..."
mkdir -p $TMP_DIR/$RP/$VAR;
while read line; do
  line=$(eval echo $line)
  if [ ! -e $line ]; then
    echo -e "!!! MISSING prebuilt libraries for release packaging: $line"
  else
    cp $line $TMP_DIR/$RP/$VAR;
  fi
done < $SCRIPT_DIR/release_prebuilts.txt
tar -C $TMP_DIR -zcvf $OUTPUT/release_prebuilts_$VAR.tgz $RP;
rm -rf $TMP_DIR

echo "staging playready prebuilts..."
mkdir -p $TMP_DIR/$VENDOR_BRCM/$RP/$VAR;
while read line; do
  line=$(eval echo $line)
  if ls $line 1> /dev/null 2>&1; then
    IS_PREBUILT_LIB=$(echo $line | awk -F/ '{print $1}')
    if [[ "$IS_PREBUILT_LIB" == "out" ]]; then
      cp $line $TMP_DIR/$VENDOR_BRCM/$RP/$VAR;
    else
      cp -r --parents $line $TMP_DIR;
    fi
  else
    echo -e "!!! MISSING prebuilt libraries for release packaging: $line"
  fi
done < $SCRIPT_DIR/playready_prebuilts.txt
tar -C $TMP_DIR -zcvf $OUTPUT/playready_prebuilts_$VAR.tgz ./

# Remove temporary files
echo "Cleaning up..."
rm -rf $TMP_DIR

echo "Done!"
