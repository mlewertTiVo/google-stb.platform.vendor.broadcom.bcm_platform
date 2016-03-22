/******************************************************************************
 *    (c)2010-2016 Broadcom Corporation
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
#include "bcmsidebandfileplayer.h"

//#define LOG_NDEBUG 0
#define LOG_TAG "BcmSidebandFilePlayer"
#include <cutils/log.h>
#define ARRAY_SIZE(x) ((int) (sizeof(x) / sizeof((x)[0])))

/* Please keep in sync with nxclient/apps/play.c */

#if NEXUS_HAS_INPUT_ROUTER && NEXUS_HAS_PLAYBACK && NEXUS_HAS_SIMPLE_DECODER
#include "nexus_platform_client.h"
#include "media_player.h"
#include "nexus_surface_client.h"
#include "nexus_core_utils.h"
#include "namevalue.h"
#include "nxclient.h"
#include "nexus_graphics2d.h"
#include "binput.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <pthread.h>
#include "bstd.h"
#include "bkni.h"
#include "bfont.h"
#include "nxapps_cmdline.h"
#include "bgui.h"

//BDBG_MODULE(play);

static void print_usage(const struct nxapps_cmdline *cmdline)
{
    printf(
    "Usage: play OPTIONS stream_url [indexfile]\n"
    "\n"
    "  stream_url can be http://server/path, or [file://]path/file\n"
    "\n"
    "OPTIONS:\n"
    "  --help or -h for help\n"
    "  -timeout SECONDS\n"
    "  -gui {off|no_alpha_hole}\n"
    );
    printf(
    "  -pip                     sets -rect and -zorder for picture-in-picture\n"
    "  -once                    play once and exit; otherwise loop around\n"
    "  -max WIDTH,HEIGHT        max video decoder resolution\n"
    "  -dtcp_ip                 enable DTCP/IP\n"
    );
    printf(
    "  -pig                     example of picture-in-graphics video window sizing\n"
    "  -smooth                  set smoothResolutionChange to disable scale factor rounding\n"
    "  -manual_4x3_box          example of client-side aspect ratio control\n"
    );
    printf(
    "  -hdcp {m[andatory]|o[ptional]}\n"
    "  -3d {lr|ou|auto}         stereoscopic (3D) source format\n"
    "  -audio_primers           use primers for fast resumption of pcm and compressed audio\n"
    "  -initial_audio_primers   if audio decoder already in use, use primers\n"
    "  -timeshift               file is being recorded; playback will wait at end of file for more data to arrive.\n"
    );
    printf(
    "  -stcTrick off            use decoder trick modes\n"
    "  -startPaused\n"
    "  -dqtFrameReverse #       number of pictures per GOP for DQT frame reverse\n"
    );
    print_list_option("format",g_videoFormatStrs);
    print_list_option("ar",g_contentModeStrs);
    print_list_option("sync",g_syncModeStrs);
    print_list_option("crypto",g_securityAlgoStrs);
    print_list_option("dolby_drc_mode",g_dolbyDrcModeStrs);
    nxapps_cmdline_print_usage(cmdline);
    printf(
    "  -scrtext SCRIPT          run --help-script to learn script commands\n"
    "  -script SCRIPTFILE       run --help-script to learn script commands\n"
    "  -video PID               override media probe. use 0 for no video.\n"
    "  -video_type CODEC        override media probe.\n"
    "  -video_cdb MBytes        size of compressed video buffer, in MBytes, decimal allowed\n"
    "  -audio PID               override media probe. use 0 for no audio.\n"
    "  -audio_type CODEC        override media probe.\n"
    );
    print_list_option("mpeg_type",g_transportTypeStrs);
    printf(
    "  -secure                  use SVP secure picture buffers\n"
    );
}

struct client_state
{
    BKNI_EventHandle endOfStreamEvent;
    media_player_t player;
    binput_t input;
    int rate; /* trick speed */
    bool stopped;
    media_player_start_settings start_settings;
    NEXUS_SurfaceClientHandle surfaceClient;

    /* gui */
    NEXUS_Graphics2DHandle gfx;
    NEXUS_SurfaceHandle surface;
    BKNI_EventHandle displayedEvent, checkpointEvent, windowMovedEvent;
    NEXUS_Rect last_cursor;
    bfont_t font;
    unsigned font_height;
    NEXUS_Rect timeline_rect;
    struct bfont_surface_desc desc;
};

