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
 *****************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "bomx_aac_parser"

#include "bomx_aac_parser.h"
#include "biobits.h"
#include "bioatom.h"
#include <cutils/log.h>

// Below utilities borrowed from bmedia_util.c

/* ISO/IEC 13818-7:2005(E) */
/* Table 35 . Sampling frequency dependent on sampling_frequency_index. Must remain sorted in decending order */
static const unsigned b_aac_adts_sample_rate[]={
    96000,
    88200,
    64000,
    48000,
    44100,
    32000,
    24000,
    22050,
    16000,
    12000,
    11025,
    8000
};

static bool BOMX_AAC_SetSamplingFrequencyIndex(BOMX_AAC_ASCInfo *pInfo, unsigned sampling_frequency)
{
    unsigned i, dif;

    for(i=0;i<sizeof(b_aac_adts_sample_rate)/sizeof(*b_aac_adts_sample_rate);i++) {
        if (b_aac_adts_sample_rate[i] == sampling_frequency) {
            pInfo->samplingFrequencyIndex = i;
            return true;
        } else
            if(b_aac_adts_sample_rate[i] < sampling_frequency) {
                break;
        }
    }

    /* Find the closest samplerate in the list to the desired sample rate.
       We use the previous sample rate index if:
          1. The provided sample rate is less than any on the list (i == num_sample_rates)
          2. The provided sample rate is not larger than any on the list (i>0) *and*
             The provided sample rate is closer to the previous list rate than it is to the current list rate */
    if ((i == sizeof(b_aac_adts_sample_rate)/sizeof(*b_aac_adts_sample_rate)) ||
        (i>0 && ((sampling_frequency - b_aac_adts_sample_rate[i]) > (b_aac_adts_sample_rate[i-1] - sampling_frequency)))){
        i--;
    }

    if(sampling_frequency > 0) {
        /* Allow for a successful hit with a .5% margin of error */
        dif = (b_aac_adts_sample_rate[i] * 1000) / sampling_frequency;
        if ((dif >= 995) && (dif <= 1005)) {
            pInfo->samplingFrequencyIndex = i;
            return true;
        }
    }
    ALOGW("BOMX_AAC_SetSamplingFrequencyIndex: unknown frequency %u", sampling_frequency);
    return false;
}

static unsigned BOMX_AAC_GetSamplingFrequencyFromIndex(unsigned index) {
    if (index < sizeof(b_aac_adts_sample_rate)/sizeof(*b_aac_adts_sample_rate)) {
        return b_aac_adts_sample_rate[index];
    }
    return 0;
}

static void BOMX_AAC_ConvertAscToAdts(BOMX_AAC_ASCInfo *pInfo)
{
    pInfo->sbrPresent = false;
    switch(pInfo->profile)
    {
    /* AAC Main */
    case 1:
        pInfo->profile = 0;
        break;
    /* AAC LC and AAC SBR */
    case 2:
        pInfo->profile = 1;
        break;
    case 5:
        pInfo->sbrPresent = true;
        pInfo->profile = 1;
        break;
    /* AAC SSR */
    case 3:
        pInfo->profile = 2;
        break;
    /*AAC LTP */
    case 4:
        pInfo->profile = 3;
        break;
    /* There are other types of Audio object types, but non fit in to any of the ADTS profiles.
       There has been no test content for the other types.  Defaulting to AAC LC */
    default:
        pInfo->profile = 1;
        break;
    }
    /* If the stream is SBR we need to program the ADTS header with the AAC-LC sample rate index */
    if (pInfo->sbrPresent)
    {
        size_t sample_rate;

        sample_rate = BOMX_AAC_GetSamplingFrequencyFromIndex(pInfo->samplingFrequencyIndex);
        sample_rate = sample_rate / 2;
        BOMX_AAC_SetSamplingFrequencyIndex(pInfo,sample_rate);
    }
}

