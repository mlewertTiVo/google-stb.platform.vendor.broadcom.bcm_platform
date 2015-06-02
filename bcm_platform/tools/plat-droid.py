#!/usr/bin/env python
# best tool since sliced bread - generates an android device configuration
# from the refsw plat defines needed, the android device fully inherits from
# 'bcm_platform' which continues to be the reference and what is seen on
# target, but it turns on the needed knobs for refsw compatibility of the
# target.
#
# this will create a ./device/broadcom/<chip-num><chip-rev><board-type>
# which inherits fully from ./device/broadcom/bcm_platform.
#
# next, user can run the standard android build commands and select the
# target device which has been created automagically.
#
# the tool relies on the 'plat' tool from the refsw, so it is guaranteed to
# be able to support any device so long a refsw platform exists for it, which
# should be always the case.
#
import re, sys, os
from subprocess import call,check_output,STDOUT
from stat import *

# debug this script.
verbose=0

def parse_and_select(l):
	selected = False
	data = re.findall('NEXUS_PLATFORM', l)
	if len(data) > 0:
		selected = True
		if verbose:
			print 'selecting: %s' % l
	data = re.findall('BCHP_VER', l)
	if len(data) > 0:
		data = re.findall('=', l)
		if len(data) > 0:
			selected = True
			if verbose:
				print 'selecting: %s' % l
	data = re.findall('B_REFSW_', l)
	if len(data) > 0:
		data = re.findall('=[yn]', l)
		if len(data) > 0:
			selected = True
			if verbose:
				print 'selecting: %s' % l
	data = re.findall('NEXUS_USE_', l)
	if len(data) > 0:
		data = re.findall('=[yn]', l)
		if len(data) > 0:
			selected = True
			if verbose:
				print 'selecting: %s' % l
	if verbose and (selected == False):
		print 'ignoring: %s' % l
	return selected

# empty existing generated repo is applicable or create the device root for it.
def rmdir_then_mkdir(d):
	if os.path.exists(d):
		for root, dirs, files in os.walk(d, topdown=False):
			for name in files:
				os.remove(os.path.join(root, name))
	else:
		os.makedirs(d)

def rmdir_device_root(d):
	os.rmdir(d)

# header generation for modules we create.
def write_header(s, d):
	os.write(s, "# copyright 2014 - broadcom canada ltd.\n#\n")
	os.write(s, "# warning: auto-generated module, edit at your own risks...\n#\n\n")
	os.write(s, "# this configuration is for device %s\n\n" % d)

# how you should use this.
def plat_droid_usage():
	print 'usage: plat-droid.py <platform> <chip-rev> <board-type> [redux|aosp|nfs|profile <name>] [spoof <cust-device> <cust-variant>]'
	print '\t<platform>    - the BCM platform number to build for, eg: 97252, 97445, ...'
	print '\t<chip-rev>    - the BCM chip revision of interest, eg: A0, B0, C1, ...'
	print '\t<board-type>  - the target board type, eg: SV, C'
	print '\t[redux|aosp|nfs|profile] (note: mutually exclusive)'
	print '\t              - redux: redux platform support image (minimal android)'
        print '\t              - aosp: AOSP feature set and integration exclusively'
        print '\t              - nfs: booting using nfs exclusively - no formal support, at your own risks'
        print '\t              - profile: device specific profile override, must pass a valid profile <name>'
	print '\t[spoof]'
	print '\t              - when set, both a cust-device corresponding to the customer android device and a cust-variant'
	print '\t                corresponding to the customer android variant must be present and valid.'
	print '\t              - cust-device: the customer device we are spoofing.'
        print '\t              - cust-variant: the customer device variant we are spoofing'
	print '\n'
	sys.exit(0)

# read input and bail if the format is not what we expect.
input = len(sys.argv)
if input < 4 :
	plat_droid_usage()
if input > 4 :
	target_option=str(sys.argv[4]).upper()
	if target_option == "PROFILE":
		if input < 6:
			plat_droid_usage()
		else:
			target_profile=str(sys.argv[5])
		if input > 6:
			spoof_device=str(sys.argv[7])
			spoof_variant=str(sys.argv[8])
		else:
			spoof_device='nope'
	if target_option == "SPOOF":
		if input < 6:
			plat_droid_usage()
		else:
			spoof_device=str(sys.argv[5])
			spoof_variant=str(sys.argv[6])
else:
	target_option='nope'
	target_profile='nope'
	spoof_device='nope'
	spoof_variant='nope'

