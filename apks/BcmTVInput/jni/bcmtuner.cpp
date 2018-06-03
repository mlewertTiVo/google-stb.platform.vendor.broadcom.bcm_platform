/******************************************************************************
 * (c) 2018 Broadcom
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
 *
 *****************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "BcmTVInput-JNI"

#include <nativehelper/JNIHelp.h>
#include <utils/Log.h>
#include <system/window.h>
#include <android/native_window_jni.h>
#include <utils/Mutex.h>

// TODO: Replace by HIDL soon....
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

#include <bcm/hardware/dspsvcext/1.0/IDspSvcExt.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sstream>
#include <iomanip>
#include <sys/time.h>

using namespace android;
using namespace android::hardware;
using namespace bcm::hardware::dspsvcext::V1_0;

typedef void(*sb_geometry_cb)(void *context, unsigned int x, unsigned int y,
                                    unsigned int width, unsigned int height);

static Mutex svclock;
#define ATTEMPT_PAUSE_USEC 500000
#define MAX_ATTEMPT_COUNT  4
static const sp<IDspSvcExt> idse(void) {
   static sp<IDspSvcExt> idse = NULL;
   Mutex::Autolock _l(svclock);
   int c = 0;

   if (idse != NULL) {
      return idse;
   }

   do {
      idse = IDspSvcExt::getService();
      if (idse != NULL) {
         return idse;
      }
      usleep(ATTEMPT_PAUSE_USEC);
      c++;
   }
   while(c < MAX_ATTEMPT_COUNT);
   // can't get interface.
   return NULL;
}

#define NUM_DECODES 1
#define INVALID_PID 0x1FFFF
#define INVALID_HANDLE 0xdeadbeef

class SdbGeomCb : public IDspSvcExtSdbGeomCb {
public:
   SdbGeomCb(sb_geometry_cb cb, void *cb_data): geom_cb(cb), geom_ctx(cb_data) {};
   sb_geometry_cb geom_cb;
   void *geom_ctx;
   Return<void> onGeom(int32_t i, const DspSvcExtGeom& geom);
};

static sp<SdbGeomCb> gSdbGeomCb = NULL;
static ANativeWindow *native_window = NULL;
static native_handle_t *native_handle = NULL;
static live_decode_t gDecode;
static NEXUS_SurfaceClientHandle gSurfaceClient, gVideo_sc;
static struct btune_settings gTuneSettings;
static unsigned int gSurfaceId;
static unsigned g_totalChannels;
static bool gScanInProgress = false;

Return<void> SdbGeomCb::onGeom(int32_t i, const DspSvcExtGeom& geom) {
   if (geom_cb) {
      if (i != 0) {
         ALOGW("SdbGeomCb::onGeom(%d), but registered for 0", i);
      } else {
         geom_cb(NULL, geom.geom_x, geom.geom_y, geom.geom_w, geom.geom_h);
      }
   }
   return Void();
}

static unsigned b_get_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000 + tv.tv_usec/1000;
}

struct Channel_map {
    BLST_Q_ENTRY(Channel_map) link;
    char name[32];
    struct btune_settings tune;
    bchannel_scan_t scan;
    enum { SCAN_STATE_PENDING, SCAN_STATE_STARTED, SCAN_STATE_DONE } scanState;
    tspsimgr_scan_results scanResults;
    unsigned scanStart;
};
static BLST_Q_HEAD(channellist_t, Channel_map) g_channelMap = BLST_Q_INITIALIZER(g_channelMap);

struct Frontend {
    BLST_Q_ENTRY(Frontend) link;
    NEXUS_FrontendHandle frontend;
    struct Channel_map *map;
    unsigned refcnt;
    bool ofdm, qam, sat;
};
static BLST_Q_HEAD(frontendlist_t, Frontend) g_frontends;

struct Decoder {
    BLST_Q_ENTRY(Decoder) link;
    NEXUS_ParserBand parserBand;
    live_decode_channel_t channel;
    live_decode_start_settings start_settings;
    unsigned video_pid, audio_pid; /* scan override */
    NEXUS_VideoCodec video_type;
    NEXUS_AudioCodec audio_type;

    unsigned chNum; /* global number */
    struct Frontend *frontend;
    struct Channel_map *map;
    unsigned program; /* relative to map */
};
static BLST_Q_HEAD(decoderlist_t, Decoder) g_decoders;
static bool g_quiet = false;