bool BOMX_AAC_ParseASC(const uint8_t *pData, size_t length, BOMX_AAC_ASCInfo *pInfo)
{
    int temp;
    int byte;
    bool delay_flag;
    bool ext_flag;
    bool fl_flag;
    bool sbr;
    batom_bitstream bs;
    batom_checkpoint check_point;
    batom_cursor curs, *cursor=&curs;
    batom_vec vec;

    batom_vec_init(&vec, pData, length);
    batom_cursor_from_vec(cursor, &vec, 1);

    batom_cursor_save(cursor, &check_point);

    batom_bitstream_init(&bs, cursor);
    pInfo->profile = batom_bitstream_bits(&bs,5);
    pInfo->samplingFrequencyIndex = batom_bitstream_bits(&bs,4);

    if (pInfo->samplingFrequencyIndex == 0x0F)
    {
        ALOGW("BOMX_AAC_ParseASC: AudioSpecificConfig not supported samplingFrequencyIndex %#x, try basic parse", pInfo->samplingFrequencyIndex);
        goto basic_parse;
    }
    pInfo->channelConfiguration = batom_bitstream_bits(&bs,4);


    if (pInfo->profile == 5)
    {
        pInfo->samplingFrequencyIndex = batom_bitstream_bits(&bs,4);

        if (pInfo->samplingFrequencyIndex == 0x0F)
        {
            ALOGW("BOMX_AAC_ParseASC: AudioSpecificConfig not supported samplingFrequencyIndex %#x, try basic parse", pInfo->samplingFrequencyIndex);
            goto basic_parse;
        }
        batom_bitstream_drop_bits(&bs, 5);
    }

    switch(pInfo->profile)
    {
        case 1:
        case 2:
        case 3:
        case 4:
        case 6:
        case 7:
        case 17:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
            fl_flag = batom_bitstream_bit(&bs);
            delay_flag = batom_bitstream_bit(&bs);
            if(delay_flag)
            {
                /* Delay is 14 bits */
                batom_bitstream_drop_bits(&bs, 14);
            }

            ext_flag = batom_bitstream_bit(&bs);

            if (pInfo->profile == 6 ||
                pInfo->profile == 20)
            {
                batom_bitstream_drop_bits(&bs, 13);
            }

            if (ext_flag)
            {
                if (pInfo->profile == 22)
                {
                    batom_bitstream_drop_bits(&bs, 16);
                }
                else if (pInfo->profile == 17 ||
                    pInfo->profile == 19 ||
                    pInfo->profile == 20 ||
                    pInfo->profile == 23)
                {
                    batom_bitstream_drop_bits(&bs, 3);
                }

                ext_flag = batom_bitstream_bit(&bs);
            }
            break;

        default:
            break;
    }

    switch (pInfo->profile) {
        case 17:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
            temp = batom_bitstream_bits(&bs,2);
            if (temp == 2 || temp == 3)
            {
                batom_bitstream_drop(&bs);
            }
            break;

        default:
            break;
    }

    if (pInfo->profile != 5)
    {
        temp = batom_bitstream_bits(&bs,11);
        if (temp == 0x2b7)
        {
            temp = batom_bitstream_bits(&bs,5);
            if (temp == 0x5)
            {
                sbr = batom_bitstream_bit(&bs);
                if (sbr)
                {
                    pInfo->profile  = temp;
                    pInfo->samplingFrequencyIndex = batom_bitstream_bits(&bs,4);

                    if (pInfo->samplingFrequencyIndex == 0x0F)
                    {
                        ALOGW("BOMX_AAC_ParseASC: AudioSpecificConfig not supported samplingFrequencyIndex %#x, try basic parse", pInfo->samplingFrequencyIndex);
                        goto basic_parse;
                    }
                }
            }
        }
    }

    BOMX_AAC_ConvertAscToAdts(pInfo);
    ALOGV("aac_info: profile:%u sampling_frequency_index:%u channel_configuration:%u", pInfo->profile, pInfo->samplingFrequencyIndex, pInfo->channelConfiguration);

    return true;

basic_parse:
    /* Table 1.8 . Syntax of AudioSpecificConfig , ISO/IEC 14496-3 MPEG4 Part-3, page 3 */
    batom_cursor_rollback(cursor,&check_point);
    byte = batom_cursor_uint16_be(cursor);
    if(byte==BATOM_EOF) {
        return false;
    }
    pInfo->profile = B_GET_BITS(byte, 15, 11);
    pInfo->samplingFrequencyIndex = B_GET_BITS(byte, 10, 7);
    if(pInfo->samplingFrequencyIndex ==0x0F) {
        ALOGW("BOMX_AAC_ParseASC: AudioSpecificConfig not supported samplingFrequencyIndex %#x", pInfo->samplingFrequencyIndex);
        return false;
    }
    pInfo->channelConfiguration = B_GET_BITS(byte, 6, 3);

    BOMX_AAC_ConvertAscToAdts(pInfo);
    ALOGV("aac_info:basic profile:%u sampling_frequency_index:%u channel_configuration:%u", pInfo->profile, pInfo->samplingFrequencyIndex, pInfo->channelConfiguration);

    return true;
}