BcmSidebandFilePlayer::BcmSidebandFilePlayer(const char*path)
{
    ALOGV("%s", __FUNCTION__);
    mPath = strdup(path);
    client = new struct client_state;
}

BcmSidebandFilePlayer::~BcmSidebandFilePlayer()
{
    ALOGV("%s", __FUNCTION__);
    free(mPath);
    delete client;
}

static void complete(void *context)
{
    BKNI_SetEvent((BKNI_EventHandle)context);
}

static void complete2(void *context, int param)
{
    BSTD_UNUSED(param);
    BKNI_SetEvent((BKNI_EventHandle)context);
}

static int gui_init(struct client_state *client, bool gui);
static int gui_set_pos(struct client_state *client, unsigned position, unsigned first, unsigned last);
static void gui_uninit(struct client_state *client);

#include <sys/time.h>
static unsigned b_get_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000 + tv.tv_usec/1000;
}

static void process_input(struct client_state *client, b_remote_key key, bool repeat)
{
    /* only allow repeats for frame advance/reverse */
    switch (key) {
    case b_remote_key_rewind:
    case b_remote_key_fast_forward:
        if (client->rate == 0) {
            break;
        }
        /* fall through */
    default:
        if (repeat) return;
    }

    switch (key) {
    case b_remote_key_play:
        client->rate = NEXUS_NORMAL_DECODE_RATE;
        ALOGW("play");
        media_player_trick(client->player, client->rate);
        break;
    case b_remote_key_pause:
        if (client->rate) {
            client->rate = 0;
            ALOGW("pause");
        }
        else {
            client->rate = NEXUS_NORMAL_DECODE_RATE;
            ALOGW("play");
        }
        media_player_trick(client->player, client->rate);
        break;
    case b_remote_key_fast_forward:
        if (client->rate == 0) {
            media_player_frame_advance(client->player, true);
        }
        else {
            client->rate += NEXUS_NORMAL_DECODE_RATE;
            if (client->rate == 0) {
                client->rate = NEXUS_NORMAL_DECODE_RATE;
            }
            if (client->rate == NEXUS_NORMAL_DECODE_RATE) {
                ALOGW("play");
            }
            else {
                ALOGW("%dx trick", client->rate/NEXUS_NORMAL_DECODE_RATE);
            }
            media_player_trick(client->player, client->rate);
        }
        break;
    case b_remote_key_rewind:
        if (client->rate == 0) {
            media_player_frame_advance(client->player, false);
        }
        else {
            client->rate -= NEXUS_NORMAL_DECODE_RATE;
            if (client->rate == 0) {
                client->rate = -1*NEXUS_NORMAL_DECODE_RATE;
            }
            if (client->rate == NEXUS_NORMAL_DECODE_RATE) {
                ALOGW("play");
            }
            else {
                ALOGW("%dx trick", client->rate/NEXUS_NORMAL_DECODE_RATE);
            }
            media_player_trick(client->player, client->rate);
        }
        break;
    case b_remote_key_right:
        media_player_seek(client->player, 30 * 1000, SEEK_CUR);
        if (client->rate == 0) {
            media_player_frame_advance(client->player, true);
        }
        break;
    case b_remote_key_left:
        media_player_seek(client->player, -30 * 1000, SEEK_CUR);
        if (client->rate == 0) {
            media_player_frame_advance(client->player, true);
        }
        break;
    case b_remote_key_stop:
    case b_remote_key_back:
    case b_remote_key_clear:
        client->stopped = true;
        /* can't call media_player_stop here because we're in a callback */
        break;
    default:
        ALOGI("unknown key %#x", key);
        break;
    }
}

