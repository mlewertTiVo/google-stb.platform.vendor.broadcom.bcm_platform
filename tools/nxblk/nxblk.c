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

#define LOG_TAG "nxblk"

#include "bstd.h"
#include "berr.h"
#include "nexus_platform.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "bkni.h"
#include "bkni_multi.h"
#include <cutils/log.h>
#include "nxclient.h"
#include <fcntl.h>
#include <cutils/properties.h>
#include "nx_ashmem.h"

BDBG_MODULE(client);

static void usage(void)
{
   ALOGI("nxblk [--dump] [--marker 0|1]");
}

int main(int argc, char **argv)
{
    int i = 1;
    int dump = 0;
    int marker = 0, mark_option = -1;
    char value[PROPERTY_VALUE_MAX];
    char value2[PROPERTY_VALUE_MAX];
    int fd, ret;

    property_get("ro.nexus.ashmem.devname", value, "nx_ashmem");
    strcpy(value2, "/dev/");
    strcat(value2, value);

    while (i < argc) {
       if (!strcmp(argv[i], "--dump")) {
          dump = 1;
       }
       if (!strcmp(argv[i], "--marker")) {
          marker = 1;
          mark_option = atoi(argv[++i]);
       }
       if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
          usage();
          return 0;
       }
       i++;
    };

    fd = open(value2, O_RDWR, 0);
    if ((fd == -1) || (!fd)) {
       return -EINVAL;
    }

    if (marker) {
       struct nx_ashmem_alloc ashmem_alloc;
       memset(&ashmem_alloc, 0, sizeof(struct nx_ashmem_alloc));
       ashmem_alloc.marker = (mark_option == 1) ? NX_ASHMEM_MARKER_VIDEO_DECODER : 0;
       ret = ioctl(fd, NX_ASHMEM_SET_SIZE, &ashmem_alloc);
       if (ret < 0) {
          return -EINVAL;
       }
    }

    if (dump) {
       ret = ioctl(fd, NX_ASHMEM_DUMP_ALL, NULL);
       if (ret < 0) {
          return -EINVAL;
       }
    }

    close(fd);
    return 0;
}
