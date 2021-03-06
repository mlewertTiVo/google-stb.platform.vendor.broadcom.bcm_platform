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
#
#		as example, 'sec-filer' can be set to "/projects/bse_secure_dev/${USER}/<some-path>"
#		which would be the top level repository where you have extracted the security
#		related content needed for the build. at the present time, it is expected that
#		under this 'sec-filer' path, you will have extracted both the 'security.git' and
#		the 'playready.git', such that you have:
#
#		    - "/projects/bse_secure_dev/${USER}/<some-path>/security"
#		    - "/projects/bse_secure_dev/${USER}/<some-path>/playready".
#
# you MAY provide:
#
#	1)	a bunch of tunable options listed on the 'usage'.
#
#
# when running this script, the security related code will never leave the secure filer location
# (ie 'sec-filer' option) specified when, the companion android view you are using may be located on
# a non secure filer such as a local build machine to speed up builds, the script will take care of
# properly linking into the tree the secure components on the filer for the build purpose.
#
#
# by default each time you run this script and so ask to build the security libraries, the libraries
# will be rebuilt from clean. unless you specified the --skip-build option, in which case no build
# will take place.
#
# the supporting android environment will not be rebuilt from clean however, unless you are explicitely
# asking for it using one of the options: 'force-clean' or 'force-clean-refsw'.  the latter being a subset
# of the former.
#
#
# example usage:
#
#	first time:
#
#		./sec_builder --sec-filer <path-to> --configure
#
#	subsequent rebuild, with same code, examples of what action can be asked:
#
#		[reset environment] ./sec_builder --sec-filer <path-to> --configure --reset
#		[re-run the build ] ./sec_builder --sec-filer <path-to>
#		[force refsw clean] ./sec_builder --sec-filer <path-to> --force-clean-refsw
#		[update codebase  ] ./sec_builder --sec-filer <path-to> --update
#

oops="oops"
security_root="security"
playready_root="playready"
show_tree_depth=3
BCM_VENDOR_STB_ROOT="vendor/broadcom"

# you can change the auto-magic profile selection here if desired, but there is really no need to.
#
# note: if changing those, you have to make sure there is a valid 'device/broadcom/custom' profile
#       associated with your selection.
#
plat_target="97252 D0 C"
plat_profile="BCM97252CSECU"
plat_profile_verifier="BCM97252C_43602"
#
# -- end profile (auto-magic) selection.

verbose=
update=
sec_filer=
configure=
reset=
force_clean=
force_clean_refsw=
skip_build=
skip_verify=
debug=
show_tree=
secliblist=
only_build_secu=

function usage {
	echo "sec_builder --sec-filer <path-to>"
	echo "            [--update] [--configure] [--reset] [--force-clean|--force-clean-refsw]"
	echo "            [--skip-build|--skip-verify] [--debug] [--show-tree-only]"
	echo "            [--only-build-secu]"
        echo "            [--show-tree-depth <depth>] [--verbose] [--help|-h]"
	echo ""
	echo "mandatory parameter(s):"
	echo ""
	echo "   --sec-filer <path-to>        : <path-to> is the fully qualified filer path to your"
	echo "                                  secure code view root.  the security/playready repos"
	echo "                                  are found directly under this root."
	echo ""
	echo "options:"
	echo ""
	echo "   --seclib-list <path-to>      : <path-to> is the fully qualified path to your security library mapping"
	echo "                                  file, containing the list (one per row) of mapped <as-built>=<in-android-tree>"
	echo "                                  modules, where <as-built> is the library built by this script and <in-android-tree>"
	echo "                                  is the final resting location of the same library in the android prebuilt tree."
	echo "                                  this is used to compare the checked-in libraries vs the built one."
	echo ""
	echo "   --configure                  : whether to configure the source setup secure/non-secure before building,"
	echo "                                  typically only needed once per code sync cycle."
	echo ""
	echo "   --reset                      : whether to force reset the source setup secure/non-secure prior to recreating,"
	echo "                                  all links.  typically needed on a non-clean environment if using --configure."
	echo ""
	echo "   --update                     : whether to update the environment (repo sync/git pull) before building."
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
	echo "   --debug                      : run the security libraries build in debug mode (default: retail)."
	echo ""
	echo "   --show-tree-only             : just shows the current state of the source tree and bail."
	echo ""
	echo "   --show-tree-depth            : the depth in terms of git log entries to dump from top of tree, defaults to 3."
	echo ""
	echo "   --skip-verify                : skip the verification step at the end."
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
	if [ -L "$1/${BCM_VENDOR_STB_ROOT}/refsw/secsrcs" ]; then
		liens=$(($liens + 1))
	fi
	if [ -L "$1/${BCM_VENDOR_STB_ROOT}/refsw/prsrcs" ]; then
		liens=$(($liens + 1))
	fi
	if [ "$liens" == "2" ]; then
		if [ "$reset" == "1" ]; then
			# remove all existing links, reset the refsw code
			cd $1/${BCM_VENDOR_STB_ROOT}/refsw
			rm $1/${BCM_VENDOR_STB_ROOT}/refsw/secsrcs
			rm $1/${BCM_VENDOR_STB_ROOT}/refsw/prsrcs
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
	cd $1/${BCM_VENDOR_STB_ROOT}/refsw
	if [ -L "secsrcs" ]; then
		rm secsrcs
	fi
	ln -s $3 secsrcs
	if [ -L "prsrcs" ]; then
		rm prsrcs
	fi
	ln -s $4 prsrcs
	cd $1
}