static void *standby_monitor(void *context)
{
    struct client_state *client = (struct client_state *)context;
    NEXUS_Error rc;
    NxClient_StandbyStatus standbyStatus, prevStatus;

    BSTD_UNUSED(context);

    rc = NxClient_GetStandbyStatus(&prevStatus);
    ALOG_ASSERT(!rc);

    while(!client->stopped) {
        rc = NxClient_GetStandbyStatus(&standbyStatus);
        ALOG_ASSERT(!rc);

        if (rc == NEXUS_SUCCESS) {
            if (standbyStatus.transition == NxClient_StandbyTransition_eAckNeeded) {
                printf("'play' acknowledges standby state: %s\n", lookup_name(g_platformStandbyModeStrs, standbyStatus.settings.mode));
                media_player_stop(client->player);
                NxClient_AcknowledgeStandby(true);
            } else if (standbyStatus.settings.mode == NEXUS_PlatformStandbyMode_eOn && prevStatus.settings.mode != NEXUS_PlatformStandbyMode_eOn) {
                media_player_start(client->player, &client->start_settings);
            }
            prevStatus = standbyStatus;
        }
        BKNI_Sleep(100);
    }
    return NULL;
}

int BcmSidebandFilePlayer::start(int x, int y, int w, int h)
{
    ALOGV("%s", __FUNCTION__);
    char argv0[] = LOG_TAG;
    char argv1[] = "-rect";
    char argv2[100];
    sprintf(argv2, "%d,%d,%d,%d", x, y, w, h);
    char argv3[] = "-gui";
    char argv4[] = "off";
    char argv5[256];
    sprintf(argv5, "%s", mPath);
    const char *argv[] = {&argv0[0], &argv1[0], &argv2[0], &argv3[0], &argv4[0], &argv5[0], NULL};
    int argc = ARRAY_SIZE(argv) - 1;
    NEXUS_Error rc = 0;
    unsigned timeout = 0;
    int curarg = 1;
    //struct client_state context, *client = &context;
    media_player_create_settings create_settings;
    media_player_start_settings start_settings;
    NxClient_JoinSettings joinSettings;
    NxClient_AllocSettings allocSettings;
    NxClient_AllocResults allocResults;
    gui = gui_off;
    unsigned starttime;
    //pthread_t standby_thread_id;
    NEXUS_VideoWindowContentMode contentMode = NEXUS_VideoWindowContentMode_eMax;
    struct b_pig_inc pig_inc;
    bool manual_4x3_box = false;
    NEXUS_SurfaceClientHandle video_sc = NULL;
    NEXUS_SimpleVideoDecoderHandle videoDecoder;
#if B_REFSW_TR69C_SUPPORT
    b_tr69c_t tr69c;
#endif
    struct nxapps_cmdline cmdline;
    int n;
    struct binput_settings input_settings;

    memset(&pig_inc, 0, sizeof(pig_inc));
    memset(client, 0, sizeof(*client));
    media_player_get_default_create_settings(&create_settings);
    media_player_get_default_start_settings(&start_settings);
    binput_get_default_settings(&input_settings);
    nxapps_cmdline_init(&cmdline);
    nxapps_cmdline_allow(&cmdline, nxapps_cmdline_type_SimpleVideoDecoderPictureQualitySettings);
    nxapps_cmdline_allow(&cmdline, nxapps_cmdline_type_SurfaceComposition);

    while (argc > curarg) {
        if (!strcmp(argv[curarg], "-h") || !strcmp(argv[curarg], "--help")) {
            print_usage(&cmdline);
            return 0;
        }
        else if (!strcmp(argv[curarg], "-timeout") && argc>curarg+1) {
            timeout = strtoul(argv[++curarg], NULL, 0);
        }
        else if (!strcmp(argv[curarg], "-no_decoded_video")) { /* deprecated. use -video 0 instead. */
            start_settings.video.pid = 0;
        }
        else if (!strcmp(argv[curarg], "-no_decoded_audio")) { /* deprecated. use -audio 0 instead. */
            start_settings.audio.pid = 0;
        }
        else if (!strcmp(argv[curarg], "-gui") && argc>curarg+1) {
            ++curarg;
            if (!strcmp(argv[curarg], "off")) {
                gui = gui_off;
            }
            else if (!strcmp(argv[curarg], "no_alpha_hole")) {
                gui = gui_no_alpha_hole;
            }
            else {
                print_usage(&cmdline);
                return -1;
            }
        }
        else if (!strcmp(argv[curarg], "-once")) {
            start_settings.loopMode = NEXUS_PlaybackLoopMode_ePause;
        }
        else if (!strcmp(argv[curarg], "-pip")) {
            const char *argv[] = {"-rect","960,0,960,540","-zorder","1"};
            if (!cmdline.comp.rect.set) {
                nxapps_cmdline_parse(0, 2, argv, &cmdline);
            }
            if (!cmdline.comp.zorder.set) {
                nxapps_cmdline_parse(2, 4, argv, &cmdline);
            }
            start_settings.videoWindowType = NxClient_VideoWindowType_ePip;
        }
        else if (!strcmp(argv[curarg], "-pig")) {
            pig_inc.y = pig_inc.x = 4;
            pig_inc.width = 2;
        }
        else if (!strcmp(argv[curarg], "-smooth")) {
            start_settings.smoothResolutionChange = true;
        }
        else if (!strcmp(argv[curarg], "-manual_4x3_box")) {
            manual_4x3_box = true;
        }
        else if (!strcmp(argv[curarg], "-format") && argc>curarg+1) {
            create_settings.maxFormat = (NEXUS_VideoFormat)lookup(g_videoFormatStrs, argv[++curarg]);
        }
        else if (!strcmp(argv[curarg], "-max") && curarg+1 < argc) {
            int n = sscanf(argv[++curarg], "%u,%u", &create_settings.maxWidth, &create_settings.maxHeight);
            if (n != 2) {
                print_usage(&cmdline);
                return -1;
            }
        }
        else if (!strcmp(argv[curarg], "-dtcp_ip") && curarg+1 < argc) {
#if B_HAS_DTCP_IP
            create_settings.dtcpEnabled = true;
#else
            ALOGE("DTCP support is not enabled. Please rebuild with DTCP_IP_SUPPORT=y.");
            print_usage(&cmdline);
            return -1;
#endif
        }
        else if (!strcmp(argv[curarg], "-ar") && curarg+1 < argc) {
            contentMode = (NEXUS_VideoWindowContentMode)lookup(g_contentModeStrs, argv[++curarg]);
        }
        else if (!strcmp(argv[curarg], "-sync") && argc>curarg+1) {
            start_settings.sync = (NEXUS_SimpleStcChannelSyncMode)lookup(g_syncModeStrs, argv[++curarg]);
        }
        else if (!strcmp(argv[curarg], "-crypto") && curarg+1 < argc) {
            start_settings.decrypt.algo = (NEXUS_SecurityAlgorithm)lookup(g_securityAlgoStrs, argv[++curarg]);
        }
        else if (!strcmp(argv[curarg], "-dolby_drc_mode") && curarg+1 < argc) {
            start_settings.audio.dolbyDrcMode = (NEXUS_AudioDecoderDolbyDrcMode)lookup(g_dolbyDrcModeStrs, argv[++curarg]);
        }
        else if (!strcmp(argv[curarg], "-hdcp") && curarg+1 < argc) {
            curarg++;
            if (argv[curarg][0] == 'm') {
                start_settings.hdcp = NxClient_HdcpLevel_eMandatory;
            }
            else if (argv[curarg][0] == 'o') {
                start_settings.hdcp = NxClient_HdcpLevel_eOptional;
            }
            else {
                print_usage(&cmdline);
                return -1;
            }
        }
        else if (!strcmp(argv[curarg], "-3d") && curarg+1<argc) {
            curarg++;
            if (!strcmp(argv[curarg], "none")) {
                start_settings.source3d = media_player_start_settings::source3d_none;
            }
            else if (!strcmp(argv[curarg], "lr")) {
                start_settings.source3d = media_player_start_settings::source3d_lr;
            }
            else if (!strcmp(argv[curarg], "ou")) {
                start_settings.source3d = media_player_start_settings::source3d_ou;
            }
            else if (!strcmp(argv[curarg], "auto")) {
                start_settings.source3d = media_player_start_settings::source3d_auto;
            }
            else {
                print_usage(&cmdline);
                return -1;
            }
        }
        else if (!strcmp(argv[curarg], "-audio_primers")) {
            start_settings.audio_primers = media_player_audio_primers_later;
        }
        else if (!strcmp(argv[curarg], "-initial_audio_primers")) {
            start_settings.audio_primers = media_player_audio_primers_immediate;
        }
        else if (!strcmp(argv[curarg], "-timeshift")) {
            start_settings.loopMode = NEXUS_PlaybackLoopMode_ePlay;
        }
        else if (!strcmp(argv[curarg], "-stcTrick") && curarg+1 < argc) {
            start_settings.stcTrick = strcmp(argv[++curarg], "off");
        }
        else if (!strcmp(argv[curarg], "-startPaused")) {
            start_settings.startPaused = true;
        }
        else if (!strcmp(argv[curarg], "-script") && argc>curarg+1) {
            input_settings.script_file = argv[++curarg];
        }
        else if (!strcmp(argv[curarg], "-scrtext") && argc>curarg+1) {
            input_settings.script = argv[++curarg];
        }
        else if (!strcmp(argv[curarg], "--help-script")) {
            binput_print_script_usage();
            return 0;
        }
        else if (!strcmp(argv[curarg], "-dqtFrameReverse") && argc>curarg+1) {
            start_settings.dqtFrameReverse = atoi(argv[++curarg]);
        }
        else if (!strcmp(argv[curarg], "-video") && argc>curarg+1) {
            start_settings.video.pid = strtoul(argv[++curarg], NULL, 0);
        }
        else if (!strcmp(argv[curarg], "-video_type") && argc>curarg+1) {
            start_settings.video.codec = (NEXUS_VideoCodec)lookup(g_videoCodecStrs, argv[++curarg]);
        }
        else if (!strcmp(argv[curarg], "-video_cdb") && argc>curarg+1) {
            float size;
            sscanf(argv[++curarg], "%f", &size);
            start_settings.video.fifoSize = size * 1024 * 1024;
        }
        else if (!strcmp(argv[curarg], "-audio") && argc>curarg+1) {
            start_settings.audio.pid = strtoul(argv[++curarg], NULL, 0);
        }
        else if (!strcmp(argv[curarg], "-audio_type") && argc>curarg+1) {
            start_settings.audio.codec = (NEXUS_AudioCodec)lookup(g_audioCodecStrs, argv[++curarg]);
        }
        else if (!strcmp(argv[curarg], "-mpeg_type") && argc>curarg+1) {
            start_settings.transportType = (NEXUS_TransportType)lookup(g_transportTypeStrs, argv[++curarg]);
        }
        else if (!strcmp(argv[curarg], "-secure")) {
            start_settings.video.secure = true;
        }
        else if ((n = nxapps_cmdline_parse(curarg, argc, argv, &cmdline))) {
            if (n < 0) {
                print_usage(&cmdline);
                return -1;
            }
            curarg += n;
        }
        else if (!start_settings.stream_url) {
            start_settings.stream_url = argv[curarg];
        }
        else if (!start_settings.index_url) {
            start_settings.index_url = argv[curarg];
        }
        else {
            print_usage(&cmdline);
            return -1;
        }
        curarg++;
    }
    if (!start_settings.stream_url) {
        print_usage(&cmdline);
        return -1;
    }

    NxClient_GetDefaultJoinSettings(&joinSettings);
    snprintf(joinSettings.name, NXCLIENT_MAX_NAME, "%s %s", argv[0], start_settings.stream_url);
    rc = NxClient_Join(&joinSettings);
    if (rc) return -1;

    BKNI_CreateEvent(&client->endOfStreamEvent);
    BKNI_CreateEvent(&client->displayedEvent);
    BKNI_CreateEvent(&client->windowMovedEvent);

    NxClient_GetDefaultAllocSettings(&allocSettings);
    allocSettings.surfaceClient = 1; /* surface client required for video window */
    rc = NxClient_Alloc(&allocSettings, &allocResults);
    if (rc) BERR_TRACE(rc);

    client->input = binput_open(&input_settings);
    printf("Use remote control for trick modes. \"Stop\" or \"Clear\" exits the app.\n");

    client->surfaceClient = NEXUS_SurfaceClient_Acquire(allocResults.surfaceClient[0].id);
    if (client->surfaceClient) {
        NEXUS_SurfaceClientSettings settings;

        if (nxapps_cmdline_is_set(&cmdline, nxapps_cmdline_type_SurfaceComposition)) {
            NEXUS_SurfaceComposition comp;
            NxClient_GetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
            nxapps_cmdline_apply_SurfaceComposition(&cmdline, &comp);
            NxClient_SetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
        }

        NEXUS_SurfaceClient_GetSettings(client->surfaceClient, &settings);
        settings.displayed.callback = complete2;
        settings.displayed.context = client->displayedEvent;
        settings.windowMoved.callback = complete2;
        settings.windowMoved.context = client->windowMovedEvent;
        NEXUS_SurfaceClient_SetSettings(client->surfaceClient, &settings);

        /* creating the video window is necessasy so that SurfaceCompositor can resize the video window */
        video_sc = NEXUS_SurfaceClient_AcquireVideoWindow(client->surfaceClient, 0);
        if (contentMode != NEXUS_VideoWindowContentMode_eMax || manual_4x3_box) {
            NEXUS_SurfaceClient_GetSettings(video_sc, &settings);
            if (manual_4x3_box) {
                settings.composition.contentMode = NEXUS_VideoWindowContentMode_eFull;
                settings.composition.virtualDisplay.width = 1280;
                settings.composition.virtualDisplay.height = 720;
                settings.composition.position.width = settings.composition.virtualDisplay.height*4/3;
                settings.composition.position.height = settings.composition.virtualDisplay.height;
                settings.composition.position.x = (settings.composition.virtualDisplay.width-settings.composition.position.width)/2;
                settings.composition.position.y = 0;
            }
            else {
                if (contentMode != NEXUS_VideoWindowContentMode_eMax) {
                    settings.composition.contentMode = contentMode;
                }
            }
            NEXUS_SurfaceClient_SetSettings(video_sc, &settings);
        }
        if (pig_inc.x) {
            b_pig_init(video_sc);
        }
    }

    create_settings.window.surfaceClientId = allocResults.surfaceClient[0].id;
    create_settings.window.id = 0;
    client->player = media_player_create(&create_settings);
    if (!client->player) return -1;

#if B_REFSW_TR69C_SUPPORT
    tr69c = b_tr69c_init(media_player_get_set_tr69c_info, client->player);
#endif

    videoDecoder = media_player_get_video_decoder(client->player);
    if (videoDecoder) {
        if (nxapps_cmdline_is_set(&cmdline, nxapps_cmdline_type_SimpleVideoDecoderPictureQualitySettings)) {
            NEXUS_SimpleVideoDecoderPictureQualitySettings settings;
            NEXUS_SimpleVideoDecoder_GetPictureQualitySettings(videoDecoder, &settings);
            nxapps_cmdline_apply_SimpleVideoDecodePictureQualitySettings(&cmdline, &settings);
            NEXUS_SimpleVideoDecoder_SetPictureQualitySettings(videoDecoder, &settings);
        }
    }

    client->rate = NEXUS_NORMAL_DECODE_RATE;

    start_settings.eof = complete;
    start_settings.context = client->endOfStreamEvent;
    rc = media_player_start(client->player, &start_settings);
    if (rc) {
        ALOGE("unable to play '%s'", start_settings.stream_url);
        goto err_start;
    }
    client->start_settings = start_settings;

    mSurfaceClientId = allocResults.surfaceClient[0].id;

    /* if there's no timeline gui and video was not started, release the SurfaceClient */
    if (gui != gui_on) {
        NEXUS_VideoDecoderStatus status;
        NEXUS_SimpleVideoDecoderHandle videoDecoder = media_player_get_video_decoder(client->player);
        if (videoDecoder) {
            NEXUS_SimpleVideoDecoder_GetStatus(videoDecoder, &status);
        }
        else {
            status.started = false;
        }
        if (!status.started) {
            NEXUS_SurfaceClient_Release(client->surfaceClient);
            client->surfaceClient = NULL;
        }
    }

    rc = pthread_create(&standby_thread_id, NULL, standby_monitor, client);
    ALOG_ASSERT(!rc);

    if (gui) {
        gui_init(client, gui == gui_on);
    }

    starttime = b_get_time();
    mState = STATE_PLAYING;
    return rc;

err_start:
    reset();
    return rc;
}

