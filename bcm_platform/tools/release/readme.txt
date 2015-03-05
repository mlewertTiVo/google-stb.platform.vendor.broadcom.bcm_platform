How to use pack_release.sh
--------------------------

1. Create a fresh lollipop tree from internal git repos as usual.

2. Run plat-droid.py to generate the intended supported platform(s).

Note: Unfortunately at the moment plat-droid.py looks for a script in refsw
that is not included as part of refsw release, so need to run this prior to
packing.

3. Run pack_release.sh from the root of tree, e.g.:
./vendor/broadcom/bcm_platform/tools/release/pack_release.sh android5.0_14.4take1.tgz

How to use unpack_release.sh
----------------------------

1. Check out a vanilla Android L aosp tree.  Run the "repo init" command.

2. "repo sync -j8" to fetch the aosp tree.

3. Run unpack_release.sh by specifying the aosp tree location and the release
tarball name, e.g.:
unpack_release.sh android5.0_14.4take1.tgz ~/projects/l-aosp

4. "lunch" and select a build target.

5. "make -jN" per usual.
