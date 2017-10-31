/*
 * Copyright 2016-2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <jni.h>
#include <android/log.h>

#include "nxclient.h"
#include "nexus_frontend.h"
#include "nexus_surface_client.h"
#include "nexus_platform.h"
#include "nexus_simple_video_decoder.h"
#include "nexus_simple_audio_decoder.h"
#include "nexus_parser_band.h"
#include "live_decode.h"
#include "live_source.h"
#include "tspsimgr3.h"
#include <namevalue.h>
#include "blst_queue.h"
#include "binput.h"
#include "bgui.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sstream>
#include <iomanip>

// Android log function wrappers
static const char* TAG = "bcmtuner";
#define ALOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))
#define ALOGV(...) ((void)__android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__))
#define ALOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__))
#define ALOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__))

static NxClient_JoinSettings joinSettings;
static NEXUS_Error rc;
static NxClient_AllocResults allocResults;
static struct decoder *d;
static live_decode_t decode;
static struct frontend *frontend;
static binput_t input;
static NxClient_AllocSettings allocSettings;
static NEXUS_SurfaceClientHandle surfaceClient, video_sc;
static struct binput_settings input_settings;
static struct b_pig_inc pig_inc;
static struct btune_settings tune_settings;
static NEXUS_VideoWindowContentMode contentMode = NEXUS_VideoWindowContentMode_eMax;
static struct channel_map *map = NULL;
static unsigned num_decodes = 1;

enum gui {gui_no_alpha_hole, gui_on, gui_constellation};
static int gui_init(NEXUS_FrontendHandle frontend, NEXUS_SurfaceClientHandle surfaceClient, enum gui gui);
static void gui_uninit(void);

static enum gui gui = gui_on;

#include <sys/time.h>
static unsigned b_get_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000 + tv.tv_usec/1000;
}

struct channel_map {
    BLST_Q_ENTRY(channel_map) link;
    char name[32];
    struct btune_settings tune;
    bchannel_scan_t scan;
    enum { scan_state_pending, scan_state_started, scan_state_done } scan_state;
    tspsimgr_scan_results scan_results;
    unsigned scan_start;
};
static BLST_Q_HEAD(channellist_t, channel_map) g_channel_map = BLST_Q_INITIALIZER(g_channel_map);
static unsigned g_total_channels;

struct frontend {
    BLST_Q_ENTRY(frontend) link;
    NEXUS_FrontendHandle frontend;
    struct channel_map *map;
    unsigned refcnt;
};
static BLST_Q_HEAD(frontendlist_t, frontend) g_frontends;

struct decoder {
    BLST_Q_ENTRY(decoder) link;
    NEXUS_ParserBand parserBand;
    live_decode_channel_t channel;
    live_decode_start_settings start_settings;
    unsigned video_pid, audio_pid; /* scan override */
    NEXUS_VideoCodec video_type;
    NEXUS_AudioCodec audio_type;

    unsigned chNum; /* global number */
    struct frontend *frontend;
    struct channel_map *map;
    unsigned program; /* relative to map */
};
static BLST_Q_HEAD(decoderlist_t, decoder) g_decoders;
static bool g_quiet = false;

static struct {
    bool done;
    pthread_t thread;
    NEXUS_SurfaceClientHandle surfaceClient;
    NEXUS_SurfaceHandle surface;
    NEXUS_SurfaceCreateSettings createSettings;
    NEXUS_FrontendHandle frontend;
    BKNI_EventHandle displayedEvent;
} g_constellation;

struct client_state
{
    bool done;
};

static void complete2(void *data, int param)
{
    ALOGI("%s: Enter", __FUNCTION__);
    BSTD_UNUSED(param);
    /* race condition between unsetting the callback and destroying the event, so protect with a global */
    if (g_constellation.done) return;
    BKNI_SetEvent((BKNI_EventHandle)data);
}

static void insert_channel(const struct btune_settings *tune)
{
    ALOGI("%s: Enter", __FUNCTION__);
    struct channel_map *c = (channel_map *)BKNI_Malloc(sizeof(*c));
    BKNI_Memset(c, 0, sizeof(*c));
    c->tune = *tune;
    bchannel_source_print(c->name, sizeof(c->name), tune);
    BLST_Q_INSERT_TAIL(&g_channel_map, c, link);
}

static void uninit_channels(void)
{
    ALOGI("%s: Enter", __FUNCTION__);
    struct channel_map *map;
    while ((map = BLST_Q_FIRST(&g_channel_map))) {
        BLST_Q_REMOVE(&g_channel_map, map, link);
        BKNI_Free(map);
    }
}

