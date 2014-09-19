/******************************************************************************
 *    (c)2010-2014 Broadcom Corporation
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
 * $brcm_Workfile: $
 * $brcm_Revision: $
 * $brcm_Date: $
 *
 *****************************************************************************/
#include "media_player.h"
#include "nexus_simple_video_decoder.h"
#include "nexus_simple_audio_decoder.h"
#include "nexus_simple_stc_channel.h"
#include "nexus_surface_client.h"
#include "nexus_playback.h"
#include "nexus_file.h"
#include "nexus_core_utils.h"
#include "namevalue.h"
#include "nxclient.h"
#if PLAYBACK_IP_SUPPORT
#include "b_playback_ip_lib.h"
#include "nexus_timebase.h"
#ifdef B_HAS_DTCP_IP
#include "b_dtcp_applib.h"
#endif
#endif

#include "blst_queue.h"

/* media probe */
#include "bmedia_probe.h"
#include "bmpeg2ts_probe.h"
#include "bfile_stdio.h"
#if B_HAS_ASF
#include "basf_probe.h"
#endif
#if B_HAS_AVI
#include "bavi_probe.h"
#endif
#include "bhevc_video_probe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "bstd.h"
#include "bkni.h"

BDBG_MODULE(media_player);

#if PLAYBACK_IP_SUPPORT
#define IP_NETWORK_MAX_JITTER 300 /* in msec */
#endif

BDBG_OBJECT_ID(media_player);
struct media_player
{
    BDBG_OBJECT(media_player)
    media_player_create_settings create_settings;
    media_player_start_settings start_settings;
    bool started;

    BLST_Q_ENTRY(media_player) link; /* for 'players' list */
    BLST_Q_HEAD(media_player_list, media_player) players; /* all players, including the master itself */
    media_player_t master; /* points to player[0], even for the master itself */
    unsigned mosaic_start_count;

    NEXUS_SimpleVideoDecoderHandle videoDecoder;
    NEXUS_VideoDecoderSettings videoDecoderSettings;
    NEXUS_SimpleAudioDecoderHandle audioDecoder;
    NEXUS_FilePlayHandle file;
    NEXUS_PlaypumpHandle playpump;
    NEXUS_PlaybackHandle playback;
    NEXUS_PidChannelHandle pcrPidChannel;
    NEXUS_SimpleStcChannelHandle stcChannel;
    bmedia_probe_t probe;
    const bmedia_probe_stream *stream;
    NxClient_AllocResults allocResults;
    unsigned allocIndex;
    unsigned connectId;
    NEXUS_SimpleVideoDecoderStartSettings videoProgram;
    NEXUS_SimpleAudioDecoderStartSettings audioProgram;
#if PLAYBACK_IP_SUPPORT
    B_PlaybackIpHandle playbackIp;
    bool playbackIpActive;
    bool playbackIpLiveMode;
    NEXUS_Timebase pacingTimebase;
    B_PlaybackIpPsiInfo playbackIpPsi;
#if B_HAS_DTCP_IP
    void * akeHandle;
    void * dtcpCtx;
    bool dtcpEnabled;
    int dtcpAkePort;
#endif
#endif
    dvr_crypto_t crypto;
    unsigned colorDepth;
};

void media_player_get_default_create_settings( media_player_create_settings *psettings )
{
    memset(psettings, 0, sizeof(*psettings));
    psettings->decodeVideo = true;
    psettings->decodeAudio = true;
}

void media_player_get_default_start_settings( media_player_start_settings *psettings )
{
    memset(psettings, 0, sizeof(*psettings));
    psettings->loop = true;
    psettings->decrypt.algo = NEXUS_SecurityAlgorithm_eMax; /* none */
    psettings->audio.dolbyDrcMode = NEXUS_AudioDecoderDolbyDrcMode_eMax; /* none */
}

static void endOfStreamCallback(void *context, int param)
{
    media_player_t player = context;
    BSTD_UNUSED(param);
    if (player->start_settings.eof) {
        (player->start_settings.eof)(player->start_settings.context);
    }
}

static media_player_t media_player_p_create(const NxClient_AllocResults *pAllocResults, unsigned allocIndex, const media_player_create_settings *psettings, bool mosaic)
{
    NEXUS_PlaypumpOpenSettings playpumpOpenSettings;
    NEXUS_PlaybackSettings playbackSettings;
    media_player_t player;
    int rc;

    player = malloc(sizeof(*player));
    if (!player) {
        return NULL;
    }
    memset(player, 0, sizeof(*player));
    BDBG_OBJECT_SET(player, media_player);
    player->create_settings = *psettings;
    player->allocResults = *pAllocResults;
    player->allocIndex = allocIndex;

    NEXUS_Playpump_GetDefaultOpenSettings(&playpumpOpenSettings);
    if (psettings->maxHeight <= 576) {
        playpumpOpenSettings.fifoSize = 1024*1024;
    }
    player->playpump = NEXUS_Playpump_Open(NEXUS_ANY_ID, &playpumpOpenSettings);
    if (!player->playpump) {
        free(player);
        BDBG_WRN(("no more playpumps available"));
        return NULL;
    }
    player->playback = NEXUS_Playback_Create();
    BDBG_ASSERT(player->playback);

    player->stcChannel = NEXUS_SimpleStcChannel_Create(NULL);
    if (!player->stcChannel) {
        BDBG_WRN(("stc channel not available"));
    }

    NEXUS_Playback_GetSettings(player->playback, &playbackSettings);
    playbackSettings.playpump = player->playpump;
    playbackSettings.simpleStcChannel = player->stcChannel;
    playbackSettings.stcTrick = !mosaic && player->stcChannel != NULL;
    playbackSettings.endOfStreamCallback.callback = endOfStreamCallback;
    playbackSettings.endOfStreamCallback.context = player;
    rc = NEXUS_Playback_SetSettings(player->playback, &playbackSettings);
    BDBG_ASSERT(!rc);

    if (player->allocResults.simpleVideoDecoder[player->allocIndex].id) {
        player->videoDecoder = NEXUS_SimpleVideoDecoder_Acquire(player->allocResults.simpleVideoDecoder[player->allocIndex].id);
        NEXUS_SimpleVideoDecoder_GetSettings(player->videoDecoder, &player->videoDecoderSettings);
    }
    if (psettings->decodeVideo && !player->videoDecoder) {
        BDBG_WRN(("video decoder not available"));
    }

    if (player->allocIndex == 0) {
        if (player->allocResults.simpleAudioDecoder.id) {
            player->audioDecoder = NEXUS_SimpleAudioDecoder_Acquire(player->allocResults.simpleAudioDecoder.id);
        }
        if (psettings->decodeAudio && !player->audioDecoder) {
            BDBG_WRN(("audio decoder not available"));
        }
    }

#if PLAYBACK_IP_SUPPORT
#ifdef B_HAS_DTCP_IP
    if (psettings->dtcpEnabled) {
        if(DtcpInitHWSecurityParams(NULL) != BERR_SUCCESS)
        {
            BDBG_ERR(("ERROR: Failed to init DtcpHW Security parmas\n"));
            return NULL;
        }

        /* initialize DtcpIp library */
        /* if App hasn't already opened the Nexus M2M DMA handle, then pass-in initial arg as NULL and let DTCP/IP library open the handle */
        if ((player->dtcpCtx = DtcpAppLib_Startup(B_DeviceMode_eSink, false, B_DTCP_KeyFormat_eCommonDRM, false)) == NULL) {
            BDBG_ERR(("ERROR: DtcpAppLib_Startup failed\n"));
            return NULL;
        }
        BDBG_ASSERT(player->dtcpCtx);
        player->dtcpEnabled = true;
        player->dtcpAkePort = 8000;
        player->akeHandle = NULL;
    }
#endif
    player->playbackIp = B_PlaybackIp_Open(NULL);
    BDBG_ASSERT(player->playbackIp);
#endif

    return player;
}

static bool media_player_p_test_disconnect(media_player_t player, NEXUS_SimpleVideoDecoderStartSettings *pVideoProgram)
{
    return player->connectId && player->videoDecoder &&
        (player->videoProgram.displayEnabled != pVideoProgram->displayEnabled ||
         !player->videoDecoderSettings.supportedCodecs[pVideoProgram->settings.codec] ||
         player->colorDepth != player->videoDecoderSettings.colorDepth);
}