struct client_state
{
    bool done;
};

static void insertChannel(const struct btune_settings *tune)
{
    bool addChannel = true;
    struct Channel_map *map;
    struct Channel_map *c = (Channel_map *)BKNI_Malloc(sizeof(*c));
    BKNI_Memset(c, 0, sizeof(*c));
    c->tune = *tune;
    c->scanState = Channel_map::SCAN_STATE_PENDING;

    for (map = BLST_Q_FIRST(&g_channelMap); map && addChannel; map = BLST_Q_NEXT(map, link)) {
        if (map->tune.freq == c->tune.freq) {
            ALOGD("Ignoring duplicated frequency: %d map: %p",c->tune.freq, (void *)map);
            //map->scanState = Channel_map::SCAN_STATE_PENDING;
            addChannel = false;
            BKNI_Free(c);
        }
    }

    if (addChannel) {
        bchannel_source_print(c->name, sizeof(c->name), tune);
        BLST_Q_INSERT_TAIL(&g_channelMap, c, link);
        ALOGD("<> %s() %s scanState:%s map:%p", __FUNCTION__, c->name,
            c->scanState==Channel_map::SCAN_STATE_PENDING?"Pending":
            c->scanState==Channel_map::SCAN_STATE_STARTED?"Started":"Done",(void *)c);
    }
}

static void uninit_channels(void)
{
    ALOGD("<>%s()", __FUNCTION__);
    struct Channel_map *map;
    while ((map = BLST_Q_FIRST(&g_channelMap))) {
        BLST_Q_REMOVE(&g_channelMap, map, link);
        BKNI_Free(map);
    }
}

static void parseChannels(const struct btune_settings *tune, const char *freq_list)
{
    ALOGD("> %s()", __FUNCTION__);
#if NEXUS_HAS_FRONTEND
    unsigned first_channel = 0, last_channel = 0;
    const unsigned _mhz = 1000000;
#endif
    if (tune->source == channel_source_streamer) {
        insertChannel(tune);
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
            ALOGD("< %s()", __FUNCTION__);
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
            insertChannel(&t);
        }
        if (!first_channel) {
            freq_list = strchr(freq_list, ',');
            if (freq_list) freq_list++;
        }
    }
#else
    BSTD_UNUSED(freq_list);
#endif
    ALOGD("< %s()", __FUNCTION__);
}

static void stopDecode(struct Decoder *d)
{
    if (!d->channel) return;

    ALOGD("> %s() decoder: %p channel: %p frontend: %p(refcnt:%d) map: %p program: %i",
        __FUNCTION__,(void*)d, (void*)d->channel,
        d->frontend?(void*)d->frontend->frontend:NULL, d->frontend?d->frontend->refcnt:0,
        (void*)d->map, d->program);
    live_decode_stop_channel(d->channel);
    if (d->frontend) {
        BDBG_ASSERT(d->frontend->map);
        BDBG_ASSERT(d->frontend->refcnt);
        if (--d->frontend->refcnt == 0) {
            d->frontend->map = NULL;
        }
        d->frontend = NULL;
    }
    ALOGD("< %s()", __FUNCTION__);
}

static int setChannel(struct Decoder *d, unsigned chNum)
{
    ALOGD("> %s( decoder: %p chNum: %i )", __FUNCTION__, (void *)d, chNum);
    struct Channel_map *map;
    if (chNum >= g_totalChannels && g_totalChannels) {
        ALOGE("%s() Channel number is outside of total channels %i > %i",__FUNCTION__, chNum, g_totalChannels);
        chNum = chNum % g_totalChannels;
    }
    d->chNum = chNum;
    for (map = BLST_Q_FIRST(&g_channelMap); map; map = BLST_Q_NEXT(map, link)) {
        if (chNum < map->scanResults.num_programs) break;
        chNum -= map->scanResults.num_programs;
    }
    if (!map) {
        d->map = NULL;
        ALOGE("< %s() No channel map found!", __FUNCTION__);
        return -1;
    }
    d->map = map;
    d->program = chNum;
    ALOGD("< setChannel(decoder:%p, chNum:%d) map:%p program:%d", (void*)d, chNum, (void*)d->map, d->program);
    return 0;
}