static void parse_channels(const struct btune_settings *tune, const char *freq_list)
{
    ALOGI("%s: Enter", __FUNCTION__);
#if NEXUS_HAS_FRONTEND
    unsigned first_channel = 0, last_channel = 0;
    const unsigned _mhz = 1000000;
#endif
    if (tune->source == channel_source_streamer) {
        insert_channel(tune);
        return;
    }
#if NEXUS_HAS_FRONTEND
    if (!strcmp(freq_list, "all")) {
        if (tune->source == channel_source_qam) {
            /* simple scheme doesn't include 79, 85 exceptions */
            first_channel = 57 * _mhz;
            last_channel = 999 * _mhz;
        }
        else if (tune->source == channel_source_ofdm) {
            /* simple scheme doesn't include 79, 85 exceptions */
            if((tune->mode == 0) || (tune->mode == 1))
                first_channel = 578 * _mhz;
            else
                first_channel = 473 * _mhz;
            last_channel = 602 * _mhz;
        }
        else {
            return;
        }
    }

    while (freq_list) {
        unsigned freq;
        if (first_channel) {
            freq = first_channel;
            if (freq > last_channel) break;
            if((tune->source == channel_source_ofdm) && ((tune->mode == 0) || (tune->mode == 1)))
                first_channel += 8 * _mhz;
            else
                first_channel += 6 * _mhz;
        }
        else {
            float f;
            if (sscanf(freq_list, "%f", &f) != 1) f = 0;
            if (f <  _mhz) {
                /* convert to Hz, rounding to nearest 1000 */
                freq = (unsigned)(f * 1000) * 1000;
            }
            else {
                freq = f;
            }
        }

        if (freq) {
            struct btune_settings t = *tune;
            t.freq = freq;
            insert_channel(&t);
        }
        if (!first_channel) {
            freq_list = strchr(freq_list, ',');
            if (freq_list) freq_list++;
        }
    }
#else
    BSTD_UNUSED(freq_list);
#endif
}

static void stop_decode(struct decoder *d)
{
    ALOGI("%s: Enter", __FUNCTION__);
    if (!d->channel) return;

    ALOGV("stop_decode(%p) %p frontend=%p(%d) map=%p program=%d", (void*)d, (void*)d->channel, d->frontend?(void*)d->frontend->frontend:NULL, d->frontend?d->frontend->refcnt:0, (void*)d->map, d->program);
    live_decode_stop_channel(d->channel);
    if (d->frontend) {
        BDBG_ASSERT(d->frontend->map);
        BDBG_ASSERT(d->frontend->refcnt);
        if (--d->frontend->refcnt == 0) {
            d->frontend->map = NULL;
        }
        d->frontend = NULL;
    }
}

static int set_channel(struct decoder *d, unsigned chNum)
{
    ALOGI("%s: Enter", __FUNCTION__);
    struct channel_map *map;
    if (chNum >= g_total_channels && g_total_channels) {
        chNum = chNum % g_total_channels;
    }
    d->chNum = chNum;
    for (map = BLST_Q_FIRST(&g_channel_map); map; map = BLST_Q_NEXT(map, link)) {
        if (chNum < map->scan_results.num_programs) break;
        chNum -= map->scan_results.num_programs;
    }
    if (!map) {
        d->map = NULL;
        return -1;
    }
    d->map = map;
    d->program = chNum;
    ALOGV("set_channel(%p,%d) %p %d", (void*)d, chNum, (void*)d->map, d->program);
    return 0;
}

static int start_priming(struct decoder *d)
{
    ALOGI("%s: Enter", __FUNCTION__);
    struct channel_map *map;
    struct frontend *frontend;
    int rc;

    if (!d->channel || !d->map) {
        return -1;
    }

    map = d->map;

    /* find frontend that's already tuned */
    for (frontend = BLST_Q_FIRST(&g_frontends); frontend; frontend = BLST_Q_NEXT(frontend, link)) {
        if (frontend->map == map) {
            break;
        }
    }
    if (!frontend) {
        /* or tune a new frontend */
        for (frontend = BLST_Q_FIRST(&g_frontends); frontend; frontend = BLST_Q_NEXT(frontend, link)) {
            if (!frontend->map) break;
        }
    }
    if (frontend) {
        rc = tune(d->parserBand, frontend->frontend, &map->tune, (frontend->map != NULL));
        if (rc) {
            frontend = NULL;
        }
        else {
            frontend->map = map;
        }
    }
    if (!frontend) {
        if (!g_quiet) ALOGV("unable to tune");
        return -1;
    }

    d->frontend = frontend;
    frontend->refcnt++;

    d->start_settings.parserBand = d->parserBand;
    if (d->video_pid != 0x1fff) {
        d->start_settings.video.pid = d->video_pid;
        d->start_settings.video.codec = d->video_type;
    }
    else {
        d->start_settings.video.pid = map->scan_results.program_info[d->program].video_pids[0].pid;
        d->start_settings.video.codec = map->scan_results.program_info[d->program].video_pids[0].codec;
    }
    if (d->audio_pid != 0x1fff) {
        d->start_settings.audio.pid = d->audio_pid;
        d->start_settings.audio.codec = d->audio_type;
    }
    else {
        d->start_settings.audio.pid = map->scan_results.program_info[d->program].audio_pids[0].pid;
        d->start_settings.audio.codec = map->scan_results.program_info[d->program].audio_pids[0].codec;
    }
    d->start_settings.pcr_pid = map->scan_results.program_info[d->program].pcr_pid;
    rc = live_decode_start_channel(d->channel, &d->start_settings);
    if (rc) return BERR_TRACE(rc);

    ALOGV("start_priming(%p) %p frontend=%p(%d) parserBand=%p, map=%p program=%d", (void*)d, (void*)d->channel, (void*)d->frontend->frontend, d->frontend->refcnt, (void*)d->parserBand, (void*)d->map, d->program);

    return 0;
}

