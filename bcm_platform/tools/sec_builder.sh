#!/bin/bash
set -e

#
# this script is used to generate the security libraries for the android integration.
#
# the script will auto detect the root of your android workspace knowing its location
# within the tree.
#
# you MUST provide:
#
#	1)	the 'sec-filer' fully qualified path, which is the location where the
#		security source code resides.
#		as example, 'sec-filer' can be set to "/projects/bse_secure_dev/${USER}/<root>"
#		where <root> is the top level repository where you have extracted the security
#		related content needed for the build. at the present time, it is expected that
#		under <root> you will have extracted both the 'security.git' and
#		the 'playready.git'.
#
# you MAY provide:
#
#	1)	the 'hsm-srcs-list' input file (fully qualified path, which is the list of
#		all hsm modules that are needed for the hsm build which the script will
#		decrypt.  the hsm file format is a single *.gpg file name per row which
#		name starts at the root of the hsm code tree in the resfw view of the world.
#		if none provided, the default one checked-in in the source tree will be used.
#
#	2)	the 'keydir' fully qualified path, which is the location where you have
#		stored the key file and perl script necessary for decrypting the hsm sources.
#		if none provided, the default /home/${USER}/keydir is expected.
#
#	3)	a bunch of tunable options listed on the 'usage'.
#
# the security related code will never leave the secure filer location specified when
# running this script, the android view you are using may be located on a non secure
# filer such as a local build machine.
#
# note: by default each time you run this script and so ask to build the security libraries,
#	(unless you specified the --skip-build option), the libraries (but not the supporting
#       android/refsw environment) will be rebuilt from clean.  use the --force-clean option
#       to also clean out the supporting environment prior to rebuilding the security libraries.
#
# example usage:
#
#	first time:
#		./sec_builder --sec-filer <path-to> --keydir <path-to> --hsm-srcs-list <path-to> --configure
#
#	subsequent rebuild, with same code:
#		1) ./sec_builder --sec-filer <path-to> --keydir <path-to> --hsm-srcs-list <path-to> --configure --reset
#		2) ./sec_builder --sec-filer <path-to> --keydir <path-to> --hsm-srcs-list <path-to>
#		3) ./sec_builder --sec-filer <path-to> --keydir <path-to> --hsm-srcs-list <path-to> --force-clean
#
#	subsequent rebuild, with updated code:
#		./sec_builder --sec-filer <path-to> --keydir <path-to> --hsm-srcs-list <path-to> --update
#

oops="oops"
security_root="security"
playready_root="playready"
show_tree_depth=3

verbose=
update=
sec_filer=
keydir=
hsmsrcs=
configure=
reset=
force_clean=
force_clean_refsw=
skip_build=
hsm_debug=
show_tree=
secliblist=

