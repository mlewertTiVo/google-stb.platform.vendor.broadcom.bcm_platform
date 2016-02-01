/***************************************************************************
 *     (c)2015 Broadcom Corporation
 *
 * This program is the proprietary software of Broadcom Corporation and/or its licensors,
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
 *
 **************************************************************************/
#ifndef BSTD_CPU_ENDIAN
#define BSTD_CPU_ENDIAN BSTD_ENDIAN_LITTLE
#endif

#define LOG_TAG "yv12cap"

#include <cutils/log.h>
#include "nxclient.h"
#include "nexus_base_os.h"
#include "nexus_surface.h"
#include "nexus_surface_client.h"
#include "nexus_graphics2d.h"
#include "bm2mc_packet.h"
#include "nexus_core_utils.h"
#include "bstd.h"
#include "bkni.h"
#include "bkni_multi.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

BDBG_MODULE(YV12CAP);

int main(int argc, char **argv)
{
    NxClient_JoinSettings joinSettings;
    NEXUS_Error rc;
    void *vaddr;
    int width = -1, height = -1, size = -1;
    NEXUS_MemoryBlockHandle block_handle = NULL;
    char fname[256];
    char fpath[128];
    time_t rawtime;
    struct tm * timeinfo;
    char tick[64];
    FILE *fp = NULL;
    int curarg = 1;

    memset(fpath, 0, sizeof(fpath));
    while (argc > curarg) {
        if (!strcmp(argv[curarg], "-w") && argc>curarg+1) {
            width = strtoul(argv[++curarg], NULL, 10);
            ALOGI("%s: width %d", __FUNCTION__, width);
        } else if (!strcmp(argv[curarg], "-h") && argc>curarg+1) {
            height = strtoul(argv[++curarg], NULL, 10);
            ALOGI("%s: height %d", __FUNCTION__, height);
        } else if (!strcmp(argv[curarg], "-s") && argc>curarg+1) {
            size = strtoul(argv[++curarg], NULL, 10);
            ALOGI("%s: size %d", __FUNCTION__, size);
        } else if (!strcmp(argv[curarg], "-handle") && argc>curarg+1) {
            block_handle = (NEXUS_MemoryBlockHandle) strtoul(argv[++curarg], NULL, 16);
            ALOGI("%s: block handle \'%p\'", __FUNCTION__, block_handle);
        } else if (!strcmp(argv[curarg], "-path") && argc>curarg+1) {
            strncpy(fpath, (const char *)argv[++curarg], sizeof(fpath));
        }
        curarg++;
    };


    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(tick, sizeof(tick), "%F_%H_%M_%S", timeinfo);

    sprintf(fname, "/%s/frame_w%d_h%d_%s.yv12", fpath, width, height, tick);
    fp = fopen(fname, "wb+");
    ALOGI("%s: write to \'%s\', input %dx%d, sz %d, hnd %p, fp 0x%x (%s)", __FUNCTION__,
          fname, width, height, size, block_handle, (unsigned)fp,
          (fp == NULL) ? strerror(errno) : "okay");
    if (width <= 0 || height <= 0 || size <= 0 || fp == NULL || block_handle == NULL) return -1;

    NxClient_GetDefaultJoinSettings(&joinSettings);
    joinSettings.ignoreStandbyRequest = true;
    joinSettings.timeout = 60;
    rc = NxClient_Join(&joinSettings);
    if (rc) {ALOGE("%s: failed nexus join.", __FUNCTION__); return -1;}

    NEXUS_MemoryBlock_Lock(block_handle, &vaddr);
    if (vaddr == NULL) return -1;
    fwrite(vaddr, 1, size, fp);
    NEXUS_MemoryBlock_Unlock(block_handle);

    fclose(fp);
    NxClient_Uninit();
    return 0;
}