void BcmSidebandFilePlayer::stop()
{
    ALOGV("%s", __FUNCTION__);
    client->stopped = true;

    if (gui) {
        gui_uninit(client);
    }
    pthread_join(standby_thread_id, NULL);

#if B_REFSW_TR69C_SUPPORT
    b_tr69c_uninit(tr69c);
#endif

    media_player_stop(client->player);
    reset();
}

void BcmSidebandFilePlayer::reset()
{
    ALOGV("%s", __FUNCTION__);
    binput_close(client->input);
    BKNI_DestroyEvent(client->endOfStreamEvent);
    BKNI_DestroyEvent(client->displayedEvent);
    BKNI_DestroyEvent(client->windowMovedEvent);
    media_player_destroy(client->player);
    NxClient_Uninit();
    mState = STATE_IDLE;
}

/* simple DVR GUI */

#define BORDER 4
#define BORDER_COLOR 0xFF8888AA
#define TIMELINE_COLOR 0xFF3333DD
#define CURSOR_COLOR 0xFFBBBBBB
#define CURSOR_WIDTH 4
#define CURSOR_HEIGHT 20

static void gui_draw_bar(struct client_state *client);

static void checkpoint(struct client_state *client)
{
    int rc = NEXUS_Graphics2D_Checkpoint(client->gfx, NULL); /* require to execute queue */
    if (rc == NEXUS_GRAPHICS2D_QUEUED) {
        rc = BKNI_WaitForEvent(client->checkpointEvent, BKNI_INFINITE);
    }
    ALOG_ASSERT(!rc);
}

