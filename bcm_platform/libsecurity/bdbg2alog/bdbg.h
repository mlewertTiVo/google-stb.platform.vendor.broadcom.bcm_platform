/***************************************************************************
 *     (c)2015 Broadcom Corporation
 *
 *  This program is the proprietary software of Broadcom Corporation and/or its licensors,
 *  and may only be used, duplicated, modified or distributed pursuant to the terms and
 *  conditions of a separate, written license agreement executed between you and Broadcom
 *  (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 *  no license (express or implied), right to use, or waiver of any kind with respect to the
 *  Software, and Broadcom expressly reserves all rights in and to the Software and all
 *  intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 *  HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 *  NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
 *
 *  Except as expressly set forth in
 the Authorized License,
 *
 *  1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 *  secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 *  and to use this information only in connection with your use of Broadcom integrated circuit products.
 *
 *  2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 *  AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
 *  WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 *  THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES
 *  OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE,
 *  LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION
 *  OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF
 *  USE OR PERFORMANCE OF THE SOFTWARE.
 *
 *  3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
 *  LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR
 *  EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR
 *  USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF
 *  THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT
 *  ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
 *  LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF
 *  ANY LIMITED REMEDY.
 *
 *
 * Module Description:
 *    BDBG wrapper to turn BDBG logging calls to Android logging calls.
 *
 *    This wrapper is created to allow modules that are dynamically loaded/unloaded
 *    in Android environment to use Android native logs instead of BDBG logs.
 *    BDBG by design does not support dynamically unloading.
 *
 * Revision History:
 *
 **************************************************************************/

#ifndef BDBG2ALOG_BDBG_H
#define BDBG2ALOG_BDBG_H

#ifndef ANDROID
#error bdbg2alog bdbg.h wrapper being used in non Android build
#endif

#ifdef BDBG_H
#error magnum bdbg.h has already been included
#endif

#include <cutils/log.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef BDBG_DEBUG_BUILD
#undef BDBG_DEBUG_BUILD
#endif
#ifdef BDBG2ALOG_ENABLE_LOGS
#define BDBG_DEBUG_BUILD BDBG2ALOG_ENABLE_LOGS
#else
#define BDBG_DEBUG_BUILD 0
#endif

/* Mapping of BDBG Log levels to Android log levels
 *    BDBG_ENTER  - ANDROID_LOG_VERBOSE
 *    BDBG_LEAVE  - ANDROID_LOG_VERBOSE
 *    BDBG_LOG    - ANDROID_LOG_DEBUG
 *    BDBG_MSG    - ANDROID_LOG_INFO
 *    BDBG_WRN    - ANDROID_LOG_WARN
 *    BDBG_ERR    - ANDROID_LOG_ERROR
 *
 * BDBG_ASSERT() is mapped to ALOG_ASSERT()
 */

#define BDBG_MODULE(module) static const char *alog_module=#module;

#if !BDBG_DEBUG_BUILD
/* All logs are disabled by default for non debug build */
#ifndef BDBG_NO_LOG
#define BDBG_NO_LOG 1
#endif
#ifndef BDBG_NO_MSG
#define BDBG_NO_MSG 1
#endif
#ifndef BDBG_NO_WRN
#define BDBG_NO_WRN 1
#endif
#ifndef BDBG_NO_ERR
#define BDBG_NO_ERR 1
#endif
#endif


#define BDBG_NOP() (void)0

#if BDBG_NO_LOG
#define BDBG_LOG(x) BDBG_NOP()
#define BDBG_ENTER(function) BDBG_NOP()
#define BDBG_LEAVE(function) BDBG_NOP()
#else
#define BDBG_LOG(x) BDBG2ALOG_DEBUG x
#define BDBG2ALOG_DEBUG(...) ((void)LOG_PRI(ANDROID_LOG_DEBUG, alog_module, __VA_ARGS__))
#define BDBG_ENTER(function) ((void)LOG_PRI(ANDROID_LOG_VERBOSE, alog_module, "Enter... %s", #function ))
#define BDBG_LEAVE(function) ((void)LOG_PRI(ANDROID_LOG_VERBOSE, alog_module, "Leave... %s", #function ))
#endif

#if BDBG_NO_MSG
#define BDBG_MSG(x) BDBG_NOP()
#else
#define BDBG_MSG(x) BDBG2ALOG_INFO x
#define BDBG2ALOG_INFO(...) ((void)LOG_PRI(ANDROID_LOG_INFO, alog_module, __VA_ARGS__))
#endif

#if BDBG_NO_WRN
#define BDBG_WRN(x) BDBG_NOP()
#else
#define BDBG_WRN(x) BDBG2ALOG_WRN x
#define BDBG2ALOG_WRN(...) ((void)LOG_PRI(ANDROID_LOG_WARN, alog_module, __VA_ARGS__))
#endif

#if BDBG_NO_ERR
#define BDBG_ERR(x) BDBG_NOP()
#else
#define BDBG_ERR(x) BDBG2ALOG_ERR x
#define BDBG2ALOG_ERR(...) ((void)LOG_PRI(ANDROID_LOG_ERROR, alog_module, __VA_ARGS__))
#endif

#define BDBG_ASSERT ALOG_ASSERT

/* Stubs for BDBG macros not appliable to ALOG but required for the code to comple with the bdbg wrapper */
#define BDBG_OBJECT(name)
#define BDBG_OBJECT_ASSERT(ptr,name)
#define BDBG_OBJECT_DESTROY(ptr,name)
#define BDBG_OBJECT_SET(ptr,name)
#define BDBG_OBJECT_ID(name)
#define BDBG_OBJECT_ID_DECLARE(name)
#define BDBG_SetModuleLevel(module, level)

#ifdef __cplusplus
}
#endif

#endif /* BDBG2ALOG_BDBG_H */

