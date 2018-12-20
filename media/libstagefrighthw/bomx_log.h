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
#ifndef BOMX_LOG_H__
#define BOMX_LOG_H__

#include "vendor_bcm_props.h"

#define B_LOG_MASK_DEFAULT      (0)

/* Video decoder (12-bit) (bit 0-11) */
#define B_LOG_VDEC_BASE         (0)
#define B_LOG_VDEC_IN_FEED      (1<<(B_LOG_VDEC_BASE))      /* Input feeding */
#define B_LOG_VDEC_IN_RET       (1<<(B_LOG_VDEC_BASE+1))    /* Input returning */
#define B_LOG_VDEC_OUTPUT       (1<<(B_LOG_VDEC_BASE+2))    /* Output */
#define B_LOG_VDEC_NEXUS_RET    (1<<(B_LOG_VDEC_BASE+3))    /* Frames return to Nexus */
#define B_LOG_VDEC_DROP         (1<<(B_LOG_VDEC_BASE+4))    /* Frames dropped */
#define B_LOG_VDEC_TRANS_STATE  (1<<(B_LOG_VDEC_BASE+5))    /* State transitions */
#define B_LOG_VDEC_TRANS_PORT   (1<<(B_LOG_VDEC_BASE+6))    /* Port state transitions */
#define B_LOG_VDEC_PORT_RECFG   (1<<(B_LOG_VDEC_BASE+7))    /* Port re-configuration */
#define B_LOG_VDEC_STC          (1<<(B_LOG_VDEC_BASE+8))    /* STC (for tunneled decoder) */
#define B_LOG_VDEC_COLOR_INFO   (1<<(B_LOG_VDEC_BASE+9))    /* Color info */
#define B_LOG_VDEC_CODEC_CFG    (1<<(B_LOG_VDEC_BASE+10))   /* Codec configuration parser */

/* Video encoder (8-bit) (bit 12-19) */
#define B_LOG_VENC_BASE         (12)
#define B_LOG_VENC_IN_FEED      (1<<(B_LOG_VENC_BASE))      /* Input feeding */
#define B_LOG_VENC_IN_RET       (1<<(B_LOG_VENC_BASE+1))    /* Input returning */
#define B_LOG_VENC_OUTPUT       (1<<(B_LOG_VENC_BASE+2))    /* Output */
#define B_LOG_VENC_TRANS_STATE  (1<<(B_LOG_VENC_BASE+3))    /* State transitions */
#define B_LOG_VENC_TRANS_PORT   (1<<(B_LOG_VENC_BASE+4))    /* Port state transitions */
#define B_LOG_VENC_STATUS       (1<<(B_LOG_VENC_BASE+5))    /* Encoder status */

/* Audio decoder (8-bit) (bit 20-27) */
#define B_LOG_ADEC_BASE         (20)
#define B_LOG_ADEC_IN_FEED      (1<<(B_LOG_ADEC_BASE))      /* Input feeding */
#define B_LOG_ADEC_IN_RET       (1<<(B_LOG_ADEC_BASE+1))    /* Input returning */
#define B_LOG_ADEC_OUTPUT       (1<<(B_LOG_ADEC_BASE+2))    /* Output */
#define B_LOG_ADEC_TRANS_STATE  (1<<(B_LOG_ADEC_BASE+3))    /* State transitions */
#define B_LOG_ADEC_TRANS_PORT   (1<<(B_LOG_ADEC_BASE+4))    /* Port state transitions */

/* Misc (4-bit) (bit 28-31) */
#define B_LOG_MISC_BASE         (28)
#define B_LOG_MISC_COMP_INST    (1<<(B_LOG_MISC_BASE))      /* Component instantiation/destruction */


/* VERY_VERBOSE = 1 allows additional debug logs */
//#define VERY_VERBOSE 0
#if VERY_VERBOSE
#define ALOGVV ALOGV
#else
#define ALOGVV(...) ((void)0)
#endif

#endif /* #ifndef BOMX_LOG_H__*/
