/******************************************************************************
 *    (c)2011-2016 Broadcom Corporation
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
//#define LOG_NDEBUG 0
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "bcm-audio"

/* Android headers with "C" linkage */
#include <cutils/log.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>
#ifdef __cplusplus
}
#endif

#include <hardware/audio.h>
#include <hardware/hardware.h>
#include <utils/threads.h>
#include <utils/Mutex.h>
#include <utils/Errors.h>

/* Nexus headers */
#include "bstd.h"
#include "berr.h"
#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_audio_mixer.h"
#include "nexus_audio_dac.h"
#include "nexus_audio_output.h"
#include "nexus_audio_input.h"
#include "nxclient.h"
#include "nexus_simple_audio_playback.h"
#include "nexus_simple_audio_decoder.h"
#include "nexus_playpump.h"
#include "bmedia_util.h"

/* VERY_VERBOSE = 1 allows additional debug logs */
//#define VERY_VERBOSE 0

#if VERY_VERBOSE
#define ALOGVV ALOGV
#else
#define ALOGVV(...) ((void)0)
#endif

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#define DUMMY_AUDIO_OUT 0
#define DUMMY_AUDIO_IN  1

#define BRCM_PROPERTY_AUDIO_OUTPUT_HW_SYNC_FAKE ("media.brcm.hw_sync.fake")
#define DUMMY_HW_SYNC   0xCAFEBABE

#define BRCM_PROPERTY_AUDIO_OUTPUT_ENABLE_SPDIF_DOLBY ("persist.nx.spdif.enable_dolby")
#define BRCM_PROPERTY_AUDIO_OUTPUT_EAC3_TRANS_LATENCY ("ro.nx.eac3.trans_latency")

#define BRCM_PROPERTY_AUDIO_DIRECT_DOLBY_DECODE         ("media.brcm.direct_dolby_decode")
#define BRCM_PROPERTY_AUDIO_DIRECT_DOLBY_DECODE_PERSIST ("persist.nx.direct_dolby_decode")

#define BRCM_PROPERTY_AUDIO_DIRECT_FORCE_PCM         ("media.brcm.direct_force_pcm")
#define BRCM_PROPERTY_AUDIO_DIRECT_FORCE_PCM_PERSIST ("persist.nx.direct_force_pcm")

/* Special parameter for enabling EAC3 passthrough with tunnel video decoder */
#define AUDIO_PARAMETER_HW_AV_SYNC_EAC3 "HwAvSyncEAC3Supported"

typedef enum {
    BRCM_DEVICE_OUT_NEXUS = 0,
    BRCM_DEVICE_OUT_NEXUS_DIRECT,
    BRCM_DEVICE_OUT_NEXUS_TUNNEL,
    BRCM_DEVICE_OUT_USB,
    BRCM_DEVICE_OUT_MAX
} brcm_devices_out_t;

typedef enum {
    BRCM_DEVICE_IN_BUILTIN = 0,
    BRCM_DEVICE_IN_USB,
    BRCM_DEVICE_IN_MAX
} brcm_devices_in_t;

typedef enum {
    BRCM_NEXUS_STATE_NULL = 0,
    BRCM_NEXUS_STATE_CREATED,
    BRCM_NEXUS_STATE_DESTROYED,
} brcm_nexus_state_t;

typedef enum {
    BRCM_OWNER_NULL = 0,
    BRCM_OWNER_DEVICE,
    BRCM_OWNER_OUTPUT,
} brcm_owner_t;

struct StandbyMonitorThread;

struct brcm_device {
    struct audio_hw_device adev;

    pthread_mutex_t lock;
    audio_devices_t out_devices;
    audio_devices_t in_devices;
    bool mic_mute;

    struct brcm_stream_out *bouts[BRCM_DEVICE_OUT_MAX];
    struct brcm_stream_in *bins[BRCM_DEVICE_IN_MAX];
    struct StandbyMonitorThread *standbyThread;
    NEXUS_SimpleStcChannelHandle hw_sync_id;
};

struct brcm_stream_out_ops {
    int (*do_bout_open)(struct brcm_stream_out *bout);

    int (*do_bout_close)(struct brcm_stream_out *bout);

    uint32_t (*do_bout_get_latency)(struct brcm_stream_out *bout);

    int (*do_bout_start)(struct brcm_stream_out *bout);

    int (*do_bout_stop)(struct brcm_stream_out *bout);

    int (*do_bout_write)(struct brcm_stream_out *bout,
                         const void *buffer, size_t bytes);

    int (*do_bout_set_volume)(struct brcm_stream_out *bout,
                              float left, float right);

    int (*do_bout_get_render_position)(struct brcm_stream_out *bout,
                                        uint32_t *dsp_frames);

