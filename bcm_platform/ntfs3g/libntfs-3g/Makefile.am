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
# $brcm_Date: 4/25/12 5:52p $
# 
# Module Description:
# 
# Revision History:
# 
# $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/ntfs3g/libntfs-3g/Makefile.am $
# 
# 1   4/25/12 5:52p ttrammel
# SWANDROID-55: Add ntfs filesystem support for usbsticks.
# 
#############################################################################

MAINTAINERCLEANFILES = $(srcdir)/Makefile.in

if INSTALL_LIBRARY
rootlib_LTLIBRARIES=#Create directory
lib_LTLIBRARIES    = libntfs-3g.la
pkgconfig_DATA     = libntfs-3g.pc
else
noinst_LTLIBRARIES = libntfs-3g.la
endif

libntfs_3g_la_CFLAGS  = $(AM_CFLAGS) -I$(top_srcdir)/include/ntfs-3g

libntfs_3g_la_LDFLAGS = -version-info $(LIBNTFS_3G_VERSION) -no-undefined

if FUSE_INTERNAL
libntfs_3g_la_LIBADD  = $(top_builddir)/libfuse-lite/libfuse-lite.la
endif

libntfs_3g_la_SOURCES =	\
	attrib.c 	\
	attrlist.c 	\
	bitmap.c 	\
	bootsect.c 	\
	collate.c 	\
	compat.c 	\
	compress.c 	\
	debug.c 	\
	device.c 	\
	dir.c 		\
	index.c 	\
	inode.c 	\
	lcnalloc.c 	\
	logfile.c 	\
	logging.c 	\
	mft.c 		\
	misc.c 		\
	mst.c 		\
	runlist.c 	\
	security.c 	\
	unistr.c 	\
	volume.c

if NTFS_DEVICE_DEFAULT_IO_OPS
if WINDOWS
libntfs_3g_la_SOURCES += win32_io.c
else
libntfs_3g_la_SOURCES += unix_io.c
endif
endif

# We may need to move .so files to root
# And create ldscript or symbolic link from /usr
install-exec-hook: install-rootlibLTLIBRARIES
if INSTALL_LIBRARY
	if [ ! "$(rootlibdir)" -ef "$(libdir)" ]; then \
		$(MV) -f "$(DESTDIR)/$(libdir)"/libntfs-3g.so* "$(DESTDIR)/$(rootlibdir)";  \
	fi
if GENERATE_LDSCRIPT
	if [ ! "$(rootlibdir)" -ef "$(libdir)" ]; then \
		$(install_sh_PROGRAM) "libntfs-3g.script.so" "$(DESTDIR)/$(libdir)/libntfs-3g.so"; \
	fi
else
	if [ ! "$(rootlibdir)" -ef "$(libdir)" ]; then \
		$(LN_S) "$(rootlibdir)/libntfs-3g.so" "$(DESTDIR)/$(libdir)/libntfs-3g.so"; \
	fi
endif
endif

uninstall-local:
if INSTALL_LIBRARY
	$(RM) -f "$(DESTDIR)/$(rootlibdir)"/libntfs-3g.so*
endif

