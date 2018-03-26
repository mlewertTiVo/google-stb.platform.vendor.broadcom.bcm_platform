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
 *****************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <linux/mmc/ioctl.h>
#include <sys/endian.h>

#include "ssd_rpmb.h"

#include <log/log.h>
#include "nexus_platform.h"


#define RPMB_DEFAULT_PATH "/dev/block/mmcblk0rpmb"

/* From kernel linux/mmc/mmc.h */
#define MMC_READ_MULTIPLE_BLOCK  18   /* adtc [31:0] data addr   R1  */
#define MMC_WRITE_MULTIPLE_BLOCK 25   /* adtc                    R1  */

/* From kernel linux/mmc/core.h */
#define MMC_RSP_SPI_S1  (1 << 7)        /* one status byte */
#define MMC_RSP_SPI_R1  (MMC_RSP_SPI_S1)
#define MMC_CMD_ADTC    (1 << 5)
#define MMC_RSP_PRESENT (1 << 0)
#define MMC_RSP_CRC (1 << 2)        /* expect valid crc */
#define MMC_RSP_OPCODE  (1 << 4)        /* response contains opcode */
#define MMC_RSP_R1  (MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)

static ssd_rpmb_settings g_settings;

void SSD_RPMB_Get_Default_Settings(ssd_rpmb_settings *settings)
{
    if (settings != NULL) {
        BKNI_Memset((uint8_t *) settings, 0x00, sizeof(ssd_rpmb_settings));
        settings->dev_path = RPMB_DEFAULT_PATH;
    }

    return;
}

BERR_Code SSD_RPMB_Init(ssd_rpmb_settings *settings)
{
    BKNI_Memcpy((uint8_t *) &g_settings, (uint8_t *) settings, sizeof(g_settings));

    ALOGD("RPMB device path: %s\n", g_settings.dev_path);

    return BERR_SUCCESS;
}

/* Performs RPMB operation.
 *
 * @fd: RPMB device on which we should perform ioctl command
 * @frame_in: input RPMB frame, should be properly inited
 * @frame_out: output (result) RPMB frame. Caller is responsible for checking
 *             result and req_resp for output frame.
 * @out_cnt: count of outer frames. Used only for multiple blocks reading,
 *           in the other cases -EINVAL will be returned.
 */
BERR_Code SSD_RPMB_Operation(ssd_rpmb_frame_t *frames, unsigned int blockCount)
{
    BERR_Code rc = BERR_SUCCESS;
    int dev_fd = 0;
    u_int16_t rpmb_type;

    struct mmc_ioc_cmd ioc = {
        .arg = 0x0,
        .blksz = 512,
        .blocks = blockCount,
        .write_flag = 1,
        .opcode =  MMC_WRITE_MULTIPLE_BLOCK, .flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC,
        .data_ptr = (uintptr_t) frames
    };

    if (!frames || !blockCount) {
        rc = NEXUS_INVALID_PARAMETER;
        goto errorExit;
    }

    dev_fd = open(g_settings.dev_path, O_RDWR);
    if (dev_fd < 0) {
        rc = BERR_OS_ERROR;
        goto errorExit;
    }

    rpmb_type = be16toh(frames->req_resp);

    switch (rpmb_type) {
    case MMC_RPMB_WRITE:
        /* Write request */
        ioc.write_flag |= (1 << 31);
        if (ioctl(dev_fd, MMC_IOC_CMD, &ioc)) {
            rc = BERR_OS_ERROR;
            goto errorExit;
        }

        /* Result request. */
        frames->req_resp = htobe16(MMC_RPMB_READ_RESP);
        ioc.write_flag = 1;
        ioc.blocks = 1;
        if (ioctl(dev_fd, MMC_IOC_CMD, &ioc)) {
            rc = BERR_OS_ERROR;
            goto errorExit;
        }

        /* Get response */
        ioc.write_flag = 0;
        ioc.blocks = 1;
        ioc.opcode = MMC_READ_MULTIPLE_BLOCK;
        if (ioctl(dev_fd, MMC_IOC_CMD, &ioc)) {
            rc = BERR_OS_ERROR;
            goto errorExit;
        }

        break;

    case MMC_RPMB_READ_CNT:
        if (blockCount != 1) {
            rc = NEXUS_INVALID_PARAMETER;
            goto errorExit;
        }

        if (ioctl(dev_fd, MMC_IOC_CMD, &ioc)) {
            rc = BERR_OS_ERROR;
            goto errorExit;
        }

        /* Get response */
        ioc.write_flag = 0;
        ioc.blocks = 1;
        ioc.opcode = MMC_READ_MULTIPLE_BLOCK;
        if (ioctl(dev_fd, MMC_IOC_CMD, &ioc)) {
            rc = BERR_OS_ERROR;
            goto errorExit;
        }
        break;
    case MMC_RPMB_READ:
        /* Request */
        ioc.blocks = 1;
        if (ioctl(dev_fd, MMC_IOC_CMD, &ioc)) {
            rc = BERR_OS_ERROR;
            goto errorExit;
        }

        /* Get response */
        ioc.write_flag = 0;
        ioc.blocks = blockCount;
        ioc.opcode = MMC_READ_MULTIPLE_BLOCK;
        if (ioctl(dev_fd, MMC_IOC_CMD, &ioc)) {
            rc = BERR_OS_ERROR;
            goto errorExit;
        }

        break;
    case MMC_RPMB_WRITE_KEY:
        frames->result = 0;
        /* Write request */
        ioc.write_flag |= (1 << 31);
        if (ioctl(dev_fd, MMC_IOC_CMD, &ioc)) {
            rc = BERR_OS_ERROR;
            goto errorExit;
        }

        /* Result request. */
        frames->req_resp = htobe16(MMC_RPMB_READ_RESP);
        ioc.write_flag = 1;
        ioc.blocks = 1;
        if (ioctl(dev_fd, MMC_IOC_CMD, &ioc)) {
            rc = BERR_OS_ERROR;
            goto errorExit;
        }

        /* Get response */
        ioc.write_flag = 0;
        ioc.blocks = 1;
        ioc.opcode = MMC_READ_MULTIPLE_BLOCK;
        if (ioctl(dev_fd, MMC_IOC_CMD, &ioc)) {
            rc = BERR_OS_ERROR;
            goto errorExit;
        }
        break;

    default:
        rc = NEXUS_INVALID_PARAMETER;
        goto errorExit;
    }

    /* Check RPMB response */
    if (frames->result != 0) {
        ALOGE("RPMB operation failed, retcode 0x%04x\n", frames->result);
        rc = BERR_OS_ERROR;
        goto errorExit;
    }

errorExit:
    if (dev_fd > 0)
        close(dev_fd);
    return rc;
}