chip=str(sys.argv[1]).upper()
revision=str(sys.argv[2]).upper()
boardtype=str(sys.argv[3]).upper()
nexus_platform_selected=''

# create android cruft.
androiddevice='%s%s%s' % (chip, revision, boardtype)
androidrootdevice='%s%s%s' % (chip, revision, boardtype)
if target_option == "AOSP" or target_option == "REDUX" or target_option == "NFS":
	androiddevice='%s_%s' % (androiddevice, target_option)
if target_option == "PROFILE":
        custom_directory="./device/broadcom/custom/%s/%s" %(androiddevice, target_profile)
	custom_target_settings="%s/settings.mk" %(custom_directory)
	custom_target_pre_settings="%s/pre_settings.mk" %(custom_directory)
	androiddevice='%s_%s' % (androiddevice, target_profile)
	if not os.path.exists(custom_directory):
		print '\n*** your custom profile "%s/%s" does not appear to be a valid one.\n' %(androiddevice, target_profile)
		plat_droid_usage()
	if not os.access(custom_target_pre_settings, os.F_OK):
		custom_target_pre_settings='nope'
if spoof_device == 'nope':
	devicedirectory='./device/broadcom/bcm_%s/' % (androiddevice)
else:
	androiddevice='%s' % (spoof_variant)
	devicedirectory='./device/%s/%s/' % (spoof_device, androiddevice)
if verbose:
	print 'creating android device: %s, in directory: %s' % (androiddevice, devicedirectory)
# minimum modules for android device build created by this script.
vendorsetup="vendorsetup.sh"
androidproduct="AndroidProducts.mk"
if spoof_device == 'nope':
	target="bcm_%s.mk" % (androiddevice)
else:
	target="full_%s.mk" % (androiddevice)
boardconfig="BoardConfig.mk"
# clean old config, do this here so even if we fail later on we do not
# keep around some old stuff.
rmdir_then_mkdir(devicedirectory)

# get the toolchain expected for kernel image and modules build
run_toolchain='bash -c "cat ./kernel/rootfs/toolchain"'
if verbose:
	print run_toolchain
lines = check_output(run_toolchain,shell=True).splitlines()
kerneltoolchain="${ANDROID}/prebuilts/gcc/linux-x86/arm/stb/%s/bin" % lines[0].rstrip()

# get the toolchain expected for building BOLT and make sure it does not
# diverge from kernel toolchain as we want only a single verion in Android
# tree at any point in time
run_toolchain='bash -c "cat ./vendor/broadcom/bolt/config/toolchain"'
if verbose:
	print run_toolchain
boltlines = check_output(run_toolchain,shell=True).splitlines()

if boltlines[0].rstrip() != lines[0].rstrip():
	print '\nwarning: Expected BOLT toolchain (%s) is not the same as kernel toolchain (%s).' % (boltlines[0].rstrip(), lines[0].rstrip())
	print 'You can still proceed with the build.  Contact Android BOLT maintainer to follow-up with diverging toolchain version.'

# run the refsw plat tool to get the generated versions of the config.
run_plat='bash -c "source ./vendor/broadcom/refsw/BSEAV/tools/build/plat %s %s %s"' % (chip, revision, boardtype)
if verbose:
	print run_plat
refsw_configuration_selected=''
lines = check_output(run_plat,stderr=STDOUT,shell=True).splitlines()
for line in lines:
	line = line.rstrip()
	if parse_and_select(line):
		refsw_configuration_selected='%s\nexport %s' % (refsw_configuration_selected, line)
		nexus_platform = re.findall('NEXUS_PLATFORM', line)
		if len(nexus_platform) > 0:
			matching_groups = re.match(r"(\w+)\=(\w+)", line)
			nexus_platform_selected = matching_groups.group(2)
if len(nexus_platform_selected) > 0:
	refsw_configuration='export PLATFORM=%s' % nexus_platform_selected
else:
	refsw_configuration=''

# sanity.
if len(refsw_configuration_selected) <= 0 or len(refsw_configuration) <= 0:
	print '\nerror: refsw configuration for %s turned out empty - no android configuration issued.\n' % androiddevice
	rmdir_device_root(devicedirectory)
	plat_droid_usage()
else:
	refsw_configuration_selected='%s\n%s' % (refsw_configuration_selected, refsw_configuration)
	if verbose:
		print '\nrefsw configuration gathered: %s' % refsw_configuration_selected