function usage {
	echo "sec_builder --sec-filer <path-to> [--keydir <path-to>] [--hsm-srcs-list <path-to>]"
	echo "            [--update] [--configure] [--reset] [--force-clean|--force-clean-refsw]"
	echo "            [--skip-build] [--hsm-debug] [--show-tree-only] [--show-tree-depth <depth>]"
	echo "            [--verbose] [--help|-h]"
	echo ""
	echo "mandatory parameter(s):"
	echo ""
	echo "   --sec-filer <path-to>        : <path-to> is the fully qualified filer path to your"
	echo "                                  secure code view root.  the security/playready repos"
	echo "                                  are found directly under this root."
	echo ""
	echo "options:"
	echo ""
	echo "   --keydir <path-to>           : <path-to> is the fully qualified path to your key directory"
	echo "                                  containing the key to decruypt hsm code as well as the perl"
	echo "                                  script to be used for decrypting. when not specified, defaults"
	echo "                                  to '/home/${USER}/keydir'"
	echo ""
	echo "   --hsm-srcs-list <path-to>    : <path-to> is the fully qualified path to your hsm source list file"
	echo "                                  containing the list (one per row) of all files you want to decrypt"
	echo "                                  as part of the hsm build. when not specified, defaults to the version"
	echo "                                  checked in on this code tree."
	echo ""
	echo "   --seclib-list <path-to>      : <path-to> is the fully qualified path to your security library mapping"
	echo "                                  file, containing the list (one per row) of mapped <as-built>=<in-android-tree>"
	echo "                                  modules, where <as-built> is the library built by this script and <in-android-tree>"
	echo "                                  is the final resting location of the same library in the android prebuilt tree."
	echo ""
	echo "   --configure                  : whether to configure the source setup secure/non-secure before building,"
	echo "                                  typically only needed once per code sync cycle, also determines if we need"
	echo "                                  decrypt the hsm modules (no need to decrypt if nothing changed)."
	echo ""
	echo "   --reset                      : whether to force reset the source setup secure/non-secure prior to recreating,"
	echo "                                  all links.  typically needed on a non-clean environment if using --configure."
	echo ""
	echo "   --update                     : whether to update the environment (git sync) before building."
	echo ""
	echo "   --force-clean                : whether to force clean the supporting android environment, this does not apply to"
	echo "                                  the security libraries, since those are *always* clean."
	echo ""
	echo "   --force-clean-refsw          : whether to force clean the supporting refsw environment alone, this does not apply to"
	echo "                                  the security libraries, since those are *always* clean.  this is a subset of --force-clean"
	echo ""
	echo "   --verbose                    : be verbose about what we do."
	echo ""
	echo "   --skip-build                 : do not actually run the builds, just skip them."
	echo ""
	echo "   --hsm-debug                  : run the hsm build in debug mode (default: retail)."
	echo ""
	echo "   --show-tree-only             : just shows the current state of the source tree and bail."
	echo ""
	echo "   --show-tree-depth            : the depth in terms of git log entries to dump from top of tree, defaults to 3."
	echo ""
}

# $1 - string explaining action being taken.
function que_faites_vous {
	if [ "$verbose" == "1" ]; then
		echo ""
		echo $1
	fi
}

# $1 - fatal error description.
function rejouez {
	echo ""
	echo "*** error: $1"
	echo ""
	usage
	exit 1
}

function ou_suis_je {
	la=$(cd $(dirname $0) && pwd)
	# cheat - we know where we 'should be' in relation to root, go there.
	la=$(dirname $la)
	la=$(dirname $la)
	la=$(dirname $la)
	la=$(dirname $la)
	# sanity, does not hurt...
	chk=$la
	chk+="/"
	chk+="bionic"
	if [ ! -d "$chk" ]; then
		echo $oops
	else
		echo $la
	fi
}

# $1 - android tree root.
function verifier_liens {
	liens=0
	if [ -L "$1/vendor/broadcom/refsw/secsrcs" ]; then
		liens=$(($liens + 1))
	fi
	if [ -L "$1/vendor/broadcom/refsw/prsrcs" ]; then
		liens=$(($liens + 1))
	fi
	if [ -L "$1/vendor/broadcom/refsw/magnum/portinginterface/hsm" ]; then
		liens=$(($liens + 1))
	fi
	if [ "$liens" == "3" ]; then
		if [ "$reset" == "1" ]; then
			# remove all existing links, reset the refsw code to grab the
			# hsm again.
			cd $1/vendor/broadcom/refsw
			rm $1/vendor/broadcom/refsw/secsrcs
			rm $1/vendor/broadcom/refsw/prsrcs
			rm $1/vendor/broadcom/refsw/magnum/portinginterface/hsm
			git checkout -f
		else
			rejouez "environment already setup; add --reset option with --configure, or drop --configure."
		fi
	fi
}

# $1 - android tree root.
# $2 - secure filer root.
# $3 - 'security' subtree on secure filer.
# $4 - 'playready' subtree on secure filer.
function mettre_en_place {
	# note: link the secure code into the non-secure tree, do not move the code out of the secure filer.
	#       copy the 'hsm' code to be decrypted into the secure filer location and link that back to the
	#       non secure tree.
	cd $1/vendor/broadcom/refsw
	if [ -L "secsrcs" ]; then
		rm secsrcs
	fi
	ln -s $3 secsrcs
	if [ -L "prsrcs" ]; then
		rm prsrcs
	fi
	ln -s $4 prsrcs
	cd $1
	if [ -d "$2/hsm" ]; then
		rm -rf $2/hsm
	fi
	cp -faR $1/vendor/broadcom/refsw/magnum/portinginterface/hsm $2/hsm
	rm -rf $1/vendor/broadcom/refsw/magnum/portinginterface/hsm
	ln -s $2/hsm $1/vendor/broadcom/refsw/magnum/portinginterface/hsm
}

