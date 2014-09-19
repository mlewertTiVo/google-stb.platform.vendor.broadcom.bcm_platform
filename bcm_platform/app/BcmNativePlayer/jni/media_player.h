/******************************************************************************
 *    (c)2010-2013 Broadcom Corporation
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
#ifndef MEDIA_PLAYER_H__
#define MEDIA_PLAYER_H__

#include "nexus_types.h"
#include "nexus_playback.h"
#include "dvr_crypto.h"
#include "nxclient.h"
#if B_REFSW_TR69C_SUPPORT
#include "tr69clib.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
NOTE: This API is only example code. It is subject to change and
is not supported as a standard reference software deliverable.
**/

/**
Summary:
settings for media_player_create
**/
typedef struct media_player_create_settings
{
    bool decodeVideo; /* if false, video is ignored */
    bool decodeAudio; /* if false, audio is ignored */
	bool dtcpEnabled; /* if false, dtcp_ip wont be used */
    struct {
        unsigned surfaceClientId;
        unsigned id;
    } window; /* assign window to control position */
    unsigned maxWidth, maxHeight; /* Minimum source resolution from decoder. if stream resolution is detected to be
                                     higher, player will raise. After connecting, resource cannot be raised: disconnect/connect
                                     required to raise resolution. */
} media_player_create_settings;

/**
Summary:
settings for media_player_start
**/
typedef struct media_player_start_settings
{
    const char *stream_url; /* required for dvr. if NULL, then live */
    const char *index_url; /* optional */
    unsigned program;

    bool loop;
    void (*eof)(void *context);
    void *context; /* context for any callbacks */
    NxClient_VideoWindowType videoWindowType;

    struct {
        NEXUS_SecurityAlgorithm algo; /* eMax means none */
    } decrypt;
    struct {
        NEXUS_AudioDecoderDolbyDrcMode dolbyDrcMode; /* applied to ac3, ac3+, aac and aac+ as able */
    } audio;

    bool quiet; /* don't print status */
    NxClient_HdcpLevel hdcp;
} media_player_start_settings;

typedef struct media_player *media_player_t;

/**
Summary:
**/
void media_player_get_default_create_settings(
    media_player_create_settings *psettings
    );

/**
Summary:
**/
media_player_t media_player_create(
    const media_player_create_settings *psettings /* optional */
    );

/**
Summary:
**/
void media_player_get_default_start_settings(
    media_player_start_settings *psettings
    );

/**
Start playback
**/
int media_player_start(
    media_player_t handle,
    const media_player_start_settings *psettings
    );

/**
Summary:
Perform trick mode
**/
int media_player_trick(
    media_player_t handle,
    int rate /* units are NEXUS_NORMAL_DECODE_RATE. for example:
                0 = pause
                NEXUS_NORMAL_DECODE_RATE = normal play
                2*NEXUS_NORMAL_DECODE_RATE = 2x fast forward
                -3*NEXUS_NORMAL_DECODE_RATE = 3x rewind
                */
    );

/**
Summary:
Seek based on time
**/
int media_player_seek(
    media_player_t handle,
    int offset, /* in milliseconds */
    int whence /* SEEK_SET, SEEK_CUR, SEEK_END */
    );

/**
Summary:
**/
int media_player_get_playback_status( media_player_t handle, NEXUS_PlaybackStatus *pstatus );

/**
Summary:
**/
void media_player_stop( media_player_t handle );

/**
Summary:
**/
void media_player_destroy( media_player_t handle );

/**
Summary:
Create a set of media players for mosaic decode

Each can be started and stopped independently. Creating in a bundle
allows the underlying NxClient_Alloc and _Connect calls to be batched as required.
**/
int media_player_create_mosaics(
    media_player_t *players, /* array of size num_mosaics. will return NULL for players that can't be created */
    unsigned num_mosaics, /* may create less than requested number */
    const media_player_create_settings *psettings /* optional */
    );

void media_player_destroy_mosaics(
    media_player_t *players, /* array of size num_mosaics. */
    unsigned num_mosaics
    );

/**
Summary:
**/
NEXUS_SimpleVideoDecoderHandle media_player_get_video_decoder(media_player_t player);

/**
Summary:
**/
#if B_REFSW_TR69C_SUPPORT
int media_player_get_set_tr69c_info(void *context, enum b_tr69c_type type, union b_tr69c_info *info);
#endif

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_PLAYER_H__ */