static void jumpToChannel(int tsid, int programNum){
    ALOGI("%s: Enter", __FUNCTION__);
    struct decoder *d;
    struct channel_map *map;
    int channelNumber = 0;

    for (d = BLST_Q_FIRST(&g_decoders); d; d = BLST_Q_NEXT(d, link)) {
        stop_decode(d);
    }

    //Find the channel number matching tsid and programNum
    for (map = BLST_Q_FIRST(&g_channel_map); map; map = BLST_Q_NEXT(map, link)) {
        if (map->scan_state == channel_map::scan_state_done) {
            if (tsid == map->scan_results.transport_stream_id) {
                for (int c = 0; c < map->scan_results.num_programs; c++) {
                    if (programNum == (unsigned short)(map->scan_results.program_info[c].program_number)) {
                        channelNumber = c;
                        break;
                    }
                }
            }
        }
    }

    for (d = BLST_Q_FIRST(&g_decoders); d; d = BLST_Q_NEXT(d, link)) {
        set_channel(d, channelNumber);
        start_priming(d);
    }

    d = BLST_Q_FIRST(&g_decoders);
    if (d->channel && d->map) {
        ALOGV("activate(%p)", (void*)d);
        live_decode_activate(d->channel);
    }
}

static void change_channels(int dir)
{
    ALOGI("%s: Enter", __FUNCTION__);
    struct decoder *d;

    if (dir > 0) {
        struct decoder *prev_d;
        d = BLST_Q_FIRST(&g_decoders);
        if (!d) return;

        stop_decode(d);

        /* if we're priming, the channel change happens in the next prime */
        BLST_Q_REMOVE_HEAD(&g_decoders, link);
        BLST_Q_INSERT_TAIL(&g_decoders, d, link);

        /* start priming the last */
        d = BLST_Q_LAST(&g_decoders);
        prev_d = BLST_Q_PREV(d, link);
        if (!prev_d) prev_d = d; /* no primers */
        set_channel(d, prev_d->chNum+1);
        start_priming(d);

        /* start decoding the first */
        d = BLST_Q_FIRST(&g_decoders);
        if (d->channel && d->map) {
            ALOGV("activate(%p)", (void*)d);
            live_decode_activate(d->channel);
        }
    }
    else {
        struct decoder *first_d;

        /* start priming the first */
        first_d = BLST_Q_FIRST(&g_decoders);
        stop_decode(first_d);
        start_priming(first_d);

        /* stop priming the last and use it to decode */
        d = BLST_Q_LAST(&g_decoders);
        if (!d) return;
        BLST_Q_REMOVE(&g_decoders, d, link);
        BLST_Q_INSERT_HEAD(&g_decoders, d, link);
        stop_decode(d);

        /* start decoding the first */
        if (!set_channel(d, first_d->chNum == 0 ? g_total_channels-1: first_d->chNum-1)) {
            ALOGV("activate(%p)", (void*)d);
            start_priming(d);
            live_decode_activate(d->channel);
        }
    }
}

static void init_decoders(unsigned ch)
{
    ALOGI("%s: Enter", __FUNCTION__);
    struct decoder *d;

    for (d = BLST_Q_FIRST(&g_decoders); d; d = BLST_Q_NEXT(d, link)) {
        stop_decode(d);
    }

    for (d = BLST_Q_FIRST(&g_decoders); d; d = BLST_Q_NEXT(d, link)) {
        set_channel(d, ch++);
        start_priming(d);
    }

    d = BLST_Q_FIRST(&g_decoders);
    if (d->channel && d->map) {
        ALOGV("activate(%p)", (void*)d);
        live_decode_activate(d->channel);
    }
}

static void print_status(void)
{
    ALOGI("%s: Enter", __FUNCTION__);
    struct decoder *d;
    for (d = BLST_Q_FIRST(&g_decoders); d; d = BLST_Q_NEXT(d, link)) {
        NEXUS_VideoDecoderStatus status;
        ALOGV("%s: calling NEXUS_SimpleVideoDecoder_GetStatus", __FUNCTION__);
        NEXUS_SimpleVideoDecoder_GetStatus(live_decode_get_video_decoder(d->channel), &status);
        ALOGV("%s %p: %d/%d %d%%, pts %#x, diff %d",
            d == BLST_Q_FIRST(&g_decoders) ? "decode":"primer",
            (void*)d, status.fifoDepth, status.fifoSize, status.fifoSize ? status.fifoDepth * 100 / status.fifoSize : 0,
            status.pts, status.ptsStcDifference);
    }
}

