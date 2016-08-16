/******************************************************************************
 *    (c)2016 Broadcom Corporation
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
#include "brcm_audio.h"
#include "brcm_audio_nexus_hdmi.h"

using namespace android;

struct hdmi_audio_format_map_t {
    NEXUS_AudioCodec nexusCodec;
    const char *formatString;
    bool compressed;
    bool channels_5_1;
    bool channels_7_1;
};

static struct hdmi_audio_format_map_t formatMap[] =
{
    {
        .nexusCodec = NEXUS_AudioCodec_ePcm,
        .formatString = "AUDIO_FORMAT_PCM_16_BIT",
        .compressed = false,
        .channels_5_1 = false,
        .channels_7_1 = false,
    },
    {
        .nexusCodec = NEXUS_AudioCodec_eAc3,
        .formatString = "AUDIO_FORMAT_AC3",
        .compressed = true,
        .channels_5_1 = true,
        .channels_7_1 = false,
    },
    {
        .nexusCodec = NEXUS_AudioCodec_eAc3Plus,
        .formatString = "AUDIO_FORMAT_E_AC3",
        .compressed = true,
        .channels_5_1 = true,
        .channels_7_1 = true,
    },
    {
        .nexusCodec = NEXUS_AudioCodec_eDts,
        .formatString = "AUDIO_FORMAT_DTS",
        .compressed = true,
        .channels_5_1 = true,
        .channels_7_1 = false,
    },
    {
        .nexusCodec = NEXUS_AudioCodec_eDtsHd,
        .formatString = "AUDIO_FORMAT_DTS_HD",
        .compressed = true,
        .channels_5_1 = true,
        .channels_7_1 = true,
    },
};

static int num_codecs = sizeof(formatMap)/sizeof(formatMap[0]);

void nexus_get_hdmi_parameters(String8& rates, String8& channels, String8& formats)
{
    NEXUS_HdmiOutputHandle hdmiHandle;
    NEXUS_HdmiOutputStatus hdmiStatus;
    NEXUS_PlatformConfiguration *pConfig;
    NEXUS_Error errCode;

    rates.clear();
    channels.clear();
    formats.clear();

    pConfig = (NEXUS_PlatformConfiguration *)BKNI_Malloc(sizeof(*pConfig));
    if (pConfig) {
        NEXUS_Platform_GetConfiguration(pConfig);
        hdmiHandle = pConfig->outputs.hdmi[0]; /* always first output. */
        BKNI_Free(pConfig);
        errCode = NEXUS_HdmiOutput_GetStatus(hdmiHandle, &hdmiStatus);
        if (!errCode) {
            int codec;
            bool compressed = false;
            bool channels_5_1 = false;
            bool channels_7_1 = false;

            for (codec = 0; codec < num_codecs; codec++) {
                if (hdmiStatus.audioCodecSupported[formatMap[codec].nexusCodec]) {
                    ALOGV("%s: codec %s supported\n", __PRETTY_FUNCTION__, formatMap[codec].formatString);

                    /* Check for compressed and multichannel codecs */
                    compressed = compressed || (formatMap[codec].compressed);
                    channels_5_1 = channels_5_1 || (formatMap[codec].channels_5_1);
                    channels_7_1 = channels_7_1 || (formatMap[codec].channels_7_1);

                    /* Append formats string */
                    if (!formats.isEmpty())
                        formats.append("|");
                    formats.append(formatMap[codec].formatString);
                }
            }

            /* Advertise the standard sampling rates and let Nexus do upsampling if necessary.
             * Add the 4K multiples of 44.1KHz and 48Khz if compressed codec is supported */
            rates.append("8000|11025|12000|16000|22050|24000|32000|44100|48000");
            if (compressed)
                rates.append("|176400|192000");

            channels.append("AUDIO_CHANNEL_OUT_STEREO");
            if (channels_5_1)
                channels.append("|AUDIO_CHANNEL_OUT_5POINT1");
            if (channels_7_1)
                channels.append("|AUDIO_CHANNEL_OUT_7POINT1");
        }
    }
}