# $1 - android tree root.
# $2 - 'security' subtree on secure filer.
# $3 - 'playready' subtree on secure filer.
function mettre_code_a_jour {
	cd $1/${BCM_VENDOR_STB_ROOT}/refsw
	git checkout -f
	cd $1
	repo sync -j12
	cd $2
	git pull --rebase
	cd $3
	git pull --rebase
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
		./${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/plat-droid.py ${plat_target} profile ${plat_profile}
		source ./build/envsetup.sh
		# build the user side libraries, we need to build a system first to get
		# the android objects setup and the refsw/nexus libs.
		lunch ${plat_profile}-eng
		if [ "$force_clean" == "1" ]; then
			make -j12 clean
		else
			if [ "$force_clean_refsw" == "1" ]; then
				make -j12 clean_refsw
			fi
		fi
		if [ "$only_build_secu" != "1" ]; then
			make -j12
		fi
		# setup a 'fake' NDK equivalent to allow the security build to find out the
		# native libs it links against.
		mkdir -p out/target/product/bcm_platform/sysroot/usr/lib
		cp -faR out/target/product/bcm_platform/obj/lib/* out/target/product/bcm_platform/sysroot/usr/lib/
		# now do the real security build as an incremental build.
		if [ "$debug" == "1" ]; then
			export B_REFSW_DEBUG=y
		else
			export B_REFSW_DEBUG=n
			export B_REFSW_DEBUG_COMPACT_ERR=n
		fi
		make -j12 clean_security_user
		make -j12 security_user
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
		OLDIFS=$IFS; IFS='=' var=(${seclib_mapping[$ix]}); IFS=$OLDIFS;
		if [[ "${var[0]}" != "" &&  "${var[1]}" != "" ]]; then
			built=$1/out/target/product/bcm_platform/sysroot/${var[0]}
			temp=$1/${BCM_VENDOR_STB_ROOT}/bcm_platform/${var[1]}
			# strip trailing '\n'
			original=$(echo $temp | sed -e 's/\n//g')
			# optionally strip the MARKER (used for debug vs retail).
			temp=$original
			if [ "$debug" == "1" ]; then
				original=$(echo $temp | sed -e 's/MARKER/retail/g')
			else
				original=$(echo $temp | sed -e 's/MARKER/debug/g')
			fi

			if [[ -f "$built" && -f "$original" ]]; then
				if diff "$original" "$built" >/dev/null; then
					echo "unchanged: ${var[1]}"
				else
					echo "DIFFERS: ${var[0]}"
					if [ ! "$skip_verify" == "1" ]; then
						# copy the generated lib in the final resting location for the
						# build verification step.
						cp $built $original
					fi
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
	cd $1/${BCM_VENDOR_STB_ROOT}/refsw
	echo ""
	echo "*** code for $1/${BCM_VENDOR_STB_ROOT}/refsw"
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

# $1 - android tree root.
function verifier {
	if [ "$skip_verify" == "1" ]; then
		que_faites_vous "verification skipped on demand"
	else
		cd $1
		./${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/plat-droid.py ${plat_target} profile ${plat_profile_verifier}
		source ./build/envsetup.sh
		lunch ${plat_profile_verifier}-eng
		make -j12 clean_refsw
		make -j12
	fi
}

# command line parsing.
while [ "$1" != "" ]; do
	case $1 in
		--sec-filer)			shift
						sec_filer=$1
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
		--skip-verify)			skip_verify=1
						;;
		--debug)			debug=1
						;;
		--show-tree-only)		show_tree=1
						;;
		--show-tree-depth)		shift
						show_tree_depth=$1
						;;
		--only-build-secu)		only_build_secu=1
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

que_faites_vous "locating top-of-tree android extract."
fqn_root=$(ou_suis_je)
if [ "$fqn_root" == "$oops" ]; then
	rejouez "not appear to be running within a valid android tree."
fi
que_faites_vous "... located at $fqn_root."

if [ "$secliblist" == "" ]; then
	# use defaut one for this code tree.
	secliblist=$fqn_root/${BCM_VENDOR_STB_ROOT}/bcm_platform/tools/seclibs.map
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

# build a clean refsw based image from the reference board now to see if the
# libraries generated do make sense.
que_faites_vous "now running final verification build..."
verifier $fqn_root