/* connect client resources to server's resources */
static int media_player_p_connect(media_player_t player)
{
    NxClient_ConnectSettings connectSettings;
    int rc;

    BDBG_ASSERT(!player->master || player->master == player);

    if (player->videoDecoder && player->videoDecoderSettings.colorDepth != player->colorDepth) {
        /* no media_player API for colorDepth. instead, we force an NxClient_Connect based on a probe-detected
        colorDepth change from current settings. if we get dynamic changes from 8->10 in the future, we'll need a
        user setting to start with 10 bit. */
        NEXUS_SimpleVideoDecoder_GetSettings(player->videoDecoder, &player->videoDecoderSettings);
        player->videoDecoderSettings.colorDepth = player->colorDepth;
        rc = NEXUS_SimpleVideoDecoder_SetSettings(player->videoDecoder, &player->videoDecoderSettings);
        if (rc) return BERR_TRACE(rc);
    }

    NxClient_GetDefaultConnectSettings(&connectSettings);
    if (player->master) {
        unsigned i;
        media_player_t p;

        if (player->mosaic_start_count) {
            /* already connected */
            player->mosaic_start_count++;
            return 0;
        }
        for (i=0,p=BLST_Q_FIRST(&player->master->players);p;i++,p=BLST_Q_NEXT(p,link)) {
            const media_player_create_settings *psettings = &p->create_settings;
            connectSettings.simpleVideoDecoder[i].id = player->master->allocResults.simpleVideoDecoder[i].id;
            connectSettings.simpleVideoDecoder[i].surfaceClientId = psettings->window.surfaceClientId;
            connectSettings.simpleVideoDecoder[i].windowId = i;
            connectSettings.simpleVideoDecoder[i].decoderCapabilities.maxWidth = player->videoProgram.maxWidth;
            connectSettings.simpleVideoDecoder[i].decoderCapabilities.maxHeight = player->videoProgram.maxHeight;
            connectSettings.simpleVideoDecoder[i].decoderCapabilities.colorDepth = player->colorDepth;
            connectSettings.simpleVideoDecoder[i].windowCapabilities.type = player->start_settings.videoWindowType;
            /* connectSettings.simpleVideoDecoder[i].decoderCapabilities.supportedCodecs[player->videoProgram.settings.codec] = true; */
        }
        connectSettings.simpleAudioDecoder.id = player->master->allocResults.simpleAudioDecoder.id;
    }
    else {
        const media_player_create_settings *psettings = &player->create_settings;
        if (player->videoProgram.settings.pidChannel) {
            connectSettings.simpleVideoDecoder[0].id = player->allocResults.simpleVideoDecoder[0].id;
            connectSettings.simpleVideoDecoder[0].surfaceClientId = psettings->window.surfaceClientId;
            connectSettings.simpleVideoDecoder[0].windowId = psettings->window.id;
            connectSettings.simpleVideoDecoder[0].decoderCapabilities.maxWidth = player->videoProgram.maxWidth;
            connectSettings.simpleVideoDecoder[0].decoderCapabilities.maxHeight = player->videoProgram.maxHeight;
            connectSettings.simpleVideoDecoder[0].decoderCapabilities.supportedCodecs[player->videoProgram.settings.codec] = true;
            connectSettings.simpleVideoDecoder[0].decoderCapabilities.colorDepth = player->colorDepth;
            connectSettings.simpleVideoDecoder[0].windowCapabilities.type = player->start_settings.videoWindowType;
        }
        if (player->audioProgram.primary.pidChannel) {
            connectSettings.simpleAudioDecoder.id = player->allocResults.simpleAudioDecoder.id;
        }
    }
    rc = NxClient_Connect(&connectSettings, &player->connectId);
    if (rc) return BERR_TRACE(rc);

    if (player->master) {
        player->mosaic_start_count++;
    }

    return 0;
}

static void media_player_p_disconnect(media_player_t player)
{
    BDBG_ASSERT(!player->master || player->master == player);
    if (player->master) {
        if (!player->mosaic_start_count || --player->mosaic_start_count) return;
    }
    if (player->connectId) {
        NxClient_Disconnect(player->connectId);
        player->connectId = 0;
    }
}

media_player_t media_player_create( const media_player_create_settings *psettings )
{
    NxClient_JoinSettings joinSettings;
    NxClient_AllocSettings allocSettings;
    NxClient_AllocResults allocResults;
    media_player_t player;
    int rc;
    media_player_create_settings default_settings;

    if (!psettings) {
        media_player_get_default_create_settings(&default_settings);
        psettings = &default_settings;
    }

    /* connect to server and nexus */
    NxClient_GetDefaultJoinSettings(&joinSettings);
    snprintf(joinSettings.name, NXCLIENT_MAX_NAME, "%s", "media_player");
    rc = NxClient_Join(&joinSettings);
    if (rc) {
        return NULL;
    }

    NxClient_GetDefaultAllocSettings(&allocSettings);
    allocSettings.simpleVideoDecoder = psettings->decodeVideo?1:0;
    allocSettings.simpleAudioDecoder = psettings->decodeAudio?1:0;
    rc = NxClient_Alloc(&allocSettings, &allocResults);
    if (rc) { rc = BERR_TRACE(rc); goto error_alloc;}

    player = media_player_p_create(&allocResults, 0, psettings, false);
    if (!player) goto error_create_player;

    return player;

error_create_player:
    NxClient_Free(&allocResults);
error_alloc:
    NxClient_Uninit();
    return NULL;
}

int media_player_create_mosaics(media_player_t *players, unsigned num_mosaics, const media_player_create_settings *psettings )
{
    NxClient_JoinSettings joinSettings;
    NxClient_AllocSettings allocSettings;
    NxClient_AllocResults allocResults;
    int rc;
    media_player_create_settings default_settings;
    unsigned i;

    if (!psettings) {
        media_player_get_default_create_settings(&default_settings);
        psettings = &default_settings;
    }

    /* connect to server and nexus */
    NxClient_GetDefaultJoinSettings(&joinSettings);
    snprintf(joinSettings.name, NXCLIENT_MAX_NAME, "%s", "media_player");
    rc = NxClient_Join(&joinSettings);
    if (rc) return BERR_TRACE(rc);

    NxClient_GetDefaultAllocSettings(&allocSettings);
    allocSettings.simpleVideoDecoder = psettings->decodeVideo?num_mosaics:0;
    allocSettings.simpleAudioDecoder = psettings->decodeAudio?1:0;
    rc = NxClient_Alloc(&allocSettings, &allocResults);
    if (rc) { rc = BERR_TRACE(rc); goto error_alloc;}

    BKNI_Memset(players, 0, sizeof(players[0])*num_mosaics);
    for (i=0;i<num_mosaics;i++) {
        players[i] = media_player_p_create(&allocResults, i, psettings, true);
        if (!players[i]) {
            if (i == 0) {
                rc = BERR_TRACE(NEXUS_NOT_AVAILABLE);
                goto error_create_player;
            }
            /* if we can create at least one, go with it */
            break;
        }

        players[i]->master = players[0];
        BLST_Q_INSERT_TAIL(&players[0]->players, players[i], link);
    }

    return 0;

error_create_player:
    media_player_destroy_mosaics(players, num_mosaics);
    NxClient_Free(&allocResults);
error_alloc:
    NxClient_Uninit();
    BDBG_ASSERT(rc);
    return rc;
}

void media_player_destroy_mosaics( media_player_t *players, unsigned num_mosaics )
{
    int i;
    for (i=num_mosaics-1;i>=0;i--) {
        if (players[i]) {
            BLST_Q_REMOVE(&players[0]->players, players[i], link);
            media_player_destroy(players[i]);
        }
    }
}

static void b_print_media_string(const bmedia_probe_stream *stream)
{
    char stream_info[512];
    bmedia_stream_to_string(stream, stream_info, sizeof(stream_info));
    printf( "Media Probe:\n" "%s\n\n", stream_info);
}

/*
syntax: "scheme://domain:port/path?query_string#fragment_id"
each char array is null-terminated
*/
struct url {
    char scheme[32];
    char domain[128];
    unsigned port;
    char path[256]; /* contains "/path?query_string#fragment_id" */
};