static int tuneToChannel(int tsid, int programNum)
{
    ALOGI("%s: Enter", __FUNCTION__);

    int curarg = 1;

    live_decode_create_settings create_settings;
    live_decode_start_settings start_settings;
    unsigned video_pid = 0x1fff, audio_pid = 0x1fff;
    NEXUS_VideoCodec video_type = NEXUS_VideoCodec_eMpeg2;
    NEXUS_AudioCodec audio_type = NEXUS_AudioCodec_eAc3;

    bool prompt = false;
    struct client_state client_state;
    unsigned program = 0;
    unsigned timeout = 0, starttime;
#if B_REFSW_TR69C_SUPPORT
    b_tr69c_t tr69c;
#endif

    int n;

    memset(&pig_inc, 0, sizeof(pig_inc));
    memset(&client_state, 0, sizeof(client_state));
    live_decode_get_default_create_settings(&create_settings);
    live_decode_get_default_start_settings(&start_settings);
    binput_get_default_settings(&input_settings);

    tune_settings.source = channel_source_ofdm;
    tune_settings.mode = NEXUS_FrontendOfdmMode_eDvbt;

    start_settings.video.maxWidth = 1920;
    start_settings.video.maxHeight = 1080;

    create_settings.primed = false;
    ALOGV("%s: surfaceClientId=%d", __FUNCTION__, allocResults.surfaceClient[0].id);
    create_settings.window.surfaceClientId = allocResults.surfaceClient[0].id;
    decode = live_decode_create(&create_settings);
    for (unsigned int i=0; i<num_decodes; i++) {
        struct decoder *dt = (struct decoder *)BKNI_Malloc(sizeof(*dt));
        NEXUS_SimpleVideoDecoderHandle videoDecoder;
        memset(dt, 0, sizeof(*dt));
        ALOGV("%s: calling NEXUS_ParserBand_Open", __FUNCTION__);
        dt->parserBand = NEXUS_ParserBand_Open(NEXUS_ANY_ID);
        if (!dt->parserBand) {
            BKNI_Free(dt);
            continue;
        }
        dt->start_settings = start_settings;
        dt->video_pid = video_pid;
        dt->video_type = video_type;
        dt->audio_pid = audio_pid;
        dt->audio_type = audio_type;
        dt->channel = live_decode_create_channel(decode);
        BDBG_ASSERT(dt->channel);
        BLST_Q_INSERT_TAIL(&g_decoders, dt, link);
        ALOGV("decoder %d = %p", i, (void*)dt);

        videoDecoder = live_decode_get_video_decoder(dt->channel);

    }
    d = BLST_Q_FIRST(&g_decoders);
    if( !d ) {
        ALOGE("Unable to obtain parser band");
        return -1;
    }

    jumpToChannel(tsid, programNum);

#if B_REFSW_TR69C_SUPPORT
    tr69c = b_tr69c_init(live_decode_get_set_tr69c_info, BLST_Q_FIRST(&g_decoders)->channel);
#endif

    starttime = b_get_time();
    if (!g_total_channels) {
        if (!g_quiet) ALOGV("No channels found.");
    }
    else if (prompt) {
        if (!g_quiet) ALOGV("Press ENTER to change channel.");
    }
    else {
        if (!g_quiet) ALOGV("Use remote control to change channel. \"Stop\" or \"Clear\" will exit the app.");
    }

    return 0;
}

#if NEXUS_HAS_FRONTEND
static void *gui_thread(void *context)
{
    ALOGI("%s: Enter", __FUNCTION__);
    NEXUS_SurfaceMemory mem;
    unsigned count = 0;
    NEXUS_Rect rect = {0,0,0,0};

    BSTD_UNUSED(context);
    NEXUS_Surface_GetMemory(g_constellation.surface, &mem);

    memset(mem.buffer, 0, mem.pitch * g_constellation.createSettings.height);
    rect.width = g_constellation.createSettings.width/2;
    rect.height = g_constellation.createSettings.height/2;
    rect.y = 0;
    rect.x = rect.width;

    while (!g_constellation.done) {
#define MAX_SOFTDEC 32
        NEXUS_FrontendSoftDecision softdec[MAX_SOFTDEC];
        size_t num;
        int rc;

        if (!count) {
            int x, y;
            for (y=rect.y;y<rect.y+rect.height;y++) {
                uint32_t *ptr = (uint32_t *)((uint8_t*)mem.buffer + mem.pitch*y);
                for (x=rect.x;x<rect.x+rect.width;x++) {
                    ptr[x] = 0xAA888888;
                }
            }
        }
        rc = NEXUS_Frontend_ReadSoftDecisions(g_constellation.frontend, softdec, MAX_SOFTDEC, &num);
        if (!rc) {
            unsigned i;
            for (i=0;i<num;i++) {
                unsigned x = rect.x + (rect.width * (softdec[i].i+32768)) / 65536;
                unsigned y = rect.y + (rect.height * (softdec[i].q+32768)) / 65536;
                uint32_t *ptr = (uint32_t *)((uint8_t*)mem.buffer + mem.pitch*y);

                ptr = &ptr[x];
                ptr[0] = ptr[1] = 0xFFBBBBBB;
                ptr = (uint32_t *)((uint8_t*)ptr + mem.pitch);
                ptr[0] = ptr[1] = 0xFFBBBBBB;
                count++;
            }
            NEXUS_Surface_Flush(g_constellation.surface);

            rc = NEXUS_SurfaceClient_SetSurface(g_constellation.surfaceClient, g_constellation.surface);
            BDBG_ASSERT(!rc);
            rc = BKNI_WaitForEvent(g_constellation.displayedEvent, 5000);
            BDBG_ASSERT(!rc);

            if (count > 4000) count = 0;
        }
    }
    return NULL;
}
#endif

