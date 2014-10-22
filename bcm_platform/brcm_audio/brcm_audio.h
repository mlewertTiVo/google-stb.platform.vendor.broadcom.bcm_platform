/******************************************************************************
 *    (c)2011-2013 Broadcom Corporation
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
 * $brcm_Workfile: AudioHardwareNexus.h $
 * $brcm_Revision:  $
 * $brcm_Date: 08/08/14 12:05p $
 * $brcm_Author: zhang@broadcom.com
 *
 * Module Description:
 *
 * Revision History:
 *
 * $brcm_Log:  $
 *
 *****************************************************************************/
#ifndef BRCM_AUDIO_H
#define BRCM_AUDIO_H

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <fcntl.h>
#include <linux/soundcard.h>

#ifdef __cplusplus
extern "C" {
#endif
/* LOG_NDEBUG = 0 allows debug logs */
#define LOG_NDEBUG 1

/* Android headers with "C" linkage */
#include <cutils/log.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>
#ifdef __cplusplus
}
#endif

#include <hardware/audio.h>
#include <hardware/hardware.h>

/* Nexus headers */
#include "bstd.h"
#include "berr.h"
#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_audio_playback.h"
#include "nexus_audio_mixer.h"
#include "nexus_audio_dac.h"
#include "nexus_audio_output.h"
#include "nexus_audio_input.h"
#include "nexus_simple_audio_playback.h"
#include "nexus_ipc_client_factory.h"

/* VERY_VERBOSE = 1 allows additional debug logs */
#define VERY_VERBOSE 0

#ifdef VERY_VERBOSE
#define LOGVV LOGV
#else
#define LOGVV(...) ((void)0)
#endif

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#define DUMMY_AUDIO_OUT 0
#define DUMMY_AUDIO_IN  1

typedef enum {
    BRCM_DEVICE_OUT_NEXUS = 0,
    BRCM_DEVICE_OUT_USB,
    BRCM_DEVICE_OUT_MAX
} brcm_devices_out_t;

typedef enum {
    BRCM_DEVICE_IN_BUILTIN = 0,
    BRCM_DEVICE_IN_USB,
    BRCM_DEVICE_IN_MAX
} brcm_devices_in_t;

struct brcm_device {
    struct audio_hw_device adev;

    pthread_mutex_t lock;
    audio_devices_t out_devices;
    audio_devices_t in_devices;
    bool mic_mute;

    struct brcm_stream_out *bouts[BRCM_DEVICE_OUT_MAX];
    struct brcm_stream_in *bins[BRCM_DEVICE_IN_MAX];
};

struct brcm_stream_out_ops {
    int (*do_bout_open)(struct brcm_stream_out *bout);

    int (*do_bout_close)(struct brcm_stream_out *bout);

    int (*do_bout_start)(struct brcm_stream_out *bout);

    int (*do_bout_stop)(struct brcm_stream_out *bout);

    int (*do_bout_write)(struct brcm_stream_out *bout,
                         const void *buffer, size_t bytes);

    int (*do_bout_set_volume)(struct brcm_stream_out *bout,
                              float left, float right);

    int (*do_bout_get_presentation_position)(struct brcm_stream_out *bout,
                              uint64_t *frames);

    /* optional */
    int (*do_bout_dump)(struct brcm_stream_out *bout, int fd);
};

struct brcm_stream_out {
    struct audio_stream_out aout;
    struct brcm_stream_out_ops ops;

    pthread_mutex_t lock;
    audio_devices_t devices;
    struct audio_config config;
    size_t buffer_size;
    bool started;
    bool suspended;

    union {
        /* nexus specific */
        struct {
            NexusIPCClientBase *ipc_client;
            NexusClientContext *nexus_client;
            NEXUS_SimpleAudioPlaybackHandle simple_playback;
            BKNI_EventHandle event;
        } nexus;
#if DUMMY_AUDIO_OUT
        /* dummy specific */
        struct {
            int fd;
        } dummy;
#endif
    };

    struct brcm_device *bdev;
};

struct brcm_stream_in_ops {
    int (*do_bin_open)(struct brcm_stream_in *bin);

    int (*do_bin_close)(struct brcm_stream_in *bin);

    int (*do_bin_start)(struct brcm_stream_in *bin);

    int (*do_bin_stop)(struct brcm_stream_in *bin);

    int (*do_bin_read)(struct brcm_stream_in *bin,
                       void *buffer, size_t bytes);

    /* optional */
    int (*do_bin_dump)(struct brcm_stream_in *bin, int fd);
};

struct brcm_stream_in {
    struct audio_stream_in ain;
    struct brcm_stream_in_ops ops;

    pthread_mutex_t lock;
    audio_devices_t devices;
    struct audio_config config;
    size_t buffer_size;
    bool started;
    bool suspended;

    union {
        /* builtin specific */
        struct {
            int fragment;
            int fd;
        } builtin;
#if DUMMY_AUDIO_IN
        /* dummy specific */
        struct {
            int fd;
        } dummy;
#endif
    };

    struct brcm_device *bdev;
};


extern size_t get_brcm_audio_buffer_size(unsigned int sample_rate,
                                         audio_format_t format,
                                         unsigned int channel_count);

extern struct brcm_stream_out_ops nexus_bout_ops;

#if DUMMY_AUDIO_OUT
extern struct brcm_stream_out_ops dummy_bout_ops;
#endif

extern struct brcm_stream_in_ops builtin_bin_ops;

#if DUMMY_AUDIO_IN
extern struct brcm_stream_in_ops dummy_bin_ops;
#endif

#endif // BRCM_AUDIO_H