#undef min
#define min(A,B) ((A)<(B)?(A):(B))

/* parse_url()

example: http://player.vimeo.com:80/play_redirect?quality=hd&codecs=h264&clip_id=638324

    scheme=http
    domain=player.vimeo.com
    port=80
    path=/play_redirect?quality=hd&codecs=h264&clip_id=638324

example: file://videos/cnnticker.mpg or videos/cnnticker.mpg

    scheme=file
    domain=
    port=0
    path=videos/cnnticker.mpg

example: udp://192.168.1.10:1234 || udp://224.1.1.10:1234 || udp://@:1234

    scheme=udp
    domain=live channel is streamed to this STB's IP address 192.168.1.10 || server is streaming to a multicast address 224.1.1.10 || this STB but user doesn't need to explicitly specify its IP address
    port=1234
    path=N/A as server is just streaming out a live channel, client doesn't get to pick a file

*/
static void parse_url(const char *s, struct url *url)
{
    const char *server, *file;

    memset(url, 0, sizeof(*url));

    server = strstr(s, "://");
    if (!server) {
        strcpy(url->scheme, "file");
        server = s;
    }
    else {
        strncpy(url->scheme, s, server-s);
        server += strlen("://"); /* points to the start of server name */
    }

    if (!strcmp(url->scheme, "file")) {
        strncpy(url->path, server, sizeof(url->path)-1);
    }
    else {
        char *port;
        file = strstr(server, "/"); /* should point to start of file name */
        if (file) {
            strncpy(url->domain, server, min(sizeof(url->domain)-1, (unsigned)(file-server)));
            strncpy(url->path, file, sizeof(url->path)-1);
        }
        else {
            strncpy(url->domain, server, sizeof(url->domain)-1);
        }

        /* now server string is null terminated, look for explicit port number in the format of server:port */
        port = strstr(url->domain, ":");
        if (port) {
            *port = 0;
            url->port = atoi(port+1);
        }
        else {
            url->port = 80; /* default port */
        }
    }
}

