/******************************************************************************
 * Copyright (C) 2017 Broadcom. The term "Broadcom" refers to Broadcom Limited and/or its subsidiaries.
 *
 * This program is the proprietary software of Broadcom and/or its licensors,
 * and may only be used, duplicated, modified or distributed pursuant to the terms and
 * conditions of a separate, written license agreement executed between you and Broadcom
 * (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 * no license (express or implied), right to use, or waiver of any kind with respect to the
 * Software, and Broadcom expressly reserves all rights in and to the Software and all
 * intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 * HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 * NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 * secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 * and to use this information only in connection with your use of Broadcom integrated circuit products.
 *
 * 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES
 * OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE,
 * LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION
 * OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF
 * USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
 * LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR
 * EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR
 * USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT
 * ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
 * LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF
 * ANY LIMITED REMEDY.
 ******************************************************************************/

#ifndef SSD_RPMB_H__
#define SSD_RPMB_H__

#include "bsagelib_types.h"

#ifdef __cplusplus
extern "C" {
#endif

enum rpmb_op_type {
    MMC_RPMB_WRITE_KEY = 0x01,
    MMC_RPMB_READ_CNT = 0x02,
    MMC_RPMB_WRITE = 0x03,
    MMC_RPMB_READ = 0x04,
    MMC_RPMB_READ_RESP = 0x05
};

#define SSD_RPMB_FRAME_STUFF_SIZE (196)
#define SSD_RPMB_FRAME_KEYMAC_SIZE (32)
#define SSD_RPMB_FRAME_DATA_SIZE (256)
#define SSD_RPMB_FRAME_NONCE_SIZE (16)

typedef struct ssd_rpmb_frame_s {
    uint8_t  stuff[SSD_RPMB_FRAME_STUFF_SIZE];
    uint8_t  key_mac[SSD_RPMB_FRAME_KEYMAC_SIZE];
    uint8_t  data[SSD_RPMB_FRAME_DATA_SIZE];
    uint8_t  nonce[SSD_RPMB_FRAME_NONCE_SIZE];
    uint32_t write_counter;
    uint16_t addr;
    uint16_t block_count;
    uint16_t result;
    uint16_t req_resp;
} ssd_rpmb_frame_t;

typedef struct {
    char *dev_path;
} ssd_rpmb_settings;

void SSD_RPMB_Get_Default_Settings(ssd_rpmb_settings *settings);

BERR_Code SSD_RPMB_Init(ssd_rpmb_settings *settings);

BERR_Code SSD_RPMB_Operation(ssd_rpmb_frame_t *frames, unsigned int cnt);

#ifdef __cplusplus
}
#endif


#endif /*SSD_RPMB_H__*/