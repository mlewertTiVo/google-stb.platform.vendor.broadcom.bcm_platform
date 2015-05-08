How to use pack_release.sh
--------------------------

1. Create a fresh lollipop tree from internal git repos as usual.

2. Run plat-droid.py to generate the intended supported platform(s).

Note: Unfortunately at the moment plat-droid.py looks for a script in refsw
that is not included as part of refsw release, so need to run this prior to
packing.

3. Create "release_prebuilts" in the root of tree

4. Copy the needed prebuilt binaries as listed in ./vendor/broadcom/bcm_platform/tools/release/release_prebuilts.txt into release_prebuilts

5. Run pack_release.sh from the root of tree, e.g.:

./vendor/broadcom/bcm_platform/tools/release/pack_release.sh android5.1_15.2take1.tgz

Note 1: By default, refsw source code is NOT included in the release tarball.  If you want to include refsw src code in your current code tree into the release tarball, you should run the above script with the -t option, e.g.:

./vendor/broadcom/bcm_platform/tools/release/pack_release.sh -t android5.1_15.2take1.tgz

Note 2: If you want to create a patch between your current refsw code tree and a predefined refsw baseline (e.g. create a patch for all Android specific refsw changes that were applied on top of an official URSR release), you can use the -r and -s options to specify the baseline and the SHA in the baseline that the script would use to create the diff and patch, e.g.:

./vendor/broadcom/bcm_platform/tools/release/pack_release.sh -r shared/SWSTB-1 -s dc1e5072c51e4c80f29551d0873bbdf6ae4d2217 android5.1_15.2take1.tgz

Note 3: If you include -t in addition to -r and -s in the example given above in Note 2, a snapshot of the refsw source code as indicated by the reference reference baseline/SHA will be packaged into the release tarball.


How to use unpack_release.sh
----------------------------

1. Check out a vanilla Android L aosp tree.  Run the "repo init" command.

2. "repo sync -j8" to fetch the aosp tree.

3. Run unpack_release.sh by specifying the aosp tree location and the release
tarball name, e.g.:
unpack_release.sh android5.0_14.4take1.tgz ~/projects/l-aosp

Note: The script will check if aosp tree location has been preloaded with a version of refsw code base in vendor/broadcom/refsw. If it doesn't, the script would then check if a version of refsw source is included in the release tarball and use it if one exists.  If the script can't locate a version of refsw source in the expected location nor can it find a copy of refsw source code in the release tarball, the script will bail and the user is expected to source a version of URSR release that works with the given Android release and populate the AOSP code tree acccordingly. 

4. "lunch" and select a build target.

5. "make -jN" per usual.