int media_player_start( media_player_t player, const media_player_start_settings *psettings )
{
    NEXUS_SimpleVideoDecoderStartSettings videoProgram;
    NEXUS_PlaybackPidChannelSettings playbackPidSettings;
    NEXUS_TransportType transportType = NEXUS_TransportType_eTs;
    NEXUS_TransportTimestampType timestampType = NEXUS_TransportTimestampType_eNone;
    NEXUS_PlaybackSettings playbackSettings;
    NxClient_DisplaySettings displaySettings;
    int rc;

    BDBG_OBJECT_ASSERT(player, media_player);
    if (player->started) {
        return BERR_TRACE(NEXUS_NOT_AVAILABLE);
    }
    if (!psettings) {
        return BERR_TRACE(NEXUS_INVALID_PARAMETER);
    }
    if (!psettings->stream_url) {
        return BERR_TRACE(NEXUS_INVALID_PARAMETER);
    }

    player->start_settings = *psettings;

    /* open pid channels and configure start settings based on probe */
    NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&videoProgram);
    videoProgram.displayEnabled = psettings->videoWindowType != NxClient_VideoWindowType_eNone;
    NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&player->audioProgram);

    NxClient_GetDisplaySettings(&displaySettings);
    if (displaySettings.hdmiPreferences.hdcp != psettings->hdcp) {
        displaySettings.hdmiPreferences.hdcp = psettings->hdcp;
        NxClient_SetDisplaySettings(&displaySettings);
    }

    /* probe the stream */
    {
        bfile_io_read_t fd = NULL;
        FILE *fin;
        bmedia_probe_config probe_config;
        const bmedia_probe_track *track;
        struct url url;

        parse_url(psettings->stream_url, &url);

        if (!strcasecmp(url.scheme, "http") || !strcasecmp(url.scheme, "https")) {
#if PLAYBACK_IP_SUPPORT
            /* URL contains http src info, setup IP playback */
            B_PlaybackIpSessionOpenSettings ipSessionOpenSettings;
            B_PlaybackIpSessionOpenStatus ipSessionOpenStatus;
            B_PlaybackIpSessionSetupSettings ipSessionSetupSettings;
            B_PlaybackIpSessionSetupStatus ipSessionSetupStatus;
            NEXUS_Error rc;

#if B_HAS_DTCP_IP
            if (player->dtcpEnabled)
            {
                /* Perform AKE for DTCP/IP */
                if((rc = DtcpAppLib_DoAkeOrVerifyExchKey(player->dtcpCtx, url.domain, player->dtcpAkePort, &(player->akeHandle))) != BERR_SUCCESS) {
                    BDBG_ERR(("ip_client_dtcp: DTCP AKE Failed!!!\n"));
                    goto error;
                }
            }
#endif
            /* Setup socket setting structure used in the IP Session Open */
            B_PlaybackIp_GetDefaultSessionOpenSettings(&ipSessionOpenSettings);
            ipSessionOpenSettings.socketOpenSettings.protocol = B_PlaybackIpProtocol_eHttp;
            strncpy(ipSessionOpenSettings.socketOpenSettings.ipAddr, url.domain, sizeof(ipSessionOpenSettings.socketOpenSettings.ipAddr)-1);
            ipSessionOpenSettings.socketOpenSettings.port = url.port;
            ipSessionOpenSettings.socketOpenSettings.url = url.path;
            BDBG_WRN(("parsed url is http://%s:%d%s", ipSessionOpenSettings.socketOpenSettings.ipAddr, ipSessionOpenSettings.socketOpenSettings.port, ipSessionOpenSettings.socketOpenSettings.url));
            ipSessionOpenSettings.ipMode = B_PlaybackIpClockRecoveryMode_ePull;

#if B_HAS_DTCP_IP
            if (player->dtcpEnabled)
            {
                ipSessionOpenSettings.security.securityProtocol = B_PlaybackIpSecurityProtocol_DtcpIp;
                ipSessionOpenSettings.security.initialSecurityContext = player->akeHandle;
            }
#endif
            rc = B_PlaybackIp_SessionOpen(player->playbackIp, &ipSessionOpenSettings, &ipSessionOpenStatus);
            if (rc) { rc = BERR_TRACE(rc); goto error; }
            BDBG_MSG (("Session Open call succeeded, HTTP status code %d", ipSessionOpenStatus.u.http.statusCode));

            /* now do session setup */
            B_PlaybackIp_GetDefaultSessionSetupSettings(&ipSessionSetupSettings);
            /* if app needs to play multiple formats (such as a DLNA DMP/DMR) (e.g. TS, VOB/PES, MP4, ASF, etc.), then set this option to do deep payload inspection */
            ipSessionSetupSettings.u.http.enablePayloadScanning = true;
            /* set a limit on how long the psi parsing should continue before returning */
            ipSessionSetupSettings.u.http.psiParsingTimeLimit = 30000; /* 30sec */
            rc = B_PlaybackIp_SessionSetup(player->playbackIp, &ipSessionSetupSettings, &ipSessionSetupStatus);
            if (rc) { rc = BERR_TRACE(rc); goto error; }
            BDBG_MSG (("Session Setup call succeeded, file handle %p", ipSessionSetupStatus.u.http.file));
            player->stream = (bmedia_probe_stream *)(ipSessionSetupStatus.u.http.stream);
            player->file = ipSessionSetupStatus.u.http.file;
            player->playbackIpActive = true;
            if (ipSessionSetupStatus.u.http.psi.liveChannel)
                player->playbackIpLiveMode = true;
            else
                player->playbackIpLiveMode = false;
            player->playbackIpPsi = ipSessionSetupStatus.u.http.psi;
            transportType = player->playbackIpPsi.mpegType;
#else
            BDBG_ERR(("%s not suppported. Rebuild with PLAYBACK_IP_SUPPORT=y", url.scheme));
            rc = -1;
            goto error;
#endif
        }
        else if (!strcasecmp(url.scheme, "udp") || !strcasecmp(url.scheme, "rtp")) {
#if PLAYBACK_IP_SUPPORT
            /* URL contains a live IP channel info, setup IP playback */
            B_PlaybackIpSessionOpenSettings ipSessionOpenSettings;
            B_PlaybackIpSessionOpenStatus ipSessionOpenStatus;
            B_PlaybackIpSessionSetupSettings ipSessionSetupSettings;
            B_PlaybackIpSessionSetupStatus ipSessionSetupStatus;
            NEXUS_Error rc;

            /* Setup socket setting structure used in the IP Session Open */
            B_PlaybackIp_GetDefaultSessionOpenSettings(&ipSessionOpenSettings);
            ipSessionOpenSettings.maxNetworkJitter = IP_NETWORK_MAX_JITTER;
            ipSessionOpenSettings.networkTimeout = 1;  /* timeout in 1 sec during network outage events */
            if (!strcasecmp(url.scheme, "rtp")) {
                ipSessionOpenSettings.socketOpenSettings.protocol = B_PlaybackIpProtocol_eRtp;
            }
            else {
                ipSessionOpenSettings.socketOpenSettings.protocol = B_PlaybackIpProtocol_eUdp;
            }
            strncpy(ipSessionOpenSettings.socketOpenSettings.ipAddr, url.domain, sizeof(ipSessionOpenSettings.socketOpenSettings.ipAddr)-1);
            ipSessionOpenSettings.socketOpenSettings.port = url.port;
#if 0
            /* needed for RTSP */
            ipSessionOpenSettings.socketOpenSettings.url = url.path;
#endif
            BDBG_MSG(("parsed url is udp://%s:%d%s", ipSessionOpenSettings.socketOpenSettings.ipAddr, ipSessionOpenSettings.socketOpenSettings.port, ipSessionOpenSettings.socketOpenSettings.url));
            ipSessionOpenSettings.ipMode = B_PlaybackIpClockRecoveryMode_ePushWithPcrSyncSlip;

            rc = B_PlaybackIp_SessionOpen(player->playbackIp, &ipSessionOpenSettings, &ipSessionOpenStatus);
            if (rc) { rc = BERR_TRACE(rc); goto error; }
            BDBG_MSG(("Session Open call succeeded"));

            /* now do session setup */
            B_PlaybackIp_GetDefaultSessionSetupSettings(&ipSessionSetupSettings);
            /* set a limit on how long the psi parsing should continue before returning */
            ipSessionSetupSettings.u.udp.psiParsingTimeLimit = 3000; /* 3sec */
            rc = B_PlaybackIp_SessionSetup(player->playbackIp, &ipSessionSetupSettings, &ipSessionSetupStatus);
            if (rc) { rc = BERR_TRACE(rc); goto error; }
            BDBG_MSG(("Session Setup call succeeded"));
            player->stream = (bmedia_probe_stream *)(ipSessionSetupStatus.u.udp.stream);
            player->file = NULL;
            player->playbackIpActive = true;
            player->playbackIpLiveMode = true;
            player->playbackIpPsi = ipSessionSetupStatus.u.udp.psi;
            transportType = player->playbackIpPsi.mpegType;
            BDBG_MSG(("Video Pid %d, Video Codec %d, Audio Pid %d, Audio Codec %d, Pcr Pid %d, Transport Type %d",
                player->playbackIpPsi.videoPid, player->playbackIpPsi.videoCodec, player->playbackIpPsi.audioPid, player->playbackIpPsi.audioCodec, player->playbackIpPsi.pcrPid, player->playbackIpPsi.mpegType));
#else
            BDBG_ERR(("%s not suppported. Rebuild with PLAYBACK_IP_SUPPORT=y", url.scheme));
            rc = -1;
            goto error;
#endif
        }
        else {
            const char *index_url = NULL;
            const char *probe_file = url.path;

            player->probe = bmedia_probe_create();

            fin = fopen64(probe_file, "rb");
            if (!fin) {
                BDBG_ERR(("can't open '%s'", url.path));
                rc = -1;
                goto error;
            }

            fd = bfile_stdio_read_attach(fin);

            bmedia_probe_default_cfg(&probe_config);
            probe_config.file_name = probe_file;
            probe_config.type = bstream_mpeg_type_unknown;
            player->stream = bmedia_probe_parse(player->probe, fd, &probe_config);

            /* now stream is either NULL, or stream descriptor with linked list of audio/video tracks */
            bfile_stdio_read_detach(fd);

            fclose(fin);
            if(!player->stream) {
                BDBG_ERR(("media probe can't parse '%s'", url.path));
                rc = -1;
                goto error;
            }

            index_url = psettings->index_url;
            if (!index_url) {
                if (player->stream->index == bmedia_probe_index_available || player->stream->index == bmedia_probe_index_required) {
                    /* if user didn't specify an index, use the file as index if probe indicates */
                    index_url = url.path;
                }
            }

            /* enable stream processing for ES audio streams */
            if (player->stream->type == bstream_mpeg_type_es) {
                track = BLST_SQ_FIRST(&player->stream->tracks);
                if (track && track->type == bmedia_track_type_audio) {
                    index_url = url.path;
                    NEXUS_Playback_GetSettings(player->playback, &playbackSettings);
                    playbackSettings.enableStreamProcessing = true;
                    (void)NEXUS_Playback_SetSettings(player->playback, &playbackSettings);
                }
            }

            player->file = NEXUS_FilePlay_OpenPosix(url.path, index_url);
            if (!player->file) {
                BDBG_ERR(("can't open '%s' and '%s'", url.path, index_url));
                rc = -1;
                goto error;
            }
        }

        if (player->stream) {
            if (!psettings->quiet) b_print_media_string(player->stream);
            transportType = b_mpegtype2nexus(player->stream->type);
            if (player->stream->type == bstream_mpeg_type_ts) {
                if ((((bmpeg2ts_probe_stream*)player->stream)->pkt_len) == 192) {
                    timestampType = NEXUS_TransportTimestampType_eMod300;
                }
            }
        }

        /* if we've created the probe, we must be stopped */
        player->started = true;
        player->colorDepth = 8;

#if PLAYBACK_IP_SUPPORT
        if (player->playbackIpActive && (player->playbackIpLiveMode || player->playbackIpPsi.hlsSessionEnabled || player->playbackIpPsi.mpegDashSessionEnabled || player->playbackIpPsi.numPlaySpeedEntries)) {
            B_PlaybackIpSettings playbackIpSettings;
            NEXUS_PlaypumpSettings playpumpSettings;
            NEXUS_SimpleStcChannelSettings stcSettings;
            NEXUS_PidChannelHandle pcrPidChannel = NULL;
            NEXUS_PlaypumpOpenPidChannelSettings pidChannelSettings;
            /* playback IP channel is in the live mode, it differs from regular playback in following ways: */
            /* -use nexus playback to feed IP playloads */
            /* -configure simpleStc in the live mode to use the PCRs */
            /* for streams w/ *no* transport timestamps, following additional changes are done */
            /* -program highJitter value in the simpleStc. This tells simpleStc to increase the PCR error thresholds */
            /* -delay decode by ip jitter value to account to absorb high network jitter */
            /* -increase CDB size to make sure that additional space for dejitter buffer is available */
            /* -TODO: check if default CDB sizes are big enough for additional buffering of IP_NETWORK_MAX_JITTER msec */

            /* configure nexus playpump */
            NEXUS_Playpump_GetSettings(player->playpump, &playpumpSettings);
            playpumpSettings.transportType = player->playbackIpPsi.mpegType;

            /* TTS streams */
            if (player->playbackIpPsi.transportTimeStampEnabled) {
                NEXUS_TimebaseSettings timebaseSettings;
                playpumpSettings.timestamp.type = NEXUS_TransportTimestampType_e32_Binary;
                /* Must use the timebase after Video decoders & Video Encodres */
                #ifdef NEXUS_NUM_VIDEO_ENCODERS
                player->pacingTimebase = NEXUS_Timebase_Open(NEXUS_Timebase_e0+NEXUS_NUM_VIDEO_DECODERS + NEXUS_NUM_VIDEO_ENCODERS);
                #else
                player->pacingTimebase = NEXUS_Timebase_Open(NEXUS_Timebase_e0+NEXUS_NUM_VIDEO_DECODERS);
                #endif
                NEXUS_Timebase_GetSettings(player->pacingTimebase,&timebaseSettings);
                timebaseSettings.sourceType = NEXUS_TimebaseSourceType_eFreeRun;
                timebaseSettings.freeze = true;
                timebaseSettings.sourceSettings.pcr.trackRange = NEXUS_TimebaseTrackRange_e122ppm;
                NEXUS_Timebase_SetSettings(player->pacingTimebase,&timebaseSettings);

                playpumpSettings.timestamp.timebase = player->pacingTimebase;
                playpumpSettings.timestamp.pacingMaxError = playbackIpSettings.ttsParams.pacingMaxError;
                playpumpSettings.timestamp.pacing = true;
                playpumpSettings.timestamp.pacingOffsetAdjustDisable = true;
                playpumpSettings.timestamp.parityCheckDisable = true;
                playpumpSettings.timestamp.resetPacing = true;
                playpumpSettings.timestamp.parityCheckDisable = true;
            }

            rc = NEXUS_Playpump_SetSettings(player->playpump, &playpumpSettings);
            if (rc) { rc = BERR_TRACE(rc); goto error; }

            B_PlaybackIp_GetSettings(player->playbackIp, &playbackIpSettings);
            playbackIpSettings.useNexusPlaypump = true;

            /* TTS streams */
            if (player->playbackIpPsi.transportTimeStampEnabled) {
                BDBG_WRN(("@@@ Enabling TTS in media_player_start()"));
                playbackIpSettings.ipMode = B_PlaybackIpClockRecoveryMode_ePushWithTtsNoSyncSlip;
            }
            rc = B_PlaybackIp_SetSettings(player->playbackIp, &playbackIpSettings);
            if (rc) { rc = BERR_TRACE(rc); goto error; }

            /* Open the pid channels */
            if (player->playbackIpPsi.videoCodec != NEXUS_VideoCodec_eNone && player->playbackIpPsi.videoPid) {
                NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&pidChannelSettings);
                pidChannelSettings.pidType = NEXUS_PidType_eVideo;
                videoProgram.settings.pidChannel = NEXUS_Playpump_OpenPidChannel(player->playpump, player->playbackIpPsi.videoPid, &pidChannelSettings);
                if (!videoProgram.settings.pidChannel) { rc = BERR_TRACE(rc); goto error; }
                videoProgram.settings.codec = player->playbackIpPsi.videoCodec;
                if (!player->playbackIpPsi.transportTimeStampEnabled && player->playbackIpLiveMode) { /* if timestamps are present, playpump buffer is used as de-jitter buffer */
                    /* increase the ptsOffset so that CDB can be used at the de-jitter buffer */
                    NEXUS_SimpleVideoDecoder_GetSettings(player->videoDecoder, &player->videoDecoderSettings);
                    player->videoDecoderSettings.ptsOffset = IP_NETWORK_MAX_JITTER * 45;    /* In 45Khz clock */
                    rc = NEXUS_SimpleVideoDecoder_SetSettings(player->videoDecoder, &player->videoDecoderSettings);
                    if (rc) { rc = BERR_TRACE(rc); goto error; }
                }
                if (player->create_settings.maxWidth && player->create_settings.maxHeight) {
                    videoProgram.maxWidth = player->create_settings.maxWidth;
                    videoProgram.maxHeight = player->create_settings.maxHeight;
                }
                BDBG_MSG(("%s: video pid %d, pid channel created", __FUNCTION__, player->playbackIpPsi.videoPid));
            }

            /* Open the extra video pid channel if present in the stream */
            if (player->playbackIpPsi.extraVideoCodec != NEXUS_VideoCodec_eNone && player->playbackIpPsi.extraVideoPid != 0) {
                NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&pidChannelSettings);
                pidChannelSettings.pidType = NEXUS_PidType_eVideo;
                videoProgram.settings.enhancementPidChannel = NEXUS_Playpump_OpenPidChannel(player->playpump, player->playbackIpPsi.extraVideoPid, &pidChannelSettings);
                if (!videoProgram.settings.enhancementPidChannel) { rc = BERR_TRACE(rc); goto error; }
                BDBG_MSG(("%s: extra video pid %d, codec %d is present, pid channel created", __FUNCTION__, player->playbackIpPsi.extraVideoPid, player->playbackIpPsi.extraVideoCodec));
            }

            if (player->playbackIpPsi.audioCodec != NEXUS_AudioCodec_eUnknown && player->playbackIpPsi.audioPid) {
                NEXUS_SimpleAudioDecoderSettings settings;
                /* Open the audio pid channel */
                NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&pidChannelSettings);
                pidChannelSettings.pidType = NEXUS_PidType_eAudio;
                pidChannelSettings.pidTypeSettings.audio.codec = player->playbackIpPsi.audioCodec;
                player->audioProgram.primary.pidChannel = NEXUS_Playpump_OpenPidChannel(player->playpump, player->playbackIpPsi.audioPid, &pidChannelSettings);
                if (!player->audioProgram.primary.pidChannel) { rc = BERR_TRACE(rc); goto error; }
                player->audioProgram.primary.codec = player->playbackIpPsi.audioCodec;
                if (!player->playbackIpPsi.transportTimeStampEnabled && player->playbackIpLiveMode) { /* if timestamps are present, playpump buffer is used as de-jitter buffer */
                    /* increase the ptsOffset so that CDB can be used at the de-jitter buffer */
                    NEXUS_SimpleAudioDecoder_GetSettings(player->audioDecoder, &settings);
                    settings.primary.ptsOffset = IP_NETWORK_MAX_JITTER * 45;    /* In 45Khz clock */
                    rc = NEXUS_SimpleAudioDecoder_SetSettings(player->audioDecoder, &settings);
                    if (rc) { rc = BERR_TRACE(rc); goto error; }
                }
            }

            if (player->playbackIpPsi.pcrPid && player->playbackIpPsi.pcrPid != player->playbackIpPsi.audioPid && player->playbackIpPsi.pcrPid != player->playbackIpPsi.videoPid) {
                /* Open the pcr pid channel */
                NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&pidChannelSettings);
                pidChannelSettings.pidType = NEXUS_PidType_eUnknown;
                pcrPidChannel = NEXUS_Playpump_OpenPidChannel(player->playpump, player->playbackIpPsi.pcrPid, &pidChannelSettings);
                if (!pcrPidChannel) { rc = BERR_TRACE(rc); goto error; }
                BDBG_MSG(("%s: Opened pcr pid channel %u for pcr pid %u ", __FUNCTION__, pcrPidChannel, player->playbackIpPsi.pcrPid));
            }
            else
            {
                if (player->playbackIpPsi.pcrPid == player->playbackIpPsi.audioPid)
                    pcrPidChannel = player->audioProgram.primary.pidChannel;
                else
                    pcrPidChannel = videoProgram.settings.pidChannel;
            }

            /* now configure the simple stc channel */
            if (!player->stcChannel) { rc = BERR_TRACE(rc); goto error; }
            NEXUS_SimpleStcChannel_GetSettings(player->stcChannel, &stcSettings);
            if (player->playbackIpPsi.hlsSessionEnabled || player->playbackIpPsi.mpegDashSessionEnabled || player->playbackIpPsi.numPlaySpeedEntries) {
                /* For HLS & MPEG DASH protocols, we configure clock recovery in pull mode as content is being pulled. We can't use playpump because playback doesn't understand HLS/DASH protocols. */
                stcSettings.mode = NEXUS_StcChannelMode_eAuto;
                stcSettings.modeSettings.Auto.transportType = transportType;
            }
            else if (player->playbackIpPsi.transportTimeStampEnabled) {
                /* when timestamps are present, dejittering is done before feeding to xpt and */
                /* thus is treated as a live playback */
                /* High Jitter mode for tts will need to be adjusted*/
                stcSettings.modeSettings.highJitter.mode = NEXUS_SimpleStcChannelHighJitterMode_eNone;
                stcSettings.mode = NEXUS_StcChannelMode_ePcr;
                stcSettings.modeSettings.pcr.pidChannel = pcrPidChannel;
                stcSettings.modeSettings.pcr.disableJitterAdjustment = true;
                stcSettings.modeSettings.pcr.disableTimestampCorrection = true;
                stcSettings.modeSettings.Auto.transportType   = transportType;
            }
            else {
                /* when timestamps are not present, we directly feed the jittered packets to the transport */
                /* and set the max network jitter in highJitterThreshold. */
                /* This enables the SimpleStc to internally program the various stc & timebase related thresholds to account for network jitter */
                stcSettings.modeSettings.highJitter.threshold = IP_NETWORK_MAX_JITTER;
                stcSettings.modeSettings.highJitter.mode = NEXUS_SimpleStcChannelHighJitterMode_eDirect;
                stcSettings.mode = NEXUS_StcChannelMode_ePcr;
                stcSettings.modeSettings.pcr.pidChannel = pcrPidChannel;
            }
            rc = NEXUS_SimpleStcChannel_SetSettings(player->stcChannel, &stcSettings);
            if (rc) { rc = BERR_TRACE(rc); goto error; }
            BDBG_MSG(("%s: live IP mode configuration complete...", __FUNCTION__));
        }
        else