static int gui_init(struct client_state *client, bool gui)
{
    NEXUS_Graphics2DSettings gfxSettings;
    NEXUS_SurfaceCreateSettings createSettings;
    NEXUS_Graphics2DFillSettings fillSettings;
    int rc;
    const char *fontname = "nxclient/arial_18_aa.bwin_font";
    NEXUS_Rect rect = {40,430,650,CURSOR_HEIGHT};

    if (!client->surfaceClient) return -1;

    BKNI_CreateEvent(&client->checkpointEvent);

    client->gfx = NEXUS_Graphics2D_Open(NEXUS_ANY_ID, NULL);
    NEXUS_Graphics2D_GetSettings(client->gfx, &gfxSettings);
    gfxSettings.checkpointCallback.callback = complete2;
    gfxSettings.checkpointCallback.context = client->checkpointEvent;
    rc = NEXUS_Graphics2D_SetSettings(client->gfx, &gfxSettings);
    ALOG_ASSERT(!rc);

    NEXUS_Surface_GetDefaultCreateSettings(&createSettings);
    createSettings.pixelFormat = NEXUS_PixelFormat_eA8_R8_G8_B8;
    createSettings.width = 720;
    createSettings.height = 480;
    client->surface = NEXUS_Surface_Create(&createSettings);

    NEXUS_Graphics2D_GetDefaultFillSettings(&fillSettings);
    fillSettings.surface = client->surface;
    fillSettings.color = 0;
    rc = NEXUS_Graphics2D_Fill(client->gfx, &fillSettings);
    ALOG_ASSERT(!rc);

    client->timeline_rect = rect;

    /* even an app with no gui needs graphics for an alpha hole if there are other clients.
    filling with 0 is enough. */
    if (gui) {
        client->font = bfont_open(fontname);
        if (!client->font) {
            ALOGW("unable to load font %s", fontname);
        }
        else {
            bfont_get_height(client->font, &client->font_height);
            bfont_get_surface_desc(client->surface, &client->desc);
            client->timeline_rect.y -= client->font_height;
            client->timeline_rect.height += client->font_height;
        }
        gui_draw_bar(client);
    }
    else {
        checkpoint(client);
    }

    rc = NEXUS_SurfaceClient_SetSurface(client->surfaceClient, client->surface);
    ALOG_ASSERT(!rc);

    rc = BKNI_WaitForEvent(client->displayedEvent, 5000);
    ALOG_ASSERT(!rc);

    return 0;
}