static int startPriming(struct Decoder *d)
{
    ALOGD("> %s()", __FUNCTION__);
    struct Channel_map *map;
    struct Frontend *frontend;
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
    if (d->video_pid != INVALID_PID) {
        d->start_settings.video.pid = d->video_pid;
        d->start_settings.video.codec = d->video_type;
    }
    else {
        d->start_settings.video.pid = map->scanResults.program_info[d->program].video_pids[0].pid;
        d->start_settings.video.codec = map->scanResults.program_info[d->program].video_pids[0].codec;
    }
    if (d->audio_pid != INVALID_PID) {
        d->start_settings.audio.pid = d->audio_pid;
        d->start_settings.audio.codec = d->audio_type;
    }
    else {
        d->start_settings.audio.pid = map->scanResults.program_info[d->program].audio_pids[0].pid;
        d->start_settings.audio.codec = map->scanResults.program_info[d->program].audio_pids[0].codec;
    }
    d->start_settings.pcr_pid = map->scanResults.program_info[d->program].pcr_pid;
    rc = live_decode_start_channel(d->channel, &d->start_settings);
    if (rc) return BERR_TRACE(rc);

    ALOGD("%s: v:%i a:%i", __FUNCTION__, d->start_settings.video.pid, d->start_settings.audio.pid);
    ALOGD("< startPriming(%p) %p frontend:%p(%d) parserBand:%p, map:%p program:%d", (void*)d, (void*)d->channel, (void*)d->frontend->frontend, d->frontend->refcnt, (void*)d->parserBand, (void*)d->map, d->program);

    return 0;
}

static void jumpToChannel(int tsid, int programNum){
    struct Decoder *d;
    struct Channel_map *map;
    int channelNumber = 0;

    ALOGD("> %s() tsid=%i, pNum=%i", __FUNCTION__, tsid, programNum);

    for (d = BLST_Q_FIRST(&g_decoders); d; d = BLST_Q_NEXT(d, link)) {
        stopDecode(d);
    }

    //Find the channel number matching tsid and programNum
    for (map = BLST_Q_FIRST(&g_channelMap); map; map = BLST_Q_NEXT(map, link)) {
        if (map->scanState == Channel_map::SCAN_STATE_DONE) {
            if (tsid == map->scanResults.transport_stream_id) {
                for (int c = 0; c < map->scanResults.num_programs; c++) {
                    if (programNum == (unsigned short)(map->scanResults.program_info[c].program_number)) {
                        channelNumber = c;
                        break;
                    }
                }
            }
        }
    }

    for (d = BLST_Q_FIRST(&g_decoders); d; d = BLST_Q_NEXT(d, link)) {
        setChannel(d, channelNumber);
        startPriming(d);
    }

    d = BLST_Q_FIRST(&g_decoders);
    if (d->channel && d->map) {
        ALOGD("%s() activate: %p", __FUNCTION__, (void*)d);
        live_decode_activate(d->channel);
    }

    ALOGD("< %s()", __FUNCTION__);
}