static int gui_init(NEXUS_FrontendHandle frontend, NEXUS_SurfaceClientHandle surfaceClient, enum gui gui)
{
    ALOGI("%s: Enter", __FUNCTION__);
    int rc;
    NEXUS_SurfaceClientSettings client_settings;
    NEXUS_SurfaceMemory mem;

    memset(&g_constellation, 0, sizeof(g_constellation));
    g_constellation.frontend = frontend;
    g_constellation.surfaceClient = surfaceClient;

    BKNI_CreateEvent(&g_constellation.displayedEvent);

    NEXUS_SurfaceClient_GetSettings(g_constellation.surfaceClient, &client_settings);
    client_settings.displayed.callback = complete2;
    client_settings.displayed.context = g_constellation.displayedEvent;
    client_settings.windowMoved.callback = complete2;
    client_settings.windowMoved.context = g_constellation.displayedEvent;
    rc = NEXUS_SurfaceClient_SetSettings(g_constellation.surfaceClient, &client_settings);
    BDBG_ASSERT(!rc);

    if (gui > gui_no_alpha_hole) {
        ALOGE("%s: NEXUS_Surface_Create", __FUNCTION__);
        NEXUS_Surface_GetDefaultCreateSettings(&g_constellation.createSettings);
        g_constellation.createSettings.pixelFormat = NEXUS_PixelFormat_eA8_R8_G8_B8;
        g_constellation.createSettings.width = 720;
        g_constellation.createSettings.height = 480;
        g_constellation.surface = NEXUS_Surface_Create(&g_constellation.createSettings);

        /* black screen, even if no constellation */
        NEXUS_Surface_GetMemory(g_constellation.surface, &mem);
        memset(mem.buffer, 0, mem.pitch * g_constellation.createSettings.height);
        NEXUS_Surface_Flush(g_constellation.surface);
        rc = NEXUS_SurfaceClient_SetSurface(g_constellation.surfaceClient, g_constellation.surface);
        BDBG_ASSERT(!rc);
        rc = BKNI_WaitForEvent(g_constellation.displayedEvent, 5000);
        BDBG_ASSERT(!rc);

#if NEXUS_HAS_FRONTEND
        if (gui == gui_constellation && frontend) {
            rc = pthread_create(&g_constellation.thread, NULL, gui_thread, NULL);
            BDBG_ASSERT(!rc);
        }
#endif
    }

    return 0;
}

static void gui_uninit(void)
{
    ALOGI("%s: Enter", __FUNCTION__);
    NEXUS_SurfaceClientSettings client_settings;
    g_constellation.done = true;
#if NEXUS_HAS_FRONTEND
    if (g_constellation.thread) {
        pthread_join(g_constellation.thread, NULL);
    }
#endif
    if (g_constellation.surface) {
        NEXUS_Surface_Destroy(g_constellation.surface);
    }
    NEXUS_SurfaceClient_GetSettings(g_constellation.surfaceClient, &client_settings);
    client_settings.displayed.callback = NULL;
    client_settings.windowMoved.callback = NULL;
    NEXUS_SurfaceClient_SetSettings(g_constellation.surfaceClient, &client_settings);

    BKNI_DestroyEvent(g_constellation.displayedEvent);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env;

    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

    //TODO register native methods

    ALOGE("LOADING FROM JNI!!");


    return JNI_VERSION_1_6;
}