# now generate all the needed modules for the android device build.
f='%s%s' % (devicedirectory, vendorsetup)
s=os.open(f, os.O_WRONLY|os.O_CREAT)
write_header(s, androiddevice)
# note: additional combo can be added if need be (ie: -user)
if spoof_device == 'nope':
	os.write(s, "add_lunch_combo bcm_%s-eng\n" % androiddevice)
	os.write(s, "add_lunch_combo bcm_%s-userdebug\n" % androiddevice)
	os.write(s, "add_lunch_combo bcm_%s-user\n" % androiddevice)
else:
	os.write(s, "add_lunch_combo full_%s-eng\n" % androiddevice)
os.close(s);

f='%s%s' % (devicedirectory, androidproduct)
s=os.open(f, os.O_WRONLY|os.O_CREAT)
write_header(s, androiddevice)
if spoof_device == 'nope':
	os.write(s, "PRODUCT_MAKEFILES := $(LOCAL_DIR)/bcm_%s.mk\n" % androiddevice)
else:
	os.write(s, "PRODUCT_MAKEFILES := $(LOCAL_DIR)/full_%s.mk\n" % androiddevice)
os.close(s);

if spoof_device != 'nope':
	f='%s%s' % (devicedirectory, boardconfig)
	s=os.open(f, os.O_WRONLY|os.O_CREAT)
	write_header(s, androiddevice)
	os.write(s, "include device/broadcom/bcm_platform/BoardConfig.mk\n")
	os.close(s);

f='%s%s' % (devicedirectory, target)
s=os.open(f, os.O_WRONLY|os.O_CREAT)
write_header(s, androiddevice)
# this needs to be included before we absorb the rest of the platform config since
# the latter needs some of those definitions.
os.write(s, "# start of refsw gathered configuration\n\n")
os.write(s, "%s\n\n" % refsw_configuration_selected)
os.write(s, "# end of refsw gathered config...\n")
root_pre_settings="./device/broadcom/custom/%s/root/pre_settings.mk" %(androidrootdevice)
if os.access(root_pre_settings, os.F_OK):
	root_pre_settings="\ninclude device/broadcom/custom/%s/root/pre_settings.mk" %(androidrootdevice)
	os.write(s, root_pre_settings)
if target_option == "PROFILE" and custom_target_pre_settings != 'nope':
	if os.access(custom_target_pre_settings, os.F_OK):
		os.write(s, "\n\n# CUSTOM 'pre' setting tweaks...\n")
		os.write(s, "include %s\n" % custom_target_pre_settings)
if target_option == "REDUX":
	os.write(s, "\n\n# REDUX target set...\n")
	os.write(s, "include device/broadcom/common/target_redux.mk")
os.write(s, "\n\ninclude device/broadcom/bcm_platform/bcm_platform.mk")
root_settings="./device/broadcom/custom/%s/root/settings.mk" %(androidrootdevice)
if os.access(root_settings, os.F_OK):
	root_settings="\ninclude device/broadcom/custom/%s/root/settings.mk" %(androidrootdevice)
	os.write(s, root_settings)
if target_option == "AOSP":
	os.write(s, "\n\n# AOSP setting tweaks...\n")
	os.write(s, "include device/broadcom/common/settings_aosp.mk")
if target_option == "NFS":
	os.write(s, "\n\n# NFS setting tweaks...\n")
	os.write(s, "include device/broadcom/common/settings_nfs.mk")
if target_option == "PROFILE":
	if os.access(custom_target_settings, os.F_OK):
		os.write(s, "\n\n# CUSTOM setting tweaks...\n")
		os.write(s, "include %s\n" % custom_target_settings)
if spoof_device != "nope":
	spoof_settings="./device/%s/config/settings.mk" %(spoof_device)
	if os.access(spoof_settings, os.F_OK):
		spoof_settings="include device/%s/config/settings.mk" %(spoof_device)
		os.write(s, "\n\n# SPOOF setting tweaks...\n")
		os.write(s, spoof_settings)
else:
	os.write(s, "\n\nPRODUCT_NAME := bcm_%s\n" % androiddevice)
os.write(s, "\n\n# exporting toolchains path for kernel image+modules\n")
os.write(s, "export PATH := %s:${PATH}\n" % kerneltoolchain)
os.close(s);

# yeah! happy...
print '\n'
print 'congratulations! device [bcm_]%s configured, you may proceed with android build...' % androiddevice
print '\t1) source ./build/envsetup.sh'
print '\t2) lunch [bcm_]%s-[eng|userdebug|user].' % androiddevice
print '\n'