static int aquireTuner(void)
{
    struct Frontend *frontend;
    ALOGD("> %s()", __FUNCTION__);

    if (gTuneSettings.source == channel_source_streamer) {
        frontend = (struct Frontend *)BKNI_Malloc(sizeof(*frontend));
        memset(frontend, 0, sizeof(*frontend));
        BLST_Q_INSERT_TAIL(&g_frontends, frontend, link);
    }
#if NEXUS_HAS_FRONTEND
    else {
        // Check if a frontend has already been aquired.
        frontend = BLST_Q_FIRST(&g_frontends);
        if( frontend ) {
            ALOGV("%s(): Frontend of type [%s] already aquired.", __FUNCTION__,
                frontend->ofdm?"OFDM":frontend->qam?"QAM":"SAT");
        }
        else {
            for (unsigned int i=0;i<NUM_DECODES;i++) {
                NEXUS_FrontendAcquireSettings settings;

                frontend = (struct Frontend *)BKNI_Malloc(sizeof(*frontend));
                memset(frontend, 0, sizeof(*frontend));

                NEXUS_Frontend_GetDefaultAcquireSettings(&settings);
                frontend->qam  = settings.capabilities.qam = (gTuneSettings.source == channel_source_qam);
                frontend->ofdm = settings.capabilities.ofdm = (gTuneSettings.source == channel_source_ofdm);
                frontend->sat  = settings.capabilities.satellite = (gTuneSettings.source == channel_source_sat);
                frontend->frontend = NEXUS_Frontend_Acquire(&settings);
                if (!frontend->frontend) {
                    ALOGE("%s() No Frontend!!", __FUNCTION__);
                    BKNI_Free(frontend);
                    break;
                }

                if (frontend->ofdm) {
                    gTuneSettings.mode = NEXUS_FrontendOfdmMode_eDvbt;
                } else if (frontend->qam) {
                    gTuneSettings.mode = NEXUS_FrontendOfdmMode_eDvbc2;
                }

                ALOGV("%s(): Frontend of type [%s] aquired.", __FUNCTION__,
                    frontend->ofdm?"OFDM":frontend->qam?"QAM":"SAT");

                BLST_Q_INSERT_TAIL(&g_frontends, frontend, link);
            }
        }
    }
#endif

    frontend = BLST_Q_FIRST(&g_frontends);
    if (!frontend) {
        ALOGE("Unable to find capable frontend");
        return -1;
    }

    ALOGD("< %s()", __FUNCTION__);
    return 0;
}


static int tuneToChannel(int tsid, int programNum)
{
    live_decode_create_settings create_settings;
    live_decode_start_settings start_settings;
    NEXUS_VideoCodec video_type = NEXUS_VideoCodec_eMpeg2;
    NEXUS_AudioCodec audio_type = NEXUS_AudioCodec_eAc3;
    struct client_state client_state;
#if B_REFSW_TR69C_SUPPORT
    b_tr69c_t tr69c;
#endif

    ALOGD("> %s()", __FUNCTION__);

    memset(&client_state, 0, sizeof(client_state));

    aquireTuner();

    live_decode_get_default_create_settings(&create_settings);
    live_decode_get_default_start_settings(&start_settings);

    start_settings.video.maxWidth = 1920;
    start_settings.video.maxHeight = 1080;

    create_settings.primed = false;

    ALOGD("%s() surfaceClientId: %d", __FUNCTION__, gSurfaceId);
    create_settings.window.surfaceClientId = gSurfaceId;
    create_settings.window.id = 0;

    if (!gDecode) {
        gDecode = live_decode_create(&create_settings);
    }

    for (unsigned int i=0; i<NUM_DECODES; i++) {
        struct Decoder *dt = NULL;

        dt = BLST_Q_FIRST(&g_decoders);

        if( dt == NULL ) {
            // TODO: Re-use parserband
            dt = (struct Decoder *)BKNI_Malloc(sizeof(*dt));
            memset(dt, 0, sizeof(*dt));
            dt->parserBand = NEXUS_ParserBand_Open(NEXUS_ANY_ID);
            if (!dt->parserBand) {
                BKNI_Free(dt);
                ALOGE("%s() Failed to obtain parser band.", __FUNCTION__);
                continue;
            }

            dt->start_settings = start_settings;
            dt->video_pid = INVALID_PID;
            dt->video_type = video_type;
            dt->audio_pid = INVALID_PID;
            dt->audio_type = audio_type;
            dt->channel = live_decode_create_channel(gDecode);
            BDBG_ASSERT(dt->channel);
            BLST_Q_INSERT_TAIL(&g_decoders, dt, link);
        }
        ALOGV("%s() decoder[%d]: %p parserBand: %p videoDecoder: %p",__FUNCTION__, i,
            (void*)dt, (void*)dt->parserBand, (void *)live_decode_get_video_decoder(dt->channel));
    }

    jumpToChannel(tsid, programNum);

#if B_REFSW_TR69C_SUPPORT
    tr69c = b_tr69c_init(live_decode_get_set_tr69c_info, BLST_Q_FIRST(&g_decoders)->channel);
#endif

    ALOGD("< %s()", __FUNCTION__);
    return 0;
}