#endif
        {
            unsigned cur_program_offset = 0, prev_program = 0;
            bool prev_program_set = false;

            /* playback mode setup for either file or HTTP playback */
            if (player->stcChannel) {
                NEXUS_SimpleStcChannelSettings stcSettings;
                NEXUS_SimpleStcChannel_GetSettings(player->stcChannel, &stcSettings);
                if (stcSettings.modeSettings.Auto.transportType != transportType) {
                    stcSettings.modeSettings.Auto.transportType = transportType;
                    rc = NEXUS_SimpleStcChannel_SetSettings(player->stcChannel, &stcSettings);
                    if (rc) { rc = BERR_TRACE(rc); goto error; }
                }
            }

            NEXUS_Playback_GetSettings(player->playback, &playbackSettings);
            playbackSettings.playpumpSettings.transportType = transportType;
            playbackSettings.playpumpSettings.timestamp.type = timestampType;
            playbackSettings.endOfStreamAction = psettings->loop?NEXUS_PlaybackLoopMode_eLoop:NEXUS_PlaybackLoopMode_ePause;
            rc = NEXUS_Playback_SetSettings(player->playback, &playbackSettings);
            if (rc) { rc = BERR_TRACE(rc); goto error; }

            for(track=BLST_SQ_FIRST(&player->stream->tracks);track;track=BLST_SQ_NEXT(track, link)) {
                if (track->type == bmedia_track_type_audio || track->type == bmedia_track_type_video) {
                    if (prev_program_set) {
                        if (track->program != prev_program) {
                            cur_program_offset++;
                        }
                    }
                    prev_program = track->program;
                    prev_program_set = true;
                }
                if (cur_program_offset != psettings->program) continue;

                switch(track->type) {
                    case bmedia_track_type_audio:
                        if(track->info.audio.codec != baudio_format_unknown && player->audioDecoder) {
                            if (!player->audioProgram.primary.pidChannel) {
                                NEXUS_Playback_GetDefaultPidChannelSettings(&playbackPidSettings);
                                playbackPidSettings.pidSettings.pidType = NEXUS_PidType_eAudio;
                                playbackPidSettings.pidSettings.pidTypeSettings.audio.codec = b_audiocodec2nexus(track->info.audio.codec);
                                playbackPidSettings.pidTypeSettings.audio.simpleDecoder = player->audioDecoder;
                                player->audioProgram.primary.pidChannel = NEXUS_Playback_OpenPidChannel(player->playback, track->number, &playbackPidSettings);
                                player->audioProgram.primary.codec = playbackPidSettings.pidSettings.pidTypeSettings.audio.codec;
                            }
                        }
                        break;
                    case bmedia_track_type_video:
                        if (player->videoDecoder) {
                            if (track->info.video.codec == bvideo_codec_h264_svc || track->info.video.codec == bvideo_codec_h264_mvc) {
                                if (videoProgram.settings.pidChannel && !videoProgram.settings.enhancementPidChannel) {
                                    NEXUS_Playback_GetDefaultPidChannelSettings(&playbackPidSettings);
                                    playbackPidSettings.pidSettings.pidType = NEXUS_PidType_eVideo;
                                    playbackPidSettings.pidTypeSettings.video.codec = b_videocodec2nexus(track->info.video.codec);
                                    playbackPidSettings.pidTypeSettings.video.index = true;
                                    playbackPidSettings.pidTypeSettings.video.simpleDecoder = player->videoDecoder;
                                    videoProgram.settings.enhancementPidChannel = NEXUS_Playback_OpenPidChannel(player->playback, track->number, &playbackPidSettings);
                                    videoProgram.settings.codec = b_videocodec2nexus(track->info.video.codec); /* overwrite */
                                }
                                break;
                            }
                            else if (track->info.video.codec != bvideo_codec_unknown) {
                                if (!videoProgram.settings.pidChannel) {
                                    NEXUS_Playback_GetDefaultPidChannelSettings(&playbackPidSettings);
                                    playbackPidSettings.pidSettings.pidType = NEXUS_PidType_eVideo;
                                    playbackPidSettings.pidTypeSettings.video.codec = b_videocodec2nexus(track->info.video.codec);
                                    playbackPidSettings.pidTypeSettings.video.index = true;
                                    playbackPidSettings.pidTypeSettings.video.simpleDecoder = player->videoDecoder;
                                    videoProgram.settings.pidChannel = NEXUS_Playback_OpenPidChannel(player->playback, track->number, &playbackPidSettings);
                                    videoProgram.settings.codec = b_videocodec2nexus(track->info.video.codec);
                                    if (player->create_settings.maxWidth && player->create_settings.maxHeight) {
                                        videoProgram.maxWidth = player->create_settings.maxWidth;
                                        videoProgram.maxHeight = player->create_settings.maxHeight;
                                    }
                                    if (track->info.video.width > videoProgram.maxWidth) {
                                        videoProgram.maxWidth = track->info.video.width;
                                    }
                                    if (track->info.video.height > videoProgram.maxHeight) {
                                        videoProgram.maxHeight = track->info.video.height;
                                    }
                                }
                            }
                            if (track->info.video.codec == bvideo_codec_h265) {
                                player->colorDepth = ((bmedia_probe_h265_video*)&track->info.video.codec_specific)->sps.bit_depth_luma;
                            }
                        }
                        break;
#if 0
                        /* TODO: need playback to handle duplicate w/ different settings in the case of eOther */
                    case bmedia_track_type_pcr:
                        NEXUS_Playback_GetDefaultPidChannelSettings(&playbackPidSettings);
                        playbackPidSettings.pidSettings.pidType = NEXUS_PidType_eOther;
                        player->pcrPidChannel = NEXUS_Playback_OpenPidChannel(player->playback, track->number, &playbackPidSettings);
                        break;
#endif
                    default:
                        break;
                }
            }

#if B_HAS_AVI
            if (player->stream->type == bstream_mpeg_type_avi && ((bavi_probe_stream *)player->stream)->video_framerate) {
                NEXUS_LookupFrameRate(((bavi_probe_stream *)player->stream)->video_framerate, &videoProgram.settings.frameRate);
            }
#endif
        }
    }

    if (!videoProgram.settings.pidChannel && !player->audioProgram.primary.pidChannel) {
        BDBG_WRN(("no content found for program %d", psettings->program));
        goto error;
    }

    if (media_player_p_test_disconnect(player->master?player->master:player, &videoProgram)) {
        media_player_p_disconnect(player->master?player->master:player);
    }
    player->videoProgram = videoProgram;
    if (!player->connectId) {
        rc = media_player_p_connect(player->master?player->master:player);
        if (rc) { rc = BERR_TRACE(rc); goto error; }
    }

    /* crypto */
    if (player->start_settings.decrypt.algo < NEXUS_SecurityAlgorithm_eMax) {
        struct dvr_crypto_settings settings;
        unsigned total = 0;
        dvr_crypto_get_default_settings(&settings);
        settings.algo = player->start_settings.decrypt.algo;
        if (player->videoProgram.settings.pidChannel) {
            settings.pid[total++] = player->videoProgram.settings.pidChannel;
        }
        if (player->audioProgram.primary.pidChannel) {
            settings.pid[total++] = player->audioProgram.primary.pidChannel;
        }
        player->crypto = dvr_crypto_create(&settings);
        /* if it fails, keep going */
    }

    /* apply codec settings */
    if (player->start_settings.audio.dolbyDrcMode < NEXUS_AudioDecoderDolbyDrcMode_eMax) {
        NEXUS_AudioDecoderCodecSettings settings;
        NEXUS_SimpleAudioDecoder_GetCodecSettings(player->audioDecoder, NEXUS_SimpleAudioDecoderSelector_ePrimary, player->audioProgram.primary.codec, &settings);
        switch (player->audioProgram.primary.codec) {
        case NEXUS_AudioCodec_eAc3: settings.codecSettings.ac3.drcMode = player->start_settings.audio.dolbyDrcMode; break;
        case NEXUS_AudioCodec_eAc3Plus: settings.codecSettings.ac3Plus.drcMode = player->start_settings.audio.dolbyDrcMode; break;
        /* only line and rf applie for aac/aacplus, but nexus can validate params */
        case NEXUS_AudioCodec_eAac: settings.codecSettings.aac.drcMode = (NEXUS_AudioDecoderDolbyPulseDrcMode)player->start_settings.audio.dolbyDrcMode; break;
        case NEXUS_AudioCodec_eAacPlus: settings.codecSettings.aacPlus.drcMode = (NEXUS_AudioDecoderDolbyPulseDrcMode)player->start_settings.audio.dolbyDrcMode; break;
        default: /* just ignore */ break;
        }
        NEXUS_SimpleAudioDecoder_SetCodecSettings(player->audioDecoder, NEXUS_SimpleAudioDecoderSelector_ePrimary, &settings);
    }

    /* set StcChannel to all decoders before starting any */
    if (player->videoProgram.settings.pidChannel) {
        rc = NEXUS_SimpleVideoDecoder_SetStcChannel(player->videoDecoder, player->stcChannel);
        if (rc) { rc = BERR_TRACE(rc); goto error; }
    }
    if (player->audioProgram.primary.pidChannel) {
        rc = NEXUS_SimpleAudioDecoder_SetStcChannel(player->audioDecoder, player->stcChannel);
        if (rc) { rc = BERR_TRACE(rc); goto error; }
    }

    if (player->videoProgram.settings.pidChannel) {
        rc = NEXUS_SimpleVideoDecoder_Start(player->videoDecoder, &player->videoProgram);
        if (rc) { rc = BERR_TRACE(rc); goto error; }
    }
    if (player->audioProgram.primary.pidChannel) {
        rc = NEXUS_SimpleAudioDecoder_Start(player->audioDecoder, &player->audioProgram);
        if (rc) { rc = BERR_TRACE(rc); goto error; }
        /* decode may fail if audio codec not supported */
    }

