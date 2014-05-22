#############################################################################
#    (c)2012 Broadcom Corporation
# 
# This program is the proprietary software of Broadcom Corporation and/or its licensors,
# and may only be used, duplicated, modified or distributed pursuant to the terms and
# conditions of a separate, written license agreement executed between you and Broadcom
# (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
# no license (express or implied), right to use, or waiver of any kind with respect to the
# Software, and Broadcom expressly reserves all rights in and to the Software and all
# intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
# HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
# NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.  
#  
# Except as expressly set forth in the Authorized License,
#  
# 1.     This program, including its structure, sequence and organization, constitutes the valuable trade
# secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
# and to use this information only in connection with your use of Broadcom integrated circuit products.
#  
# 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS" 
# AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR 
# WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO 
# THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES 
# OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, 
# LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION 
# OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF 
# USE OR PERFORMANCE OF THE SOFTWARE.
# 
# 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS 
# LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR 
# EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR 
# USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF 
# THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT 
# ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE 
# LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF 
# ANY LIMITED REMEDY.
#
# $brcm_Workfile: Makefile.am $
# $brcm_Revision: 1 $
# $brcm_Date: 4/25/12 6:05p $
# 
# Module Description:
# 
# Revision History:
# 
# $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/ntfs3g/src/Makefile.am $
# 
# 1   4/25/12 6:05p ttrammel
# SWANDROID-55: Check in ntfs filesystem support for usbsticks.
# 
#############################################################################

MAINTAINERCLEANFILES = $(srcdir)/Makefile.in

if FUSE_INTERNAL
FUSE_CFLAGS = -I$(top_srcdir)/include/fuse-lite
FUSE_LIBS   =
else
FUSE_CFLAGS = $(FUSE_MODULE_CFLAGS)
FUSE_LIBS   = $(FUSE_MODULE_LIBS)
endif

bin_PROGRAMS	 = ntfs-3g.probe
rootbin_PROGRAMS = ntfs-3g
rootsbin_DATA 	 = #Create directory
man_MANS	 = ntfs-3g.8 ntfs-3g.probe.8

ntfs_3g_LDADD    = $(FUSE_LIBS) $(top_builddir)/libntfs-3g/libntfs-3g.la
if REALLYSTATIC
ntfs_3g_LDFLAGS  = $(AM_LDFLAGS) -all-static
endif
ntfs_3g_CFLAGS   =			\
	$(AM_CFLAGS) 			\
	-DFUSE_USE_VERSION=26 		\
	$(FUSE_CFLAGS) 			\
	-I$(top_srcdir)/include/ntfs-3g
ntfs_3g_SOURCES  = ntfs-3g.c

ntfs_3g_probe_LDADD 	= $(top_builddir)/libntfs-3g/libntfs-3g.la
if REALLYSTATIC
ntfs_3g_probe_LDFLAGS	= $(AM_LDFLAGS) -all-static
endif
ntfs_3g_probe_CFLAGS  	= $(AM_CFLAGS) -I$(top_srcdir)/include/ntfs-3g
ntfs_3g_probe_SOURCES 	= ntfs-3g.probe.c

if RUN_LDCONFIG
install-exec-hook:
	$(LDCONFIG)
endif

if ENABLE_MOUNT_HELPER
install-exec-local:	install-rootbinPROGRAMS
	$(MKDIR_P) "$(DESTDIR)/sbin"
	$(LN_S) -f "$(rootbindir)/ntfs-3g" "$(DESTDIR)/sbin/mount.ntfs-3g"
endif

install-data-local:	install-man8
	$(LN_S) -f ntfs-3g.8 "$(DESTDIR)$(man8dir)/mount.ntfs-3g.8"

uninstall-local:
	$(RM) -f "$(DESTDIR)$(man8dir)/mount.ntfs-3g.8"
if ENABLE_MOUNT_HELPER
	$(RM) -f "$(DESTDIR)/sbin/mount.ntfs-3g"
endif