extern "C" {
JNIEXPORT jint JNICALL
Java_com_broadcom_tvinput_BcmTunerJniBridge_initializeTuner(JNIEnv *env, jobject caller) {
    ALOGI("Initalizing tuner hardware");
    const char *freq_list = "618";

    NxClient_GetDefaultJoinSettings(&joinSettings);
    snprintf(joinSettings.name, NXCLIENT_MAX_NAME, "%s", TAG);
    ALOGV("%s: calling NxClient_Join", __FUNCTION__);
    rc = NxClient_Join(&joinSettings);
    if (rc) return -1;

    //init frontend
    ALOGV("%s: calling NEXUS_Platform_InitFrontend", __FUNCTION__);
	rc = NEXUS_Platform_InitFrontend();
	if ( rc != BERR_SUCCESS ) {
		return -1;
	}

    ALOGV("%s: calling NxClient_GetDefaultAllocSettings", __FUNCTION__);
    NxClient_GetDefaultAllocSettings(&allocSettings);
    allocSettings.surfaceClient = 1;
    ALOGV("%s: calling NxClient_Alloc", __FUNCTION__);
    rc = NxClient_Alloc(&allocSettings, &allocResults);
    if (rc) return BERR_TRACE(rc);
    ALOGV("%s: calling NEXUS_SurfaceClient_Acquire surfaceClientId=%d", __FUNCTION__, allocResults.surfaceClient[0].id);
    surfaceClient = NEXUS_SurfaceClient_Acquire(allocResults.surfaceClient[0].id);
    if (surfaceClient) {
        ALOGV("%s: calling NEXUS_SurfaceClient_AcquireVideoWindow", __FUNCTION__);
        video_sc = NEXUS_SurfaceClient_AcquireVideoWindow(surfaceClient, 0);
        if (contentMode != NEXUS_VideoWindowContentMode_eMax) {
            NEXUS_SurfaceClientSettings settings;
            NEXUS_SurfaceClient_GetSettings(video_sc, &settings);
            settings.composition.contentMode = contentMode;
            NEXUS_SurfaceClient_SetSettings(video_sc, &settings);
        }
        if (pig_inc.x) {
            b_pig_init(video_sc);
        }
    }

    input = binput_open(&input_settings);

    get_default_channels(&tune_settings, &freq_list);
    parse_channels(&tune_settings, freq_list);

    {
        unsigned scanning = 0;
        bool waiting = false;
        /* start scanning as many as possible */
        while (1) {
            bool done = true;
            for (map = BLST_Q_FIRST(&g_channel_map); map; map = BLST_Q_NEXT(map, link)) {
                struct channel_map *nextmap;
                if (map->scan_state == channel_map::scan_state_done) {
                    continue;
                }
                done = false;
                if (map->scan_state == channel_map::scan_state_pending && !waiting) {
                    bchannel_scan_start_settings scan_settings;
                    bchannel_scan_get_default_start_settings(&scan_settings);
                    scan_settings.tune = map->tune;
                    if (!map->scan) {
                        map->scan = bchannel_scan_start(&scan_settings);
                        if (!map->scan) {
                            /* start fails if there are no resources.
                            if there's no scan going, there's no reason to expect new resources.
                            if there's a scan going, don't try again until one scan has finished. */
                            if (!scanning) {
                                map->scan_state = channel_map::scan_state_done;
                            }
                            else {
                                waiting = true;
                            }
                        }
                    }
                    else {
                        bchannel_scan_restart(map->scan, &scan_settings);
                    }

                    if (map->scan) {
                        if (!g_quiet) ALOGV("Scanning %s...", map->name);
                        map->scan_state = channel_map::scan_state_started;
                        map->scan_start = b_get_time();
                        scanning++;
                    }

                }
                if (!map->scan) {
                    continue;
                }

                rc = bchannel_scan_get_results(map->scan, &map->scan_results);
                if (rc == NEXUS_NOT_AVAILABLE) {
                    if (b_get_time() - map->scan_start < 7500) {
                        continue;
                    }
                }

                map->scan_state = channel_map::scan_state_done;
                waiting = false;
                scanning--;
                if (!g_quiet) ALOGV("%d programs found on %s", map->scan_results.num_programs, map->name);
#if NEXUS_HAS_FRONTEND
                if (!map->scan_results.num_programs && map->tune.source != channel_source_streamer) {
                    NEXUS_FrontendHandle frontend;
                    NEXUS_ParserBand parserBand;
                    NEXUS_FrontendFastStatus status;
                    bchannel_scan_get_resources(map->scan, &frontend, &parserBand);
                    //ALOGV("%s: calling NEXUS_Frontend_GetFastStatus", __FUNCTION__);
                    rc = NEXUS_Frontend_GetFastStatus(frontend, &status);
                    if (rc) {
                        BERR_TRACE(rc);
                    }
                    else {
                        const char *lockstr[NEXUS_FrontendLockStatus_eMax] = {"unknown", "unlocked", "locked", "no signal"};
                        if (!g_quiet) ALOGV("  frontend lock status: %s", lockstr[status.lockStatus]);
                    }
                }
#endif

                g_total_channels += map->scan_results.num_programs;

                /* give this scan to another if possible */
                for (nextmap = BLST_Q_NEXT(map, link); nextmap; nextmap = BLST_Q_NEXT(nextmap, link)) {
                    if (!nextmap->scan && !map->scan_state == channel_map::scan_state_pending) {
                        nextmap->scan = map->scan;
                        map->scan = NULL;
                        break;
                    }
                }
                if (!nextmap) {
                    /* no longer needed */
                    bchannel_scan_stop(map->scan);
                    map->scan = NULL;
                }
            }
            if (done) break;
        }

        for (map = BLST_Q_FIRST(&g_channel_map); map; map = BLST_Q_NEXT(map, link)) {
            if (map->scan) {
                bchannel_scan_stop(map->scan);
                map->scan = NULL;
            }
        }
    }
    return 0;
}
}