# $1 - android tree root.
# $2 - 'security' subtree on secure filer.
# $3 - 'playready' subtree on secure filer.
function mettre_code_a_jour {
	cd $1/vendor/broadcom/refsw
	git checkout -f
	cd $1
	repo sync -j12
	cd $2
	git pull --rebase
	cd $3
	git pull --rebase
}

# $1 - security filer location.
# $2 - keydir location.
# $3 - hsm source file.
function decrypter_hsm {
	readarray hsm_a_decrypter < $3
	count=${#hsm_a_decrypter[@]}
	ix=0
	while [ "$ix" -lt "$count" ]; do
		perl $2/keygpg.pl $1/hsm/${hsm_a_decrypter[$ix]}
		ix=$(($ix + 1))
	done
}

# $1 - android tree root.
function compiler {
	if [ "$skip_build" == "1" ]; then
		que_faites_vous "build skipped on demand"
	else
		cd $1
		# generate the device with the profiles we need to build.
		#
		# TODO: make the platform configured.
		#
		./vendor/broadcom/bcm_platform/tools/plat-droid.py 97252 D0 C profile secu
		./vendor/broadcom/bcm_platform/tools/plat-droid.py 97252 D0 C profile seck
		source ./build/envsetup.sh
		# build the user side libraries, we need to build a system first to get
		# the android objects setup and the refsw/nexus libs.
		lunch bcm_97252D0C_secu-eng
		if [ "$force_clean" == "1" ]; then
			make -j12 clean
		else
			if [ "$force_clean_refsw" == "1" ]; then
				make -j12 clean_refsw
			fi
		fi
		make -j12
		# setup a 'fake' NDK equivalent to allow the security build to find out the
		# native libs it links against.
		mkdir -p out/target/product/bcm_platform/sysroot/usr/lib
		cp -faR out/target/product/bcm_platform/obj/lib/* out/target/product/bcm_platform/sysroot/usr/lib/
		# now do the real security build as an incremental build.
		make -j12 clean_security_user
		make -j12 security_user
		# build the kernel side libraries - note: allow notion of 'debug' vs 'retail' (is it really needed?)
		lunch bcm_97252D0C_seck-eng
		if [ "$hsm_debug" == "1" ]; then
			export B_REFSW_DEBUG=y
		fi
		make -j12 clean_security_kernel
		make -j12 security_kernel
	fi
}

# $1 - android tree root.
# $2 - security library mapping module.
function comparer {
	readarray seclib_mapping < $2
	count=${#seclib_mapping[@]}
	ix=0
	echo ""
	echo "reporting build results comparison between built and checked-in versions."
	echo ""
	# TODO: there is a debug/retail version of libcmndrmprdy.so which we do not seem to care for.  investigate...
	#
	while [ "$ix" -lt "$count" ]; do
		IFS='=' var=(${seclib_mapping[$ix]});
		if [[ "${var[0]}" != "" &&  "${var[1]}" != "" ]]; then
			built=$1/out/target/product/bcm_platform/sysroot/${var[0]}
			temp=$1/vendor/broadcom/bcm_platform/${var[1]}
			# strip trailing '\n'
			original=$(echo $temp | sed -e 's/\n//g')
			# optionally strip the MARKER (used for debug vs retail).
			temp=$original
			if [ "$hsm_debug" == "1" ]; then
				original=$(echo $temp | sed -e 's/MARKER/retail/g')
			else
				original=$(echo $temp | sed -e 's/MARKER/debug/g')
			fi

			if [[ -f "$built" && -f "$original" ]]; then
				if diff "$original" "$built" >/dev/null; then
					echo "unchanged: ${var[1]}"
				else
					echo "DIFFERS: ${var[0]}"
				fi
			else
				echo "either original '$original', or built '$built', does not actually exists.  ignoring."
			fi
		fi
		ix=$(($ix + 1))
	done
}

# $1 - android tree root.
# $2 - sec-filer tree root.
function montre_code {
	depth=$show_tree_depth
	cd $1/vendor/broadcom/refsw
	echo ""
	echo "*** code for $1/vendor/broadcom/refsw"
	echo ""
	git log --pretty=oneline -n $depth
	cd $2/$security_root
	echo ""
	echo "*** code for $2/$security_root"
	echo ""
	git log --pretty=oneline -n $depth
	cd $2/$playready_root
	echo ""
	echo "*** code for $2/$playready_root"
	echo ""
	git log --pretty=oneline -n $depth
	echo ""
	cd $1
}

# command line parsing.
while [ "$1" != "" ]; do
	case $1 in
		--sec-filer)			shift
						sec_filer=$1
						;;
		--keydir)			shift
						keydir=$1
						;;
		--hsm-srcs-list)		shift
						hsmsrcs=$1
						;;
		--seclib-list)			shift
						secliblist=$1
						;;
		--verbose)			verbose=1
						;;
		--update)			update=1
						;;
		--configure)			configure=1
						;;
		--reset)			reset=1
						;;
		--force-clean)			force_clean=1
						;;
		--force-clean-refsw)		force_clean_refsw=1
						;;
		--skip-build)			skip_build=1
						;;
		--hsm-debug)			hsm_debug=1
						;;
		--show-tree-only)		show_tree=1
						;;
		--show-tree-depth)		shift
						show_tree_depth=$1
						;;
		-h | --help)			usage
						exit
						;;
		*)				rejouez "invalid option(s) provided, aborting."
	esac
	shift
done

# various validation and fallback attempt following.
if [ "$sec_filer" == "" ]; then
	rejouez "you must provide a security filer location."
fi

fqn_sec=$sec_filer
fqn_sec+="/"
fqn_sec+=$security_root

fqn_pr=$sec_filer
fqn_pr+="/"
fqn_pr+=$playready_root

que_faites_vous "verifying security resources are there."
if [ ! -d "$fqn_sec" ]; then
	rejouez "$fqn_sec is missing"
fi
if [ ! -d "$fqn_pr" ]; then
	rejouez "$fqn_pr is missing"
fi
if [ ! -d "$keydir" ]; then
	# attempt fallback to default expected location.
	keydir=/home/${USER}/keydir
	if [ ! -d "$keydir" ]; then
		rejouez "$keydir is missing"
	fi
fi

que_faites_vous "locating top-of-tree android extract."
fqn_root=$(ou_suis_je)
if [ "$fqn_root" == "$oops" ]; then
	rejouez "not appear to be running within a valid android tree."
fi
que_faites_vous "... located at $fqn_root."

if [ "$hsmsrcs" == "" ]; then
	# use defaut one for this code tree.
	hsmsrcs=$fqn_root/vendor/broadcom/bcm_platform/tools/hsm.srcs
fi
if [ ! -f "$hsmsrcs" ]; then
	rejouez "hsm source list '$hsmsrcs' does not exists"
fi

if [ "$secliblist" == "" ]; then
	# use defaut one for this code tree.
	secliblist=$fqn_root/vendor/broadcom/bcm_platform/tools/seclibs.map
fi
if [ ! -f "$secliblist" ]; then
	rejouez "security library mapping list '$secliblist' does not exists"
fi

# sync up the code before we build it.
if [ "$update" == "1" ]; then
	que_faites_vous "updating code per request."
	mettre_code_a_jour $fqn_root $fqn_sec $fqn_pr
	# want to re-run config after a sync.
	configure=1
fi

if [ "$show_tree" == "1" ]; then
	que_faites_vous "showing code tree history alone."
	montre_code $fqn_root $sec_filer
	# just show the code that would be built, and exit.
	exit
fi

# configure the tree if needed.
if [ "$configure" == "1" ]; then
	verifier_liens $fqn_root
	que_faites_vous "linking in security code.  patience..."
	mettre_en_place $fqn_root $sec_filer $fqn_sec $fqn_pr
	que_faites_vous "decrypting hsm sources for the build.  more patience..."
	decrypter_hsm $sec_filer $keydir $hsmsrcs
fi

# now build.
que_faites_vous "starting build of security libraries."
compiler $fqn_root

# issue statement as to what differs.
que_faites_vous "compare built libraries with existing ones."
comparer $fqn_root $secliblist

# show code built as reference.
que_faites_vous "showing code tree for this build."
montre_code $fqn_root $sec_filer