#if PLAYBACK_IP_SUPPORT
    if (player->playbackIpActive && (player->playbackIpLiveMode || player->playbackIpPsi.hlsSessionEnabled || player->playbackIpPsi.mpegDashSessionEnabled || player->playbackIpPsi.numPlaySpeedEntries )) {
        BDBG_MSG(("starting nexus playpump"));
        rc = NEXUS_Playpump_Start(player->playpump);
    }
    else
#endif
    {
        BDBG_MSG(("starting nexus playback"));
        rc = NEXUS_Playback_Start(player->playback, player->file, NULL);
    }
    if (rc) { rc = BERR_TRACE(rc); goto error; }

#if PLAYBACK_IP_SUPPORT
    if (player->playbackIpActive) {
        B_PlaybackIpSessionStartSettings ipSessionStartSettings;
        B_PlaybackIpSessionStartStatus ipSessionStartStatus;
        /* B_PlaybackIp_GetDefaultSessionStartSettings(&ipSessionStartSettings);*/
        memset(&ipSessionStartSettings, 0, sizeof(ipSessionStartSettings));
        /* set Nexus handles */
        ipSessionStartSettings.nexusHandles.playpump = player->playpump;
        ipSessionStartSettings.nexusHandles.playback = player->playback;
        ipSessionStartSettings.nexusHandles.simpleVideoDecoder = player->videoDecoder;
        ipSessionStartSettings.nexusHandles.simpleStcChannel = player->stcChannel;
        ipSessionStartSettings.nexusHandles.simpleAudioDecoder = player->audioDecoder;
        ipSessionStartSettings.nexusHandles.videoPidChannel = player->videoProgram.settings.pidChannel;
        ipSessionStartSettings.nexusHandles.extraVideoPidChannel = player->videoProgram.settings.enhancementPidChannel;
        ipSessionStartSettings.nexusHandles.audioPidChannel = player->audioProgram.primary.pidChannel;
        ipSessionStartSettings.nexusHandles.pcrPidChannel = player->pcrPidChannel;
        ipSessionStartSettings.nexusHandlesValid = true;
        ipSessionStartSettings.mpegType = transportType;
        rc = B_PlaybackIp_SessionStart(player->playbackIp, &ipSessionStartSettings, &ipSessionStartStatus);
        if (rc) { rc = BERR_TRACE(rc); goto error; }
        BDBG_MSG(("playback playback in %s mode is started", player->playbackIpLiveMode?"Live":"Playback"));
    }