extern "C" {
JNIEXPORT jstring JNICALL
Java_com_broadcom_tvinput_BcmTunerJniBridge_getScanResults(JNIEnv *env, jobject caller) {
    ALOGI("%s: Enter", __FUNCTION__);
    unsigned int channelNumber = 0;
    std::stringstream ss;

    if (g_total_channels > 0) {
        ss << "[";
        for (map = BLST_Q_FIRST(&g_channel_map); map; map = BLST_Q_NEXT(map, link)) {

            if (map->scan_state == channel_map::scan_state_done) {
                channel_map *data = map;
                if (data->scan_results.num_programs > 0) {
                    ss << "{";
                    ss << "\"name\":" << "\"" << data->name << "\"";
                    ss << ",\"freq\":" << data->tune.freq;
                    ss << ",\"version\":" << (short)(data->scan_results.version); //unint8
                    ss << ",\"tsid\":" << (unsigned short)data->scan_results.transport_stream_id; //uint16_t

                    ss << ",\"program_list\":[";
                    for (int c = 0; c < data->scan_results.num_programs; c++) {
                        ss << "{";
                        ss << "\"channel_number\":" << channelNumber;
                        ss << ",\"program_number\":" << (unsigned short)(data->scan_results.program_info[c].program_number); //uint16_t
                        ss << ",\"program_pid\":" << (unsigned short)data->scan_results.program_info[c].map_pid; //uint16_t
                        ss << ",\"program_version\":" << (short)(data->scan_results.program_info[c].version); //uint8_t
                        ss << ",\"pcr_pid\":" << (unsigned short)data->scan_results.program_info[c].pcr_pid; //uint16_t
                        ss << ",\"ca_pid\":" << (unsigned short)(data->scan_results.program_info[c].ca_pid); //uint16_t

                        ss << ",\"video_pids\":[";
                        for (int vid = 0; vid < data->scan_results.program_info[c].num_video_pids; vid++) {
                            ss << "{";
                            ss << "\"pid\":" << (unsigned short)(data->scan_results.program_info[c].video_pids[vid].pid); //uint16_t
                            ss << ",\"codec\":\"" << lookup_name(g_videoCodecStrs, data->scan_results.program_info[c].video_pids[vid].codec) << "\""; //NEXUS_VideoCodec
                            ss << ",\"ca_pid\":" << (unsigned short)(data->scan_results.program_info[c].video_pids[vid].ca_pid); //uint16_t
                            ss << "}";
                            if (vid + 1 < data->scan_results.program_info[c].num_video_pids) ss << ",";
                        }
                        ss << "]";

                        ss << ",\"audio_pids\":[";
                        for (int aid = 0; aid < data->scan_results.program_info[c].num_audio_pids; aid++) {
                            ss << "{";
                            ss << "\"pid\":" << (unsigned short)(data->scan_results.program_info[c].audio_pids[aid].pid); //uint16_t
                            ss << ",\"codec\":\"" << lookup_name(g_audioCodecStrs, data->scan_results.program_info[c].audio_pids[aid].codec) << "\""; //NEXUS_AudioCodec
                            ss << ",\"ca_pid\":" << (unsigned short)(data->scan_results.program_info[c].audio_pids[aid].ca_pid); //uint16_t
                            ss << "}";
                            if (aid + 1 < data->scan_results.program_info[c].num_audio_pids) ss << ",";
                        }
                        ss << "]";

                        ss << ",\"other_pids\":[";
                        for (int oid = 0; oid < data->scan_results.program_info[c].num_other_pids; oid++) {
                            ss << "{";
                            ss << ",\"pid\":" << (unsigned short)(data->scan_results.program_info[c].other_pids[oid].pid); //uint16_t
                            ss << ",\"stream_type\":" << (unsigned int)(data->scan_results.program_info[c].other_pids[oid].stream_type); //unsigned
                            ss << ",\"ca_pid\":" << (unsigned short)(data->scan_results.program_info[c].other_pids[oid].ca_pid); //uint16_t
                            ss << "}";
                            if (oid + 1 < data->scan_results.program_info[c].num_other_pids) ss << ",";
                        }
                        ss << "]";
                        ss << "}";
                        if (c + 1 < data->scan_results.num_programs) ss << ",";

                        channelNumber++;
                    }
                    ss << "]";
                    ss << "},";
                }
            }
        }
        ss.seekp(-1, ss.cur);
        ss << "]";
    }

    ALOGV("channelData=%s",ss.str().c_str());
    return  env->NewStringUTF(ss.str().c_str());
}
}