static void gui_draw_bar(struct client_state *client)
{
    NEXUS_Graphics2DFillSettings fillSettings;
    int rc;

    NEXUS_Graphics2D_GetDefaultFillSettings(&fillSettings);
    fillSettings.surface = client->surface;
    fillSettings.color = BORDER_COLOR;
    fillSettings.rect = client->timeline_rect;
    fillSettings.rect.x -= BORDER;
    fillSettings.rect.y -= BORDER;
    fillSettings.rect.width += BORDER*2;
    fillSettings.rect.height += BORDER*2;
    rc = NEXUS_Graphics2D_Fill(client->gfx, &fillSettings);
    ALOG_ASSERT(!rc);

    fillSettings.color = TIMELINE_COLOR;
    fillSettings.rect = client->timeline_rect;
    fillSettings.rect.height = CURSOR_HEIGHT;
    rc = NEXUS_Graphics2D_Fill(client->gfx, &fillSettings);
    ALOG_ASSERT(!rc);

    checkpoint(client);

    if (client->font) {
        NEXUS_Rect textrect = client->timeline_rect;
        int width, height, base;
        char duration[32];
        NEXUS_PlaybackStatus status;
        int text_limit_width = (textrect.width * 80)/100;

        textrect.y = textrect.y + client->timeline_rect.height - client->font_height;
        textrect.height = client->font_height;
        bfont_measure_text(client->font, client->start_settings.stream_url, -1, &width, &height, &base);
        if(width && width < text_limit_width) {
            bfont_draw_aligned_text(&client->desc, client->font, &textrect, client->start_settings.stream_url, -1, 0xFFCCCCCC, bfont_valign_center, bfont_halign_left);
        } else {
            char truncated[128];
            size_t len = strlen(client->start_settings.stream_url);
            size_t limit = (text_limit_width*len)/width;

            BKNI_Snprintf(truncated, sizeof(truncated), "... %s", client->start_settings.stream_url + (len - limit));
            bfont_draw_aligned_text(&client->desc, client->font, &textrect, truncated, -1, 0xFFCCCCCC, bfont_valign_center, bfont_halign_left);
        }

        media_player_get_playback_status(client->player, &status);
        status.last -= status.first;
        snprintf(duration, sizeof(duration), "%lu:%02lu", status.last / 1000 / 60, (status.last / 1000) % 60);
        bfont_measure_text(client->font, duration, -1, &width, &height, &base);

        textrect.x = textrect.x + textrect.width - width;
        textrect.width = width;
        bfont_draw_aligned_text(&client->desc, client->font, &textrect, duration, -1, 0xFFCCCCCC, bfont_valign_center, bfont_halign_left);

        NEXUS_Surface_Flush(client->surface);
    }
}