    int (*do_bout_get_presentation_position)(struct brcm_stream_out *bout,
                              uint64_t *frames);

    /* optional */
    int (*do_bout_dump)(struct brcm_stream_out *bout, int fd);
    char *(*do_bout_get_parameters)(struct brcm_stream_out *bout, const char *keys);

    /* optional but required for tunnel output */
    int (*do_bout_pause)(struct brcm_stream_out *bout);
    int (*do_bout_resume)(struct brcm_stream_out *bout);
    int (*do_bout_drain)(struct brcm_stream_out *bout, int action);
    int (*do_bout_flush)(struct brcm_stream_out *bout);
};

struct brcm_stream_out {
    struct audio_stream_out aout;
    struct brcm_stream_out_ops ops;

    pthread_mutex_t lock;
    audio_devices_t devices;
    audio_output_flags_t flags;
    struct audio_config config;
    uint32_t frameSize;
    uint64_t framesPlayed;
    size_t buffer_size;
    bool started;
    bool suspended;
    bool tunneled;
    int standbyCallback;
    FILE *outDebugFile;
    uint64_t last_pres_frame;
    struct timespec last_pres_ts;

    union {
        /* nexus specific */
        struct {
            brcm_nexus_state_t state;
            uint32_t connectId;
            NxClient_AllocResults allocResults;
            union {
                struct {
                    NEXUS_SimpleAudioPlaybackHandle simple_playback;
                    size_t lastPlayedBytes;
                } primary;
                struct {
                    NEXUS_SimpleAudioDecoderHandle simple_decoder;
                    bool playpump_mode;
                    NEXUS_PlaypumpHandle playpump;
                    NEXUS_PidChannelHandle pid_channel;
                    struct timespec start_ts;
                    size_t lastCount;
                    uint32_t transcode_latency;
                    NxClient_AudioOutputMode savedHDMIOutputMode;
                    NxClient_AudioOutputMode savedSPDIFOutputMode;
                } direct;
                struct {
                    NEXUS_SimpleAudioDecoderHandle audio_decoder;
                    NEXUS_PlaypumpHandle playpump;
                    NEXUS_SimpleStcChannelHandle stc_channel;
                    brcm_owner_t stc_channel_owner;
                    NEXUS_PidChannelHandle pid_channel;
                    const uint8_t *pp_buffer_end;
                    bmedia_waveformatex_header wave_fmt;
                    nsecs_t last_write_time;
                    nsecs_t last_pause_time;
                    bool debounce;
                    bool debounce_pausing;
                    bool debounce_more;
                    bool debounce_expired;
                    bool debounce_stopping;
                    pthread_t debounce_thread;
                    bool pcm_format;
                    FILE *pes_debug;
                } tunnel;
            };
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
                                         unsigned int channel_count,
                                         unsigned int duration_ms);

extern struct brcm_stream_out_ops nexus_bout_ops;
extern struct brcm_stream_out_ops nexus_direct_bout_ops;
extern struct brcm_stream_out_ops nexus_tunnel_bout_ops;

#if DUMMY_AUDIO_OUT
extern struct brcm_stream_out_ops dummy_bout_ops;
#endif

extern struct brcm_stream_in_ops builtin_bin_ops;

#if DUMMY_AUDIO_IN
extern struct brcm_stream_in_ops dummy_bin_ops;
#endif

extern void brcm_audio_set_mute_state(bool mute);
extern bool brcm_audio_get_mute_state(void);
extern void brcm_audio_set_master_volume(float volume);
extern float brcm_audio_get_master_volume(void);
extern NEXUS_Error brcm_audio_client_join(const char *name);
extern void brcm_audio_set_audio_volume(float leftVol, float rightVol);
extern NEXUS_AudioCodec brcm_audio_get_codec_from_format(audio_format_t format);

/* Thread to monitor standby */
#define MAX_STANDBY_MONITOR_CALLBACKS 3
typedef bool (*b_standby_monitor_callback)(void *context);
struct StandbyMonitorThread : public android::Thread {
public:
    StandbyMonitorThread();
    ~StandbyMonitorThread();

    int RegisterCallback(b_standby_monitor_callback callback, void *context);
    void UnregisterCallback(int id);

private:
    bool threadLoop();
    android::Mutex mMutex;
    b_standby_monitor_callback mCallbacks[MAX_STANDBY_MONITOR_CALLBACKS];
    void *mContexts[MAX_STANDBY_MONITOR_CALLBACKS];
    unsigned mNumCallbacks;

    /* Disallow copy constructor and copy operator... */
    StandbyMonitorThread(const StandbyMonitorThread &);
    StandbyMonitorThread &operator=(const StandbyMonitorThread &);
};

#endif // BRCM_AUDIO_H
