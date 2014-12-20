How to use pack_release.sh
--------------------------

1. Create a fresh lollipop tree from internal git repos as usual.

2. Run plat-droid.py to generate the intended supported platform(s).

Note: Unfortunately at the moment plat-droid.py looks for a script in refsw
that is not included as part of refsw release, so need to run this prior to
packing.

3. Run pack_release.sh from the root of tree, e.g.:
./vendor/broadcom/bcm_platform/tools/release/pack_release.sh 14.4_release.tgz

How to use unpack_release.sh
----------------------------

1. Check out a vanilla Android L aosp tree.  Run the "repo init" command, but
do not run "repo sync" yet as the release would apply a local manifest.

2. Run unpack_release.sh by specifying the aosp tree location and the release
tarball name, e.g.:
unpack_release.sh 14.4_release.tgz ~/projects/l-aosp

3. "repo sync -j8" to fetch the rest of tree.

4. "lunch" and select build target.

5. "make -j8" per usual.