extern "C" {
JNIEXPORT jint JNICALL
Java_com_broadcom_tvinput_BcmTunerJniBridge_getMsgFromJni(JNIEnv *env, jobject caller) {

    // TODO
    ALOGI("RECEIVED call from JAVA");

    jclass javaBridge = env->FindClass("com/broadcom/tvinput/BcmTunerJniBridge");
    if(javaBridge == NULL) {
        ALOGE("cannot find class");
    }
    jmethodID mId = env->GetStaticMethodID(javaBridge, "testCallToJava", "()V");
    if(mId == NULL){
        ALOGE("cannot find method");
    }
    env->CallStaticVoidMethod(javaBridge, mId);
    return 0;
}
}

extern "C" {
JNIEXPORT jint JNICALL
Java_com_broadcom_tvinput_BcmTunerJniBridge_tune(JNIEnv *env, jobject caller, jint tsid, jint pNum) {
    ALOGI("%s: Enter tsid=%d, pNum=%d", __FUNCTION__, (int)tsid, (int)pNum);
    return tuneToChannel((int)tsid, (int)pNum);
    ALOGE("%s: Exit", __FUNCTION__);

    return 0;
}
}

extern "C" {
JNIEXPORT jint JNICALL
Java_com_broadcom_tvinput_BcmTunerJniBridge_initGUI(JNIEnv *env, jobject caller) {
    ALOGI("%s: Enter", __FUNCTION__);

    if (tune_settings.source == channel_source_streamer) {
        frontend = (struct frontend *)BKNI_Malloc(sizeof(*frontend));
        memset(frontend, 0, sizeof(*frontend));
        BLST_Q_INSERT_TAIL(&g_frontends, frontend, link);
    }
#if NEXUS_HAS_FRONTEND
    else {
        for (unsigned int i=0;i<num_decodes;i++) {
            NEXUS_FrontendAcquireSettings settings;
            struct frontend *frontend;

            frontend = (struct frontend *)BKNI_Malloc(sizeof(*frontend));
            memset(frontend, 0, sizeof(*frontend));

            ALOGV("%s: calling NEXUS_Frontend_GetDefaultAcquireSettings", __FUNCTION__);
            NEXUS_Frontend_GetDefaultAcquireSettings(&settings);
            settings.capabilities.qam = (tune_settings.source == channel_source_qam);
            settings.capabilities.ofdm = (tune_settings.source == channel_source_ofdm);
            settings.capabilities.satellite = (tune_settings.source == channel_source_sat);
            ALOGV("%s: calling NEXUS_Frontend_Acquire", __FUNCTION__);
            frontend->frontend = NEXUS_Frontend_Acquire(&settings);
            if (!frontend->frontend) {
                BKNI_Free(frontend);
                //num_decodes = i; /* reduce number of decodes to number of frontends for simpler app logic */
                break;
            }
            BLST_Q_INSERT_TAIL(&g_frontends, frontend, link);
        }
    }
#endif

    frontend = BLST_Q_FIRST(&g_frontends);
    if (!frontend) {
        ALOGE("Unable to find capable frontend");
        return -1;
    }
    gui_init(frontend->frontend, surfaceClient, gui);

    ALOGE("%s: Exit", __FUNCTION__);
    return 0;
}
}

extern "C" {
JNIEXPORT jint JNICALL
Java_com_broadcom_tvinput_BcmTunerJniBridge_uninitGUI(JNIEnv *env, jobject caller) {
    ALOGI("%s: Enter", __FUNCTION__);

    gui_uninit();
    while ((frontend = BLST_Q_FIRST(&g_frontends))) {
        if (frontend->frontend) {
            ALOGV("%s: calling NEXUS_Frontend_Release", __FUNCTION__);
            NEXUS_Frontend_Release(frontend->frontend);
        }
        BLST_Q_REMOVE(&g_frontends, frontend, link);
        BKNI_Free(frontend);
    }
    ALOGE("%s: Exit", __FUNCTION__);
    return 0;
}
}

extern "C" {
JNIEXPORT jint JNICALL
Java_com_broadcom_tvinput_BcmTunerJniBridge_stop(JNIEnv *env, jobject caller) {
    ALOGI("%s: Enter", __FUNCTION__);

    while ((d = BLST_Q_FIRST(&g_decoders))) {
        stop_decode(d);
        if (d->channel) {
            live_decode_destroy_channel(d->channel);
        }
        ALOGV("%s: calling NEXUS_ParserBand_Close", __FUNCTION__);
        NEXUS_ParserBand_Close(d->parserBand);
        BLST_Q_REMOVE(&g_decoders, d, link);
        BKNI_Free(d);
    }
    live_decode_destroy(decode);

    ALOGE("%s: Exit", __FUNCTION__);
    return 0;
}
}

extern "C" {
JNIEXPORT jint JNICALL
Java_com_broadcom_tvinput_BcmTunerJniBridge_uninitializeTuner(JNIEnv *env, jobject caller) {
    ALOGI("%s: Enter", __FUNCTION__);

    uninit_channels();

#if B_REFSW_TR69C_SUPPORT
        b_tr69c_uninit(tr69c);
#endif

    NxClient_Free(&allocResults);
    binput_close(input);
    NxClient_Uninit();

    ALOGE("%s: Exit", __FUNCTION__);
    return 0;
}
}
