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

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>
#include <sys/endian.h>
#include <string.h>

#include "ssd_vfs.h"
#include "ssd_rpmb.h"
#include "nxclient.h"

#include <log/log.h>

#define VFS_DEFAULT_PATH "/data/vendor/nexus/ssd/vfs.raw"

static ssd_vfs_settings g_settings;

void SSD_VFS_Get_Default_Settings(ssd_vfs_settings *settings)
{
    if (settings != NULL) {
        BKNI_Memset((uint8_t *) settings, 0x00, sizeof(ssd_vfs_settings));
        settings->fs_path = VFS_DEFAULT_PATH;
    }

    return;
}

BERR_Code SSD_VFS_Init(ssd_vfs_settings *settings)
{
    BKNI_Memcpy((uint8_t *) &g_settings, (uint8_t *) settings, sizeof(g_settings));

    ALOGD("VFS filesystem path: %s\n", g_settings.fs_path);

    return BERR_SUCCESS;
}

BERR_Code SSD_VFS_Operation(ssd_rpmb_frame_t *frames, unsigned int blockCount) {
    FILE *file_fd = NULL;
    BERR_Code rc = BERR_SUCCESS;
    u_int16_t rpmb_type;
    u_int16_t addr;
    ssize_t size;

    if (!frames || !blockCount) {
        rc = NEXUS_INVALID_PARAMETER;
        goto errorExit;
    }

    rpmb_type = be16toh(frames->req_resp);
    addr = be16toh(frames->addr);

    /* Do some sanity checks */
    if ((addr + blockCount) > SSD_VFS_NUM_BLOCKS) {
        ALOGE("Request out of range\n");
        rc = NEXUS_INVALID_PARAMETER;
        goto errorExit;
    }

    if (((rpmb_type == MMC_RPMB_WRITE) || (rpmb_type == MMC_RPMB_READ)) && blockCount == 0) {
        ALOGE("Must specify a block count\n");
        rc = NEXUS_INVALID_PARAMETER;
        goto errorExit;
    }

    file_fd = fopen(g_settings.fs_path, "rb+");
    if (file_fd == NULL) {
        /* Doesn't exist. Make one */
        file_fd = fopen(g_settings.fs_path, "wb");
        if (file_fd == NULL) {
            ALOGE("Error opening VFS partition\n");
            rc = BERR_OS_ERROR;
            goto errorExit;
        }
    }

    rc = fseek (file_fd , addr * SSD_VFS_BLOCK_SIZE , SEEK_SET);
    if (rc) {
        ALOGE("Error seeking VFS partition\n");
        goto errorExit;
    }

    switch (rpmb_type) {
    case MMC_RPMB_WRITE:
        for (int i = 0; i < (int)blockCount; i++) {
            size = fwrite(frames[i].data, SSD_VFS_BLOCK_SIZE, 1, file_fd);
            if (size != 1) {
                ALOGE("Error writing to VFS partition\n");
                rc = BERR_OS_ERROR;
                goto errorExit;
            }
        }
        break;

    case MMC_RPMB_READ:
        for (int i = 0; i < (int)blockCount; i++) {
            size = fread(frames[i].data, SSD_VFS_BLOCK_SIZE, 1, file_fd);
            if (size != 1) {
                // Silent error. Most likely read passed existing EOF. Return NULL.
                memset(frames[i].data, 0x00, SSD_VFS_BLOCK_SIZE);
            }
        }
        break;

    default:
        ALOGW("Invalid request\n");
        rc = NEXUS_INVALID_PARAMETER;
        goto errorExit;
        break;
    }

errorExit:
    if (file_fd)
        fclose(file_fd);
    return rc;
}