#endif

    player->started = true;
    return 0;

error:
    if (player->probe) {
        if (player->stream) {
            bmedia_probe_stream_free(player->probe, player->stream);
            player->stream = NULL;
        }
        bmedia_probe_destroy(player->probe);
        player->probe = NULL;
    }
#if PLAYBACK_IP_SUPPORT
    if (player->playbackIpActive) {
        B_PlaybackIp_SessionStop(player->playbackIp);
        B_PlaybackIp_SessionClose(player->playbackIp);
        player->playbackIpActive = false;
    }
#endif
    media_player_stop(player);
    return -1;
}

void media_player_stop(media_player_t player)
{
    BDBG_OBJECT_ASSERT(player, media_player);

    if (!player->started) return;

    if (player->videoDecoder) {
        NEXUS_SimpleVideoDecoder_Stop(player->videoDecoder);
        NEXUS_SimpleVideoDecoder_SetStcChannel(player->videoDecoder, NULL);
    }
    if (player->audioDecoder) {
        NEXUS_SimpleAudioDecoder_Stop(player->audioDecoder);
        NEXUS_SimpleAudioDecoder_SetStcChannel(player->audioDecoder, NULL);
    }
    if (player->crypto) {
        dvr_crypto_destroy(player->crypto);
    }
#if PLAYBACK_IP_SUPPORT
    if (player->playbackIpActive) {
        if (player->playbackIpLiveMode || player->playbackIpPsi.hlsSessionEnabled || player->playbackIpPsi.mpegDashSessionEnabled || player->playbackIpPsi.numPlaySpeedEntries ) {
            if (player->playpump) {
                NEXUS_Playpump_Stop(player->playpump);
                NEXUS_Playpump_CloseAllPidChannels(player->playpump);
                player->playbackIpLiveMode = false;
            }
            if (player->pacingTimebase)
            {
               NEXUS_Timebase_Close(player->pacingTimebase);
            }
        }
        else {
            /* playback mode */
            if (player->playback) {
                NEXUS_Playback_Stop(player->playback);
                NEXUS_Playback_CloseAllPidChannels(player->playback);
            }
        }
        player->videoProgram.settings.pidChannel = NULL;
        player->audioProgram.primary.pidChannel = NULL;
        player->pcrPidChannel = NULL;
        B_PlaybackIp_SessionStop(player->playbackIp);
        B_PlaybackIp_SessionClose(player->playbackIp);
        player->playbackIpActive = false;

#ifdef B_HAS_DTCP_IP
        /* close the per session handle */
        if (player->dtcpEnabled)
        {
            if (DtcpAppLib_CloseAke(player->dtcpCtx, player->akeHandle) != BERR_SUCCESS)
            {
                BDBG_WRN(("%s: failed to close the DTCP AKE session", __FUNCTION__));
            }
        }
#endif
    }
    else
#endif
    {
        /* regular file playback case */
        if (player->playback) {
            NEXUS_Playback_Stop(player->playback);
            if (player->videoProgram.settings.pidChannel) {
                NEXUS_Playback_ClosePidChannel(player->playback, player->videoProgram.settings.pidChannel);
                player->videoProgram.settings.pidChannel = NULL;
            }
            if (player->audioProgram.primary.pidChannel) {
                NEXUS_Playback_ClosePidChannel(player->playback, player->audioProgram.primary.pidChannel);
                player->audioProgram.primary.pidChannel = NULL;
            }
            if (player->pcrPidChannel) {
                NEXUS_Playback_ClosePidChannel(player->playback, player->pcrPidChannel);
                player->pcrPidChannel = NULL;
            }
        }
        if (player->file) {
            NEXUS_FilePlay_Close(player->file);
        }
        if (player->probe) {
            if (player->stream) {
                bmedia_probe_stream_free(player->probe, player->stream);
                player->stream = NULL;
            }
            bmedia_probe_destroy(player->probe);
        }
    }
    player->started = false;
}

