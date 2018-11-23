/***************************************************************************
 *  Copyright (C) 2018 Broadcom.
 *  The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 *
 *  This program is the proprietary software of Broadcom and/or its licensors,
 *  and may only be used, duplicated, modified or distributed pursuant to
 *  the terms and conditions of a separate, written license agreement executed
 *  between you and Broadcom (an "Authorized License").  Except as set forth in
 *  an Authorized License, Broadcom grants no license (express or implied),
 *  right to use, or waiver of any kind with respect to the Software, and
 *  Broadcom expressly reserves all rights in and to the Software and all
 *  intellectual property rights therein. IF YOU HAVE NO AUTHORIZED LICENSE,
 *  THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD
 *  IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
 *
 *  Except as expressly set forth in the Authorized License,
 *
 *  1.     This program, including its structure, sequence and organization,
 *  constitutes the valuable trade secrets of Broadcom, and you shall use all
 *  reasonable efforts to protect the confidentiality thereof, and to use this
 *  information only in connection with your use of Broadcom integrated circuit
 *  products.
 *
 *  2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 *  "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS
 *  OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
 *  RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL
 *  IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR
 *  A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET
 *  ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME
 *  THE ENTIRE RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 *  3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM
 *  OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL,
 *  INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY
 *  RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM
 *  HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN
 *  EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1,
 *  WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY
 *  FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 * Module Description:
 *
 **************************************************************************/
#ifndef BOMX_AUDIO_LOG_H__
#define BOMX_AUDIO_LOG_H__

/* LOG_NDEBUG = 0 allows debug logs */
//#define LOG_NDEBUG 0
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "bcm-audio"

#include "vendor_bcm_props.h"

// The audio category represents a left binary shift used to
// calculate a debug mask which will be compared against the
// value set in ba_log_mask.
// Note, there are gaps left on purpose to make room for
// additional categories in the future.

// Writing here some common mask values (in decimal) that may
// be used during debugging
// 1. All logs : 0xFFFFFFFF = 4294967295
// 2. All primary: 0x3C = 60
// 3. All tunneling: 0xF0 = 3840
// 4. Tunneling minus POS = 1792
// 5. All direct: 0xF0000 = 983040
// 6. Verbose: 0x40000000 = 1073741824

enum B_LOG_AUDIO_CATEGORY {
    // High level hal APIs
    MAIN_DBG= 0,
    // Primary output
    PRIM_STATE = 2,
    PRIM_DBG,
    PRIM_WRITE,
    PRIM_POS,
    // Tunneling output
    TUN_STATE = 8,
    TUN_DBG,
    TUN_WRITE,
    TUN_POS,
    // Direct output
    DIR_STATE = 16,
    DIR_DBG,
    DIR_WRITE,
    DIR_POS,

    // Verbose
    VERB = 30
};

extern int ba_log_mask;

void inline BA_LOG_INIT() {
    ba_log_mask = property_get_int32(BCM_DYN_AUDIO_LOG_MASK, 0);
}

bool inline BA_LOG_ON(enum B_LOG_AUDIO_CATEGORY LOG_CAT) {
    return ba_log_mask & LOG_CAT;
}

#define BA_LOG(CAT, format, ...) ALOGD_IF((1<<CAT) & ba_log_mask, format,  __VA_ARGS__)

#endif /* #ifndef BOMX_AUDIO_LOG_H__*/