static int gui_set_pos(struct client_state *client, unsigned position, unsigned first, unsigned last)
{
    NEXUS_Graphics2DFillSettings fillSettings;
    int rc;

    if (!client->surfaceClient) return -1;

    position -= first;
    last -= first;
    if (position >= last) position = last;
    if (!last) return 0;

    if (client->start_settings.loopMode == NEXUS_PlaybackLoopMode_ePlay) {
        /* have to redraw each time because time updates */
        gui_draw_bar(client);
    }

    NEXUS_Graphics2D_GetDefaultFillSettings(&fillSettings);
    fillSettings.surface = client->surface;

    if (client->last_cursor.width) {
        fillSettings.color = TIMELINE_COLOR;
        fillSettings.rect = client->last_cursor;
        rc = NEXUS_Graphics2D_Fill(client->gfx, &fillSettings);
        ALOG_ASSERT(!rc);
    }

    fillSettings.color = CURSOR_COLOR;
    fillSettings.rect = client->timeline_rect;
    fillSettings.rect.x = client->timeline_rect.x + ((client->timeline_rect.width-CURSOR_WIDTH) * position/last);
    fillSettings.rect.width = CURSOR_WIDTH;
    fillSettings.rect.height = CURSOR_HEIGHT;
    rc = NEXUS_Graphics2D_Fill(client->gfx, &fillSettings);
    ALOG_ASSERT(!rc);

    client->last_cursor = fillSettings.rect;

    checkpoint(client);
    rc = NEXUS_SurfaceClient_UpdateSurface(client->surfaceClient, NULL);
    ALOG_ASSERT(!rc);

    return 0;
}

static void gui_uninit(struct client_state *client)
{
    if (!client->surfaceClient) return;

    NEXUS_Graphics2D_Close(client->gfx);
    NEXUS_Surface_Destroy(client->surface);
    BKNI_DestroyEvent(client->checkpointEvent);
    if (client->font) {
        bfont_close(client->font);
    }
}
#else
int BcmSidebandFilePlayer::start(int x, int y, int w, int h)
{
    ALOGE("This application is not supported on this platform (needs input_router, playback and simple_decoder)!");
    return 0;
}
#endif