static void sbGeometryUpdate(void * cntx, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
    NEXUS_SurfaceComposition comp;
    ALOGD("> %s (%p,%d,%d,%d,%d)", __FUNCTION__,cntx,x,y,w,h);

    ALOGD("<> surfaceClient: %p id: %d",gVideo_sc, gSurfaceId);

    if (gVideo_sc) {
        NxClient_GetSurfaceClientComposition(gSurfaceId, &comp);
        if (comp.position.x != (int)x || comp.position.y != (int)y ||
            comp.position.width != w  || comp.position.height != h) {

            comp.position.x = x;
            comp.position.y = y;
            comp.position.width = w;
            comp.position.height = h;
            NEXUS_Error rc;
            rc = NxClient_SetSurfaceClientComposition(gSurfaceId, &comp);
            if (rc) {
                ALOGE("Failed to apply new composition (%d)", rc);
                return;
            }
        }
    }
    else {
        ALOGD("Video surface client has not been created yet.");
    }

    ALOGD("< %s()", __FUNCTION__);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env;

    (void)reserved;

    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

    //TODO register native methods
    ALOGD("LOADING FROM JNI!!");

    return JNI_VERSION_1_6;
}

extern "C" {
JNIEXPORT jint JNICALL
Java_com_broadcom_tvinput_BcmTunerJniBridge_servicesScan(JNIEnv *env, jobject caller) {
    NEXUS_Error rc;
    struct Channel_map *map;
    // const char *ofdm_dvbt_t2_freq_list = "570,690,698,714,722,738,770";
    const char *freq_list = "570"; // Bristol live transponder

    // TODO: UI configurable
    gTuneSettings.source = channel_source_ofdm;
    gTuneSettings.mode = NEXUS_FrontendOfdmMode_eDvbt;

    (void)env;
    (void)caller;

    if (gScanInProgress) {
        ALOGD("<> A service scan is already in progress....");
    }

    ALOGD("> servicesScan()");
    gScanInProgress = true;

    // Don't aquire the tuner at this point, as the bchannel_* API's require the ability
    // to aquire a tuner.

    parseChannels(&gTuneSettings, freq_list);

    {
        ALOGI("servicesScan() Start scanning....");
        bool done = false;

        while (!done) {
            done = true;

            for (map = BLST_Q_FIRST(&g_channelMap); map; map = BLST_Q_NEXT(map, link)) {
                int scanAttempts = 0;
                int lastProgCnt = 0;
                bool continueSearch = false;

                if (!g_quiet) ALOGV("servicesScan() Scan settings to be used: %s.", map->name);
                if (map->scanState == Channel_map::SCAN_STATE_DONE) {
                    ALOGV("servicesScan() Scan done for %s, map: %p ...", map->name, (void *)map);
                    continue;
                }
                done = false;
                if (map->scanState == Channel_map::SCAN_STATE_PENDING) {
                    bchannel_scan_start_settings scanSettings;
                    bchannel_scan_get_default_start_settings(&scanSettings);
                    scanSettings.tune = map->tune;
                    if (!map->scan) {
                        ALOGV("servicesScan() Attach frontend and start scan.");
                        map->scan = bchannel_scan_start(&scanSettings);
                        if (!map->scan) {
                            ALOGD("servicesScan() Channel scan failed.");
                            continue;
                        }
                    }
                    else {
                        bchannel_scan_restart(map->scan, &scanSettings);
                    }

                    if (map->scan) {
                        if (!g_quiet) ALOGV("servicesScan() Scanning: %s...", map->name);
                        map->scanState = Channel_map::SCAN_STATE_STARTED;
                        map->scanStart = b_get_time();
                    }
                }

                if (!map->scan) {
                    ALOGV("JNI servicesScan() No scanner found for %s.", map->name);
                    continue;
                }

                // TODO: Can callback be used for scan results, instead of polling the scanner.
                do {
                    BKNI_Sleep(500); // Wait a while before checking for results.
                    continueSearch = false;
#if NEXUS_HAS_FRONTEND
                    if (!map->scanResults.num_programs && map->tune.source != channel_source_streamer) {
                        NEXUS_FrontendHandle frontend;
                        NEXUS_ParserBand parserBand;
                        NEXUS_FrontendFastStatus status;
                        bchannel_scan_get_resources(map->scan, &frontend, &parserBand);
                        rc = NEXUS_Frontend_GetFastStatus(frontend, &status);
                        if (!rc) {
                            const char *lockstr[NEXUS_FrontendLockStatus_eMax] = {"unknown", "unlocked", "locked", "no signal"};
                            if (!g_quiet) ALOGV("servicesScan() frontend lock status: %s", lockstr[status.lockStatus]);
                        }
                    }
#endif
                    rc = bchannel_scan_get_results(map->scan, &map->scanResults);
                    ALOGV("Polling for scan results progs:%d rc:%i",map->scanResults.num_programs, rc);

                    // Obtain all the possible services on this mux
                    if (rc == NEXUS_NOT_AVAILABLE && map->scanResults.num_programs != lastProgCnt) {
                        lastProgCnt = map->scanResults.num_programs;
                        continueSearch = true;
                    }

                    // Allow a maximum scan time of 4s.
                    ++scanAttempts;
                    if (rc == NEXUS_NOT_AVAILABLE && scanAttempts < 8 && lastProgCnt == 0) {
                        continueSearch = true;
                    }
                } while (continueSearch);
                ALOGV("%s scan time %i ms", map->name, b_get_time() - map->scanStart);

                ALOGV("servicesScan() Release frontend and stop scan.");
                bchannel_scan_stop(map->scan);
                map->scanState = Channel_map::SCAN_STATE_DONE;
                map->scan = NULL;

                if (!g_quiet) ALOGV("servicesScan() %d programs found on %s", map->scanResults.num_programs, map->name);
                g_totalChannels += map->scanResults.num_programs;
            }
        }
    }
    gScanInProgress = false;
    ALOGD("< servicesScan() Scan complete, channels found: %i", g_totalChannels);
    return 0;
}
}