bool BOMX_AAC_SetupAdtsHeader(uint8_t *pData, size_t maxLength, size_t *pBytesWritten, size_t frameLength, const BOMX_AAC_ASCInfo *pInfo)
{
    ALOG_ASSERT(NULL != pData);
    ALOG_ASSERT(NULL != pBytesWritten);
    ALOG_ASSERT(NULL != pInfo);
    *pBytesWritten = 0;
    if ( maxLength < BOMX_ADTS_HEADER_SIZE )
    {
        ALOGE("Insufficient length for ADTS header");
        return false;
    }

    frameLength += BOMX_ADTS_HEADER_SIZE;

    pData[0] =
        0xFF; /* sync word */
    pData[1] =
        0xF0 |  /* sync word */
        B_SET_BIT( ID, 0, 3) | /* MPEG-2 AAC */
        B_SET_BITS( "00", 0, 2, 1) |
        B_SET_BIT( protection_absent, 1, 0);

    pData[2] =
        B_SET_BITS( profile, pInfo->profile, 7, 6) |
        B_SET_BITS( sampling_frequency_index, pInfo->samplingFrequencyIndex, 5, 2 ) |
        B_SET_BIT( private_bit, 0, 1) |
        B_SET_BIT( channel_configuration[2], B_GET_BIT(pInfo->channelConfiguration, 2), 0);

    pData[3] = /* 4'th byte is shared */
        B_SET_BITS( channel_configuration[2], B_GET_BITS(pInfo->channelConfiguration, 1, 0), 7, 6) |
        B_SET_BIT( original_copy, 0, 5) |
        B_SET_BIT( home, 0, 4) |
        /* IS 13818-7 1.2.2 Variable Header of ADTS */
        B_SET_BIT( copyright_identification_bit, 0, 3) |
        B_SET_BIT( copyright_identification_start, 0, 2) |
        B_SET_BITS( aac_frame_length[12..11], B_GET_BITS(frameLength, 12, 11), 1, 0);
    pData[4] =
            B_SET_BITS(aac_frame_length[10..3], B_GET_BITS(frameLength, 10, 3), 7, 0);
    pData[5] =
            B_SET_BITS(aac_frame_length[2..0], B_GET_BITS(frameLength, 2, 0), 7, 5) |
            B_SET_BITS(adts_buffer_fullness[10..6], B_GET_BITS( 0x7FF /* VBR */, 10, 6), 4, 0);
    pData[6] =
            B_SET_BITS(adts_buffer_fullness[5..0], B_GET_BITS( 0x7FF /* VBR */, 5, 0), 7, 2) |
            B_SET_BITS(no_raw_data_blocks_in_frame, 0, 2, 0);

    *pBytesWritten = BOMX_ADTS_HEADER_SIZE;

    return true;
}