void media_player_destroy(media_player_t player)
{
    bool uninit;
    BDBG_OBJECT_ASSERT(player, media_player);
    uninit = !player->master || !BLST_Q_FIRST(&player->master->players);
    if (player->started) {
        media_player_stop(player);
        if (player->connectId) {
            media_player_p_disconnect(player->master?player->master:player);
        }
    }
    if (player->playback) {
        NEXUS_Playback_Destroy(player->playback);
    }
    if (player->playpump) {
        NEXUS_Playpump_Close(player->playpump);
    }
    if (player->videoDecoder) {
        NEXUS_SimpleVideoDecoder_Release(player->videoDecoder);
    }
    if (player->audioDecoder) {
        NEXUS_SimpleAudioDecoder_Release(player->audioDecoder);
    }
    if (player->stcChannel) {
        NEXUS_SimpleStcChannel_Destroy(player->stcChannel);
    }
#if PLAYBACK_IP_SUPPORT
    if (player->playbackIp) {
        B_PlaybackIp_Close(player->playbackIp);
    }
#ifdef B_HAS_DTCP_IP
        /* close the per session handle */
        if (player->dtcpEnabled)
        {
            DtcpAppLib_Shutdown(player->dtcpCtx);
            DtcpCleanupHwSecurityParams();
        }
#endif
#endif
    if (uninit) {
        NxClient_Free(&player->allocResults);
    }
    BDBG_OBJECT_DESTROY(player, media_player);
    free(player);
    if (uninit) {
        NxClient_Uninit();
    }
}

#if PLAYBACK_IP_SUPPORT
int media_ip_player_trick( media_player_t player, int rate)
{
    B_PlaybackIpTrickModesSettings ipTrickModeSettings;
    B_PlaybackIp_GetTrickModeSettings(player->playbackIp, &ipTrickModeSettings);

    if (rate == NEXUS_NORMAL_DECODE_RATE) {
        return B_PlaybackIp_Play(player->playbackIp);
    }
    else if (rate == 0) {
        ipTrickModeSettings.pauseMethod = B_PlaybackIpPauseMethod_UseDisconnectAndSeek;
        return B_PlaybackIp_Pause(player->playbackIp, &ipTrickModeSettings);
    }
    else {
        ipTrickModeSettings.method = B_PlaybackIpTrickModesMethod_UseByteRange;
        ipTrickModeSettings.rate = rate;
        return B_PlaybackIp_TrickMode(player->playbackIp, &ipTrickModeSettings);
    }
}
#endif

int media_player_trick( media_player_t player, int rate)
{
    BDBG_OBJECT_ASSERT(player, media_player);
    if (!player->started) {
        return BERR_TRACE(NEXUS_NOT_AVAILABLE);
    }
#if PLAYBACK_IP_SUPPORT
    if (player->playbackIpActive) {
        return media_ip_player_trick(player, rate);
    }
#endif
    if (rate == NEXUS_NORMAL_DECODE_RATE) {
        return NEXUS_Playback_Play(player->playback);
    }
    else if (rate == 0) {
        return NEXUS_Playback_Pause(player->playback);
    }
    else {
        NEXUS_PlaybackTrickModeSettings settings;
        NEXUS_Playback_GetDefaultTrickModeSettings(&settings);
        settings.rate = rate;
        return NEXUS_Playback_TrickMode(player->playback, &settings);
    }
}

int media_player_get_playback_status( media_player_t player, NEXUS_PlaybackStatus *pstatus )
{
#if PLAYBACK_IP_SUPPORT
    if (player->playbackIpActive) {
        int rc;
        B_PlaybackIpStatus playbackIpStatus;
        rc = B_PlaybackIp_GetStatus(player->playbackIp, &playbackIpStatus);
        if (!rc) {
            pstatus->first = playbackIpStatus.first;
            pstatus->last = playbackIpStatus.last;
            pstatus->position = playbackIpStatus.position;
            return NEXUS_SUCCESS;
        }
        else {
            return NEXUS_UNKNOWN;
        }
    }
#endif
    return NEXUS_Playback_GetStatus(player->playback, pstatus);
}

NEXUS_SimpleVideoDecoderHandle media_player_get_video_decoder(media_player_t player)
{
    return player->videoDecoder;
}

#if PLAYBACK_IP_SUPPORT
int media_ip_player_seek( media_player_t player, int offset, int whence )
{
    B_PlaybackIpStatus playbackIpStatus;
    B_PlaybackIpTrickModesSettings ipTrickModeSettings;
    int rc;
    switch (whence) {
    case SEEK_SET:
    default:
        break;
    case SEEK_CUR:
        rc = B_PlaybackIp_GetStatus(player->playbackIp, &playbackIpStatus);
        if (!rc) offset += playbackIpStatus.position;
        break;
    case SEEK_END:
        rc = B_PlaybackIp_GetStatus(player->playbackIp, &playbackIpStatus);
        if (!rc) offset += playbackIpStatus.last;
        break;
    }
    B_PlaybackIp_GetTrickModeSettings(player->playbackIp, &ipTrickModeSettings);
    ipTrickModeSettings.seekPosition = offset;
    return B_PlaybackIp_Seek(player->playbackIp, &ipTrickModeSettings);
}
#endif

int media_player_seek( media_player_t player, int offset, int whence )
{
    NEXUS_PlaybackStatus status;
    int rc;
#if PLAYBACK_IP_SUPPORT
    if (player->playbackIpActive) {
        return media_ip_player_seek(player, offset, whence);
    }
#endif
    switch (whence) {
    case SEEK_SET:
    default:
        break;
    case SEEK_CUR:
        rc = NEXUS_Playback_GetStatus(player->playback, &status);
        if (!rc) offset += status.position;
        break;
    case SEEK_END:
        rc = NEXUS_Playback_GetStatus(player->playback, &status);
        if (!rc) offset += status.last;
        break;
    }
    return NEXUS_Playback_Seek(player->playback, offset);
}

#if B_REFSW_TR69C_SUPPORT
int media_player_get_set_tr69c_info(void *context, enum b_tr69c_type type, union b_tr69c_info *info)
{
    media_player_t player = context;
    int rc;
    switch (type)
    {
#if PLAYBACK_IP_SUPPORT
        case b_tr69c_type_get_playback_ip_status:
            rc = B_PlaybackIp_GetStatus(player->playbackIp, &info->playback_ip_status);
            if (rc) return BERR_TRACE(rc);
            break;
#endif
        case b_tr69c_type_get_playback_status:
            rc = NEXUS_Playback_GetStatus(player->playback, &info->playback_status);
            if (rc) return BERR_TRACE(rc);
            break;
        case b_tr69c_type_get_video_decoder_status:
            rc = NEXUS_SimpleVideoDecoder_GetStatus(player->videoDecoder, &info->video_decoder_status);
            if (rc) return BERR_TRACE(rc);
            break;
        case b_tr69c_type_get_audio_decoder_status:
            rc = NEXUS_SimpleAudioDecoder_GetStatus(player->audioDecoder, &info->audio_decoder_status);
            if (rc) return BERR_TRACE(rc);
            break;
        case b_tr69c_type_set_video_decoder_mute:
        {
            NEXUS_VideoDecoderSettings settings;
            NEXUS_SimpleVideoDecoder_GetSettings(player->videoDecoder, &settings);
            settings.mute = info->video_decoder_mute;
            rc = NEXUS_SimpleVideoDecoder_SetSettings(player->videoDecoder, &settings);
            if (rc) return BERR_TRACE(rc);
            break;
        }
        default:
            return BERR_TRACE(NEXUS_NOT_SUPPORTED);
    }
    return 0;
}
#endif