extern "C" {
JNIEXPORT jstring JNICALL
Java_com_broadcom_tvinput_BcmTunerJniBridge_getScanResults(JNIEnv *env, jobject caller) {
    ALOGD("<> getScanResults() total channels:%d",g_totalChannels);
    unsigned int channelNumber = 0;
    struct Channel_map *map;
    std::stringstream ss;

    (void)env;
    (void)caller;

    if (g_totalChannels > 0) {
        ss << "[";
        for (map = BLST_Q_FIRST(&g_channelMap); map; map = BLST_Q_NEXT(map, link)) {

            if (map->scanState == Channel_map::SCAN_STATE_DONE) {
                Channel_map *data = map;
                if (data->scanResults.num_programs > 0) {
                    ss << "{";
                    ss << "\"name\":" << "\"" << data->name << "\"";
                    ss << ",\"freq\":" << data->tune.freq;
                    ss << ",\"version\":" << (short)(data->scanResults.version); //unint8
                    ss << ",\"tsid\":" << (unsigned short)data->scanResults.transport_stream_id; //uint16_t

                    ss << ",\"program_list\":[";
                    for (int c = 0; c < data->scanResults.num_programs; c++) {
                        ss << "{";
                        ss << "\"channel_number\":" << channelNumber;
                        ss << ",\"program_number\":" << (unsigned short)(data->scanResults.program_info[c].program_number); //uint16_t
                        ss << ",\"program_pid\":" << (unsigned short)data->scanResults.program_info[c].map_pid; //uint16_t
                        ss << ",\"program_version\":" << (short)(data->scanResults.program_info[c].version); //uint8_t
                        ss << ",\"pcr_pid\":" << (unsigned short)data->scanResults.program_info[c].pcr_pid; //uint16_t
                        ss << ",\"ca_pid\":" << (unsigned short)(data->scanResults.program_info[c].ca_pid); //uint16_t

                        ss << ",\"video_pids\":[";
                        for (int vid = 0; vid < data->scanResults.program_info[c].num_video_pids; vid++) {
                            ss << "{";
                            ss << "\"pid\":" << (unsigned short)(data->scanResults.program_info[c].video_pids[vid].pid); //uint16_t
                            ss << ",\"codec\":\"" << lookup_name(g_videoCodecStrs, data->scanResults.program_info[c].video_pids[vid].codec) << "\""; //NEXUS_VideoCodec
                            ss << ",\"ca_pid\":" << (unsigned short)(data->scanResults.program_info[c].video_pids[vid].ca_pid); //uint16_t
                            ss << "}";
                            if (vid + 1 < data->scanResults.program_info[c].num_video_pids) ss << ",";
                        }
                        ss << "]";

                        ss << ",\"audio_pids\":[";
                        for (int aid = 0; aid < data->scanResults.program_info[c].num_audio_pids; aid++) {
                            ss << "{";
                            ss << "\"pid\":" << (unsigned short)(data->scanResults.program_info[c].audio_pids[aid].pid); //uint16_t
                            ss << ",\"codec\":\"" << lookup_name(g_audioCodecStrs, data->scanResults.program_info[c].audio_pids[aid].codec) << "\""; //NEXUS_AudioCodec
                            ss << ",\"ca_pid\":" << (unsigned short)(data->scanResults.program_info[c].audio_pids[aid].ca_pid); //uint16_t
                            ss << "}";
                            if (aid + 1 < data->scanResults.program_info[c].num_audio_pids) ss << ",";
                        }
                        ss << "]";

                        ss << ",\"other_pids\":[";
                        for (int oid = 0; oid < data->scanResults.program_info[c].num_other_pids; oid++) {
                            ss << "{";
                            ss << "\"pid\":" << (unsigned short)(data->scanResults.program_info[c].other_pids[oid].pid); //uint16_t
                            ss << ",\"stream_type\":" << (unsigned int)(data->scanResults.program_info[c].other_pids[oid].stream_type); //unsigned
                            ss << ",\"ca_pid\":" << (unsigned short)(data->scanResults.program_info[c].other_pids[oid].ca_pid); //uint16_t
                            ss << "}";
                            if (oid + 1 < data->scanResults.program_info[c].num_other_pids) ss << ",";
                        }
                        ss << "]";
                        ss << "}";
                        if (c + 1 < data->scanResults.num_programs) ss << ",";

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

    return  env->NewStringUTF(ss.str().c_str());
}
}

extern "C" {
JNIEXPORT jint JNICALL
Java_com_broadcom_tvinput_BcmTunerJniBridge_servicesAvailable(JNIEnv *env, jobject caller) {
    struct Channel_map *map;

    (void)env;
    (void)caller;

    g_totalChannels = 0;
    for (map = BLST_Q_FIRST(&g_channelMap); map; map = BLST_Q_NEXT(map, link)) {
        g_totalChannels += map->scanResults.num_programs;
    }

    ALOGD("<> servicesAvailable(): %d",g_totalChannels);
    return g_totalChannels>0?1:0;
}
}

extern "C" {
JNIEXPORT jint JNICALL
Java_com_broadcom_tvinput_BcmTunerJniBridge_getMsgFromJni(JNIEnv *env, jobject caller) {

    (void)caller;

    // TODO:
    ALOGD("<> getMsgFromJni() RECEIVED call from JAVA");

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
    int rc;
    ALOGD("> tune() tsid=%i, pNum=%i", (int)tsid, (int)pNum);

    (void)env;
    (void)caller;

    // TODO: UI configurable
    gTuneSettings.source = channel_source_ofdm;
    gTuneSettings.mode = NEXUS_FrontendOfdmMode_eDvbt;

    rc = tuneToChannel((int)tsid, (int)pNum);

    ALOGD("< tune()");
    return rc;
}
}

extern "C" {
JNIEXPORT jint JNICALL
Java_com_broadcom_tvinput_BcmTunerJniBridge_releaseSdb(JNIEnv *env, jobject caller) {
   ALOGD("> releaseSdb()");

   (void)env;
   (void)caller;

   if (native_window)
      native_window_set_sideband_stream(native_window, NULL);
   native_handle_delete(native_handle);
   gSdbGeomCb = NULL;

   ALOGD("< releaseSdb()");
   return 0;
}
}

extern "C" {
JNIEXPORT jint JNICALL
Java_com_broadcom_tvinput_BcmTunerJniBridge_initialiseSdb(JNIEnv *env, jobject caller, jobject jsurface) {
   NEXUS_Error rc;
   uint32_t context;

   ALOGD("> initialiseSdb()");

   (void)caller;

   native_window = ANativeWindow_fromSurface(env,  jsurface);
   if (idse() != NULL) {
      gSdbGeomCb = new SdbGeomCb(&sbGeometryUpdate, NULL);
      context = idse()->regSdbCb(0, NULL);
      context = idse()->regSdbCb(0, gSdbGeomCb);
   }
   native_handle = native_handle_create(0, 2);
   if (!native_handle) {
      gSdbGeomCb = NULL;
      ALOGE("failed to allocate native handle");
      return JNI_FALSE;
   }
   native_handle->data[0] = 1;
   native_handle->data[1] = context;
   native_window_set_sideband_stream(native_window, native_handle);

   if (gSurfaceClient == NULL) {
      NxClient_AllocSettings allocSettings;
      NxClient_AllocResults allocResults;
      NxClient_GetDefaultAllocSettings(&allocSettings);
      allocSettings.surfaceClient = 1; /* surface client required for video window */
      rc = NxClient_Alloc(&allocSettings, &allocResults);

      gSurfaceClient = NEXUS_SurfaceClient_Acquire(allocResults.surfaceClient[0].id);
      if (gSurfaceClient) {
          /* creating the video window is necessary, so that SurfaceCompositor can resize the video window */
          gVideo_sc = NEXUS_SurfaceClient_AcquireVideoWindow(gSurfaceClient, 0);
      }
      gSurfaceId = allocResults.surfaceClient[0].id;
      ALOGD("%s() aquired surfaceClientId: %d", __FUNCTION__, gSurfaceId);
   }

   ALOGD("< initialiseSdb()");
   return 0;
}
}



extern "C" {
JNIEXPORT jint JNICALL
Java_com_broadcom_tvinput_BcmTunerJniBridge_closeSession(JNIEnv *env, jobject caller) {
    struct Frontend *frontend;
    struct Decoder *d;
    ALOGD("> closeSession()");

    (void)env;
    (void)caller;

    while ((d = BLST_Q_FIRST(&g_decoders))) {
        stopDecode(d);
        if (d->channel) {
            live_decode_destroy_channel(d->channel);
            d->channel = NULL;
        }
        ALOGD("JNI closeSession: calling NEXUS_ParserBand_Close()");
        NEXUS_ParserBand_Close(d->parserBand);
        BLST_Q_REMOVE(&g_decoders, d, link);
        BKNI_Free(d);
    }

    if (gDecode) {
        live_decode_destroy(gDecode);
        gDecode = NULL;
    }

    while ((frontend = BLST_Q_FIRST(&g_frontends))) {
        if (frontend->frontend) {
            ALOGD("JNI closeSession: calling NEXUS_Frontend_Release()");
            NEXUS_Frontend_Release(frontend->frontend);
            frontend->frontend = NULL;
        }
        BLST_Q_REMOVE(&g_frontends, frontend, link);
        BKNI_Free(frontend);
    }

    if (gSurfaceClient) {
        NEXUS_SurfaceClient_Release(gSurfaceClient);
        NEXUS_SurfaceClient_ReleaseVideoWindow(gVideo_sc);
        gSurfaceId = 0;
        gSurfaceClient = gVideo_sc = NULL;
    }

    ALOGD("< closeSession()");
    return 0;
}
}

extern "C" {
JNIEXPORT jint JNICALL
Java_com_broadcom_tvinput_BcmTunerJniBridge_stop(JNIEnv *env, jobject caller) {
    struct Decoder *d;
    ALOGD("> stop()");

    (void)env;
    (void)caller;

    for (d = BLST_Q_FIRST(&g_decoders); d; d = BLST_Q_NEXT(d, link)) {
        stopDecode(d);
    }

    ALOGD("< stop()");
    return 0;
}
}

extern "C" {
JNIEXPORT jint JNICALL
Java_com_broadcom_tvinput_BcmTunerJniBridge_uninitializeTuner(JNIEnv *env, jobject caller) {

    (void)env;
    (void)caller;

    ALOGD("> uninitializeTuner()");

    uninit_channels();

#if B_REFSW_TR69C_SUPPORT
        b_tr69c_uninit(tr69c);
#endif

    ALOGD("< uninitializeTuner()");
    return 0;
}
}
