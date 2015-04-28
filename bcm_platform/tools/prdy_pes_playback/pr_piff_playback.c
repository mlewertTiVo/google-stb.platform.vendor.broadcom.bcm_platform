/******************************************************************************
 *    (c)2008-2014 Broadcom Corporation
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
 * Module Description:
 *
 * Example to playback Playready DRM encrypted content
 *
 * Revision History:
 *
 * $brcm_Log: $
 *
 *****************************************************************************/
/* Nexus example app: Play Ready decrypt, PIFF parser, and PES conversion decode */
#define USE_BDBG_LOGGING    0
#if defined(ANDROID) && !(USE_BDBG_LOGGING)
#define LOG_NDEBUG 0
#include <utils/Log.h>
#define LOG_TAG "pr_piff_playback"
#define LOGV(x) ALOGV x
#define LOGD(x) ALOGD x
#define LOGE(x) ALOGE x
#define LOGW(x) ALOGW x
#else
#include "bstd.h"
#include "bdbg.h"
BDBG_MODULE(pr_piff_playback);
#define BDBG_MODULE_NAME "pr_piff_playback"
#define LOGV BDBG_MSG
#define LOGD BDBG_MSG
#define LOGE BDBG_ERR
#define LOGW BDBG_WRN
#endif

#include "nexus_platform.h"
#include "nexus_video_decoder.h"
#include "nexus_stc_channel.h"
#include "nexus_display.h"
#include "nexus_video_window.h"
#include "nexus_video_input.h"
#include "nexus_spdif_output.h"
#include "nexus_component_output.h"
#include "nexus_video_adj.h"
#include "nexus_playback.h"
#include "nexus_core_utils.h"

#include "common_crypto.h"
#include "drm_prdy_http.h"
#include "drm_prdy.h"
#include "drm_prdy_types.h"


#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "bkni.h"
#include "bkni_multi.h"
#include "bmp4_util.h"
#include "bbase64.h"
#include "piff_parser.h"
#include "bfile_stdio.h"
#include "bpiff.h"

#include "nxclient.h"
#include "nexus_surface_client.h"

#define NEED_TO_BE_TRUSTED_APP
#ifdef NEED_TO_BE_TRUSTED_APP
#include <cutils/properties.h>
#define NEXUS_TRUSTED_DATA_PATH "/data/misc/nexus"
#endif

#define USE_SECURE_VIDEO_PLAYBACK       1
#define USE_SECURE_AUDIO_PLAYBACK       1
#define USE_SECURE_PLAYBACK             (USE_SECURE_VIDEO_PLAYBACK || USE_SECURE_AUDIO_PLAYBACK)
#define DEBUG_OUTPUT_CAPTURE    0

#define ZORDER_TOP              10

#if USE_SECURE_PLAYBACK
#include <sage_srai.h>
#endif

#define REPACK_VIDEO_PES_ID 0xE0
#define REPACK_AUDIO_PES_ID 0xC0

#define BOX_HEADER_SIZE (8)
#define BUF_SIZE (1024 * 1024 * 2) /* 2MB */

#define CALCULATE_PTS(t)        (((uint64_t)(t) / 10000LL) * 45LL)


typedef struct app_ctx {
    FILE *fp_piff;
    uint32_t piff_filesize;

    uint8_t *pPayload;
    uint8_t *pOutBuf;

    size_t outBufSize;

    uint64_t last_video_fragment_time;
    uint64_t last_audio_fragment_time;

    DRM_Prdy_DecryptContext_t decryptor;
} app_ctx;

#if DEBUG_OUTPUT_CAPTURE
/* Input file pointer */
FILE *fp_vid;
FILE *fp_aud;
#endif

/* stream type */
int vc1_stream = 0;
typedef app_ctx * app_ctx_t;
static int video_decode_hdr;

static int gui_init( NEXUS_SurfaceClientHandle surfaceClient );

static int piff_playback_dma_buffer(CommonCryptoHandle commonCryptoHandle, void *dst,
        void *src, size_t size, bool flush)
{
    NEXUS_DmaJobBlockSettings blkSettings;
    CommonCryptoJobSettings cryptoJobSettings;

    NEXUS_DmaJob_GetDefaultBlockSettings(&blkSettings);
    blkSettings.pSrcAddr = src;
    blkSettings.pDestAddr = dst;
    blkSettings.blockSize = size;
    blkSettings.resetCrypto = true;
    blkSettings.scatterGatherCryptoStart = true;
    blkSettings.scatterGatherCryptoEnd = true;

    if (flush) {
        /* Need to flush manually the source buffer (non secure heap). We need to flush manually as soon as we copy data into
           the secure heap. Setting blkSettings[ii].cached = true would also try to flush the destination address in the secure heap
           which is not accessible. This would cause the whole memory to be flushed at once. */
        NEXUS_FlushCache(blkSettings.pSrcAddr, blkSettings.blockSize);
        blkSettings.cached = false; /* Prevent the DMA from flushing the buffers later on */
    }

    CommonCrypto_GetDefaultJobSettings(&cryptoJobSettings);
    CommonCrypto_DmaXfer(commonCryptoHandle,  &cryptoJobSettings, &blkSettings, 1);

    if (flush) {
        /* Need to flush manually the source buffer (non secure heap). We need to flush manually as soon as we copy data into
           the secure heap. Setting blkSettings[ii].cached = true would also try to flush the destination address in the secure heap
           which is not accessible. This would cause the whole memory to be flushed at once. */
        NEXUS_FlushCache(blkSettings.pSrcAddr, blkSettings.blockSize);
    }

    return 0;
}

static int parse_esds_config(bmedia_adts_hdr *hdr, bmedia_info_aac *info_aac, size_t payload_size)
{
    bmedia_adts_header adts_header;

    bmedia_adts_header_init_aac(&adts_header, info_aac);
    bmedia_adts_hdr_init(hdr, &adts_header, payload_size);

    return 0;
}

static int parse_avcc_config(uint8_t *avcc_hdr, size_t *hdr_len, size_t *nalu_len,
        uint8_t *cfg_data, size_t cfg_data_size)
{
    bmedia_h264_meta meta;
    unsigned int i, sps_len, pps_len;
    uint8_t *data;
    uint8_t *dst;
    size_t size;

    bmedia_read_h264_meta(&meta, cfg_data, cfg_data_size);

    *nalu_len = meta.nalu_len;

    data = (uint8_t *)meta.sps.data;
    dst = avcc_hdr;
    *hdr_len = 0;

    for(i=0; i < meta.sps.no; i++) {
        sps_len = (((uint16_t)data[0]) <<8) | data[1];
        data += 2;
        /* Add NAL */
        BKNI_Memcpy(dst, bpiff_nal, sizeof(bpiff_nal)); dst += sizeof(bpiff_nal);
        /* Add SPS */
        BKNI_Memcpy(dst, data, sps_len);
        dst += sps_len;
        data += sps_len;
        *hdr_len += (sizeof(bpiff_nal) + sps_len);
    }

    data = (uint8_t *)meta.pps.data;
    for(i=0; i < meta.pps.no; i++) {
        pps_len = (((uint16_t)data[0]) <<8) | data[1];
        data += 2;
        /* Add NAL */
        BKNI_Memcpy(dst, bpiff_nal, sizeof(bpiff_nal));
        dst += sizeof(bpiff_nal);
        /* Add PPS */
        BKNI_Memcpy(dst, data, pps_len);
        dst += pps_len;
        data += pps_len;
        *hdr_len += (sizeof(bpiff_nal) + pps_len);
    }
    return 0;
}

static int decrypt_sample(CommonCryptoHandle commonCryptoHandle,
        uint32_t sampleSize, batom_cursor * cursor,
        bpiff_drm_mp4_box_se_sample *pSample, uint32_t *bytes_processed,
        DRM_Prdy_DecryptContext_t *decryptor, uint32_t enc_flags,
        uint8_t *out, size_t decrypt_offset)
{
    uint32_t rc=0;
    uint8_t i=0;
    uint8_t *pOutBuf = out;
    uint8_t *src;
    uint8_t *dst;
    DRM_Prdy_AES_CTR_Info_t     aesCtrInfo;

    *bytes_processed = 0;

    BKNI_Memcpy( &aesCtrInfo.qwInitializationVector,&pSample->iv[8],8);
    aesCtrInfo.qwBlockOffset = 0;
    aesCtrInfo.bByteOffset = 0;

    if(enc_flags & 0x000002) {
        uint64_t     qwOffset = 0;

        for(i = 0; i <  pSample->nbOfEntries; i++) {
            uint32_t num_clear = pSample->entries[i].bytesOfClearData;
            uint32_t num_enc = pSample->entries[i].bytesOfEncData;

            /* Skip over clear units by offset amount */
            BDBG_ASSERT(num_clear >= decrypt_offset);
            batom_cursor_skip((batom_cursor *)cursor, decrypt_offset);

            /* Add NAL header per entry */
            if (!vc1_stream) {
                BKNI_Memcpy(out, bpiff_nal, sizeof(bpiff_nal));
                out += sizeof(bpiff_nal);
            }

            src = (uint8_t *)cursor->cursor;
            dst = out;

            piff_playback_dma_buffer(commonCryptoHandle, dst, src,
                    (num_enc + num_clear - decrypt_offset), false);

            /* Skip over remaining clear units */
            out += (num_clear - decrypt_offset);
            batom_cursor_skip((batom_cursor *)cursor, (num_clear - decrypt_offset));

            aesCtrInfo.qwBlockOffset = qwOffset / 16 ;
            aesCtrInfo.bByteOffset = qwOffset % 16 ;

            if(DRM_Prdy_Reader_Decrypt(
                        decryptor,
                        &aesCtrInfo,
                        (uint8_t *)out,
                        num_enc ) != DRM_Prdy_ok) {
                LOGE(("%s Reader_Decrypt failed - %u", __FUNCTION__, __LINE__));
                rc = -1;
                goto ErrorExit;
            }

            out += num_enc;
            batom_cursor_skip((batom_cursor *)cursor,num_enc);
            qwOffset = num_enc;
            if (!vc1_stream)
                *bytes_processed  += (num_clear - decrypt_offset +
                        num_enc + sizeof(bpiff_nal));
            else
                *bytes_processed  += (num_clear - decrypt_offset + num_enc);

            if( *bytes_processed > sampleSize) {
                LOGW(("Wrong buffer size is detected while decrypting the ciphertext, bytes processed %d, sample size to decrypt %d",*bytes_processed,sampleSize));
                rc = -1;
                goto ErrorExit;
            }
        }
    } else {
            src = (uint8_t *)cursor->cursor;
            dst = out;

            piff_playback_dma_buffer(commonCryptoHandle, dst, src, sampleSize, false);

            if(DRM_Prdy_Reader_Decrypt(
                        decryptor,
                        &aesCtrInfo,
                        (uint8_t *)out,
                        sampleSize ) != DRM_Prdy_ok) {
            LOGE(("%s Reader_Decrypt failed - %d", __FUNCTION__, __LINE__));
            rc = -1;
            goto ErrorExit;
        }

        out += (sampleSize);

        batom_cursor_skip((batom_cursor *)cursor, sampleSize);
        *bytes_processed = sampleSize;
    }

ErrorExit:
    return rc;
}

static int secure_process_fragment(CommonCryptoHandle commonCryptoHandle, app_ctx *app,
        piff_parse_frag_info *frag_info, size_t payload_size,
        void *decoder_data, size_t decoder_len,
        NEXUS_PlaypumpHandle playpump, BKNI_EventHandle event)
{
    int rc = 0;
    uint32_t i, j, bytes_processed;
    bpiff_drm_mp4_box_se_sample *pSample;
    uint8_t pes_header[BMEDIA_PES_HEADER_MAX_SIZE];
    size_t pes_header_len;
    bmedia_pes_info pes_info;
    uint64_t frag_duration;
    uint8_t *pOutBuf = app->pOutBuf;
    size_t decrypt_offset = 0;
    NEXUS_PlaypumpStatus playpumpStatus;
    size_t fragment_size = payload_size;
    void *playpumpBuffer;
    size_t bufferSize;
    size_t outSize = 0;
    uint8_t *out;
    DRM_Prdy_AES_CTR_Info_t aesCtrInfo;

    /* Obtain secure playpump buffer */
    NEXUS_Playpump_GetStatus(playpump, &playpumpStatus);
    fragment_size += 512;   /* Provide headroom for PES and SPS/PPS headers */
    BDBG_ASSERT(fragment_size <= playpumpStatus.fifoSize);
    for(;;) {
        NEXUS_Playpump_GetBuffer(playpump, &playpumpBuffer, &bufferSize);
        if(bufferSize >= fragment_size) {
            break;
        }
        if(bufferSize==0) {
            BKNI_WaitForEvent(event, 100);
            continue;
        }
        if((uint8_t *)playpumpBuffer >= (uint8_t *)playpumpStatus.bufferBase +
                (playpumpStatus.fifoSize - fragment_size)) {
            NEXUS_Playpump_WriteComplete(playpump, bufferSize, 0); /* skip buffer what wouldn't be big enough */
        }
    }

    app->outBufSize = 0;
    bytes_processed = 0;
    if (frag_info->samples_enc->sample_count != 0) {
        /* Iterate through the samples within the fragment */
        for (i = 0; i < frag_info->samples_enc->sample_count; i++) {
            size_t numOfByteDecrypted = 0;
            size_t sampleSize = 0;

            pSample = &frag_info->samples_enc->samples[i];
            sampleSize = frag_info->sample_info[i].size;

            if (frag_info->trackType == BMP4_SAMPLE_ENCRYPTED_VIDEO) {
                if (!vc1_stream) {
                    /* H.264 Decoder configuration parsing */
                    uint8_t avcc_hdr[BPIFF_MAX_PPS_SPS];
                    size_t avcc_hdr_size;
                    size_t nalu_len = 0;

                    pOutBuf = app->pOutBuf;
                    app->outBufSize = 0;

                    parse_avcc_config(avcc_hdr, &avcc_hdr_size, &nalu_len, decoder_data, decoder_len);

                    bmedia_pes_info_init(&pes_info, REPACK_VIDEO_PES_ID);
                    frag_duration = app->last_video_fragment_time +
                        frag_info->sample_info[i].composition_time_offset;
                    app->last_video_fragment_time += frag_info->sample_info[i].duration;

                    pes_info.pts_valid = true;
                    pes_info.pts = CALCULATE_PTS(frag_duration);
                    if (video_decode_hdr == 0) {
                        pes_header_len = bmedia_pes_header_init(pes_header,
                                (sampleSize + avcc_hdr_size - nalu_len + sizeof(bpiff_nal)), &pes_info);
                    } else {
                        pes_header_len = bmedia_pes_header_init(pes_header,
                                (sampleSize - nalu_len + sizeof(bpiff_nal)), &pes_info);
                    }

                    /* Copy PES header and SPS/PPS to intermediate buffer */
                    BKNI_Memcpy(pOutBuf, &pes_header, pes_header_len);
                    pOutBuf += pes_header_len;
                    app->outBufSize += pes_header_len;

                    /* Only add header once */
                    if (video_decode_hdr == 0) {
                        BKNI_Memcpy(pOutBuf, avcc_hdr, avcc_hdr_size);
                        pOutBuf += avcc_hdr_size;
                        app->outBufSize += avcc_hdr_size;
                        video_decode_hdr = 1;
                    }

                    decrypt_offset = nalu_len;

                    piff_playback_dma_buffer(commonCryptoHandle, (uint8_t *)playpumpBuffer + outSize,
                            app->pOutBuf, app->outBufSize, true);
                    outSize += app->outBufSize;
                }
            } else if (frag_info->trackType == BMP4_SAMPLE_ENCRYPTED_AUDIO) {
                if (!vc1_stream) {
                    /* AAC information parsing */
                    bmedia_adts_hdr hdr;
                    bmedia_info_aac *info_aac = (bmedia_info_aac *)decoder_data;

                    pOutBuf = app->pOutBuf;
                    app->outBufSize = 0;

                    parse_esds_config(&hdr, info_aac, sampleSize);

                    bmedia_pes_info_init(&pes_info, REPACK_AUDIO_PES_ID);
                    frag_duration = app->last_audio_fragment_time +
                        frag_info->sample_info[i].composition_time_offset;
                    app->last_audio_fragment_time += frag_info->sample_info[i].duration;

                    pes_info.pts_valid = true;
                    pes_info.pts = CALCULATE_PTS(frag_duration);

                    pes_header_len = bmedia_pes_header_init(pes_header,
                            (sampleSize + BMEDIA_ADTS_HEADER_SIZE), &pes_info);
                    BKNI_Memcpy(pOutBuf, &pes_header, pes_header_len);
                    BKNI_Memcpy(pOutBuf + pes_header_len, &hdr.adts, BMEDIA_ADTS_HEADER_SIZE);

                    pOutBuf += pes_header_len + BMEDIA_ADTS_HEADER_SIZE;
                    app->outBufSize += pes_header_len + BMEDIA_ADTS_HEADER_SIZE;

                    decrypt_offset = 0;

                    piff_playback_dma_buffer(commonCryptoHandle, (uint8_t *)playpumpBuffer + outSize,
                            app->pOutBuf, app->outBufSize, true);
                    outSize += app->outBufSize;
                }
            } else {
                LOGW(("%s Unsupported track type %d detected", __FUNCTION__, frag_info->trackType));
                return -1;
            }

            /* Process and decrypt samples */

            BKNI_Memcpy( &aesCtrInfo.qwInitializationVector,&pSample->iv[8],8);
            aesCtrInfo.qwBlockOffset = 0;
            aesCtrInfo.bByteOffset = 0;
            out = (uint8_t *)playpumpBuffer + outSize;

            if(frag_info->samples_enc->flags & 0x000002) {
                uint64_t     qwOffset = 0;

                for(j = 0; j < pSample->nbOfEntries; j++) {
                    uint32_t num_clear = pSample->entries[j].bytesOfClearData;
                    uint32_t num_enc = pSample->entries[j].bytesOfEncData;
                    int blk_idx = 0;
                    int k;
                    NEXUS_DmaJobBlockSettings blkSettings[2];
                    CommonCryptoJobSettings cryptoJobSettings;

                    /* Skip over clear units by offset amount */
                    BDBG_ASSERT(num_clear >= decrypt_offset);
                    batom_cursor_skip((batom_cursor *)&frag_info->cursor, decrypt_offset);

                    if (!vc1_stream) {
                        /* Append NAL Header */
                        BKNI_Memcpy(app->pOutBuf, bpiff_nal, sizeof(bpiff_nal));
                        NEXUS_DmaJob_GetDefaultBlockSettings(&blkSettings[blk_idx]);
                        blkSettings[blk_idx].pSrcAddr = app->pOutBuf;
                        blkSettings[blk_idx].pDestAddr = out;
                        blkSettings[blk_idx].blockSize = sizeof(bpiff_nal);
                        blkSettings[blk_idx].resetCrypto = true;
                        blkSettings[blk_idx].scatterGatherCryptoStart = true;
                        blkSettings[blk_idx].scatterGatherCryptoEnd = false;
                        blk_idx++;

                        out += sizeof(bpiff_nal);
                        outSize += sizeof(bpiff_nal);
                    }

                    NEXUS_DmaJob_GetDefaultBlockSettings(&blkSettings[blk_idx]);
                    blkSettings[blk_idx].pSrcAddr = (uint8_t *)frag_info->cursor.cursor;
                    blkSettings[blk_idx].pDestAddr = out;
                    blkSettings[blk_idx].blockSize = (num_enc + num_clear - decrypt_offset);
                    blkSettings[blk_idx].resetCrypto = blk_idx ? false : true;
                    blkSettings[blk_idx].scatterGatherCryptoStart = blk_idx ? false : true;
                    blkSettings[blk_idx].scatterGatherCryptoEnd = true;
                    blk_idx++;

                    for (k = 0; k < blk_idx; k++ ) {
                        NEXUS_FlushCache(blkSettings[k].pSrcAddr, blkSettings[k].blockSize);
                        blkSettings[k].cached = false; /* Prevent the DMA from flushing the buffers later on */
                    }

                    CommonCrypto_GetDefaultJobSettings(&cryptoJobSettings);
                    CommonCrypto_DmaXfer(commonCryptoHandle,  &cryptoJobSettings, blkSettings, blk_idx);

                    for (k = 0; k < blk_idx; k++ ) {
                        NEXUS_FlushCache(blkSettings[k].pSrcAddr, blkSettings[k].blockSize);
                    }

                    outSize += (num_enc + num_clear - decrypt_offset);

                    /* Skip over remaining clear units */
                    out += (num_clear - decrypt_offset);
                    batom_cursor_skip((batom_cursor *)&frag_info->cursor, (num_clear - decrypt_offset));

                    aesCtrInfo.qwBlockOffset = qwOffset / 16 ;
                    aesCtrInfo.bByteOffset = qwOffset % 16 ;

                    if(DRM_Prdy_Reader_Decrypt(
                                &app->decryptor,
                                &aesCtrInfo,
                                (uint8_t *)out,
                                num_enc ) != DRM_Prdy_ok) {
                        LOGE(("%s Reader_Decrypt failed - %d", __FUNCTION__, __LINE__));
                        return -1;
                    }

                    out += num_enc;
                    batom_cursor_skip((batom_cursor *)&frag_info->cursor,num_enc);
                    qwOffset = num_enc;
                    numOfByteDecrypted  += (num_clear - decrypt_offset + num_enc);

                    if(numOfByteDecrypted > sampleSize) {
                        LOGW(("Wrong buffer size is detected while decrypting the ciphertext, bytes processed %d, sample size to decrypt %d",
                                    numOfByteDecrypted, sampleSize));
                        return -1;
                    }
                }
            } else {
                piff_playback_dma_buffer(commonCryptoHandle, out, (uint8_t *)frag_info->cursor.cursor,
                        sampleSize, true);
                outSize += sampleSize;

                if(DRM_Prdy_Reader_Decrypt(
                            &app->decryptor,
                            &aesCtrInfo,
                            (uint8_t *)out,
                            sampleSize ) != DRM_Prdy_ok) {
                    LOGE(("%s Reader_Decrypt failed - %d", __FUNCTION__, __LINE__));
                    return -1;
                }

                batom_cursor_skip((batom_cursor *)&frag_info->cursor, sampleSize);
                numOfByteDecrypted = sampleSize;
            }

            bytes_processed += numOfByteDecrypted + decrypt_offset;
        }
        NEXUS_Playpump_WriteComplete(playpump, 0, outSize);
    }

    if(bytes_processed != payload_size) {
        LOGW(("%s the number of bytes %d decrypted doesn't match the actual size %d of the payload, return failure...%d",__FUNCTION__,
                    bytes_processed, payload_size, __LINE__));
        rc = -1;
    }

    return rc;
}

static int process_fragment(CommonCryptoHandle commonCryptoHandle, app_ctx *app, piff_parse_frag_info *frag_info,
        size_t payload_size, void *decoder_data, size_t decoder_len)
{
    int rc = 0;
    uint32_t i, bytes_processed;
    bpiff_drm_mp4_box_se_sample *pSample;
    uint8_t pes_header[BMEDIA_PES_HEADER_MAX_SIZE];
    size_t pes_header_len;
    bmedia_pes_info pes_info;
    uint64_t frag_duration;
    uint8_t *pOutBuf = app->pOutBuf;
    size_t decrypt_offset = 0;

    app->outBufSize = 0;
    bytes_processed = 0;
    if (frag_info->samples_enc->sample_count != 0) {
        for (i = 0; i < frag_info->samples_enc->sample_count; i++) {
            size_t numOfByteDecrypted = 0;
            size_t sampleSize = 0;

            pSample = &frag_info->samples_enc->samples[i];
            sampleSize = frag_info->sample_info[i].size;

            if (frag_info->trackType == BMP4_SAMPLE_ENCRYPTED_VIDEO) {
                if (!vc1_stream) {
                    /* H.264 Decoder configuration parsing */
                    uint8_t avcc_hdr[BPIFF_MAX_PPS_SPS];
                    size_t avcc_hdr_size;
                    size_t nalu_len = 0;

                    parse_avcc_config(avcc_hdr, &avcc_hdr_size, &nalu_len, decoder_data, decoder_len);

                    bmedia_pes_info_init(&pes_info, REPACK_VIDEO_PES_ID);
                    frag_duration = app->last_video_fragment_time +
                        frag_info->sample_info[i].composition_time_offset;
                    app->last_video_fragment_time += frag_info->sample_info[i].duration;

                    pes_info.pts_valid = true;
                    pes_info.pts = CALCULATE_PTS(frag_duration);
                    if (video_decode_hdr == 0) {
                        pes_header_len = bmedia_pes_header_init(pes_header,
                                (sampleSize + avcc_hdr_size - nalu_len + sizeof(bpiff_nal)), &pes_info);
                    } else {
                        pes_header_len = bmedia_pes_header_init(pes_header,
                                (sampleSize - nalu_len + sizeof(bpiff_nal)), &pes_info);
                    }

                    BKNI_Memcpy(pOutBuf, &pes_header, pes_header_len);
                    pOutBuf += pes_header_len;
                    app->outBufSize += pes_header_len;

                    if (video_decode_hdr == 0) {
                        BKNI_Memcpy(pOutBuf, avcc_hdr, avcc_hdr_size);
                        pOutBuf += avcc_hdr_size;
                        app->outBufSize += avcc_hdr_size;
                        video_decode_hdr = 1;
                    }

                    decrypt_offset = nalu_len;
                }
            } else if (frag_info->trackType == BMP4_SAMPLE_ENCRYPTED_AUDIO) {
                if (!vc1_stream) {
                    /* AAC information parsing */
                    bmedia_adts_hdr hdr;
                    bmedia_info_aac *info_aac = (bmedia_info_aac *)decoder_data;

                    parse_esds_config(&hdr, info_aac, sampleSize);

                    bmedia_pes_info_init(&pes_info, REPACK_AUDIO_PES_ID);
                    frag_duration = app->last_audio_fragment_time +
                        frag_info->sample_info[i].composition_time_offset;
                    app->last_audio_fragment_time += frag_info->sample_info[i].duration;

                    pes_info.pts_valid = true;
                    pes_info.pts = CALCULATE_PTS(frag_duration);

                    pes_header_len = bmedia_pes_header_init(pes_header,
                            (sampleSize + BMEDIA_ADTS_HEADER_SIZE), &pes_info);
                    BKNI_Memcpy(pOutBuf, &pes_header, pes_header_len);
                    BKNI_Memcpy(pOutBuf + pes_header_len, &hdr.adts, BMEDIA_ADTS_HEADER_SIZE);

                    pOutBuf += pes_header_len + BMEDIA_ADTS_HEADER_SIZE;
                    app->outBufSize += pes_header_len + BMEDIA_ADTS_HEADER_SIZE;

                    decrypt_offset = 0;
                }
            } else {
                LOGW(("%s Unsupported track type %d detected", __FUNCTION__, frag_info->trackType));
                return -1;
            }

            if(decrypt_sample(commonCryptoHandle, sampleSize, &frag_info->cursor, pSample, &numOfByteDecrypted,
                        &app->decryptor, frag_info->samples_enc->flags, pOutBuf, decrypt_offset) !=0) {
                LOGE(("%s Failed to decrypt sample, can't continue - %d", __FUNCTION__, __LINE__));
                return -1;
                break;
            }
            pOutBuf += numOfByteDecrypted;
            app->outBufSize += numOfByteDecrypted;
            bytes_processed += numOfByteDecrypted;
        }
    }

    if( bytes_processed != payload_size) {
        LOGW(("%s the number of bytes %d decrypted doesn't match the actual size %d of the payload, return failure...%d",__FUNCTION__,bytes_processed,payload_size, __LINE__));
        rc = -1;
    }

    return rc;
}

static int send_fragment_data(
        CommonCryptoHandle commonCryptoHandle,
        uint8_t *pData,
        uint32_t dataSize,
        NEXUS_PlaypumpHandle playpump,
        BKNI_EventHandle event)
{
    NEXUS_PlaypumpStatus playpumpStatus;
    size_t fragment_size = dataSize;
    void *playpumpBuffer;
    size_t bufferSize;
    NEXUS_Playpump_GetStatus(playpump, &playpumpStatus);
    fragment_size += 512;
    BDBG_ASSERT(fragment_size <= playpumpStatus.fifoSize);
    for(;;) {
        NEXUS_Playpump_GetBuffer(playpump, &playpumpBuffer, &bufferSize);
        if(bufferSize >= fragment_size) {
            break;
        }
        if(bufferSize==0) {
            BKNI_WaitForEvent(event, 100);
            continue;
        }
        if((uint8_t *)playpumpBuffer >= (uint8_t *)playpumpStatus.bufferBase +
                (playpumpStatus.fifoSize - fragment_size)) {
            NEXUS_Playpump_WriteComplete(playpump, bufferSize, 0); /* skip buffer what wouldn't be big enough */
        }
    }
    piff_playback_dma_buffer(commonCryptoHandle, playpumpBuffer, pData, dataSize, true);
    NEXUS_Playpump_WriteComplete(playpump, 0, dataSize);

    return 0;
}

static void play_callback(void *context, int param)
{
    BSTD_UNUSED(param);
    BKNI_SetEvent((BKNI_EventHandle)context);
}

static void
wait_for_drain(NEXUS_PlaypumpHandle videoPlaypump, NEXUS_SimpleVideoDecoderHandle videoDecoder,
               NEXUS_PlaypumpHandle audioPlaypump, NEXUS_SimpleAudioDecoderHandle audioDecoder)
{
    NEXUS_Error rc;
    NEXUS_PlaypumpStatus playpumpStatus;

    for(;;) {
        rc=NEXUS_Playpump_GetStatus(videoPlaypump, &playpumpStatus);
        if(rc!=NEXUS_SUCCESS)
            break;

        if(playpumpStatus.fifoDepth==0) {
            break;
        }
        BKNI_Sleep(100);
    }
    for(;;) {
        rc=NEXUS_Playpump_GetStatus(audioPlaypump, &playpumpStatus);
        if(rc!=NEXUS_SUCCESS)
            break;

        if(playpumpStatus.fifoDepth==0)
            break;

        BKNI_Sleep(100);
    }

    if(videoDecoder) {
        for(;;) {
            NEXUS_VideoDecoderStatus decoderStatus;
            rc=NEXUS_SimpleVideoDecoder_GetStatus(videoDecoder, &decoderStatus);
            if(rc!=NEXUS_SUCCESS)
                break;

            if(decoderStatus.queueDepth==0)
                break;

            BKNI_Sleep(100);
        }
    }
    if(audioDecoder) {
        for(;;) {
            NEXUS_AudioDecoderStatus decoderStatus;
            rc=NEXUS_SimpleAudioDecoder_GetStatus(audioDecoder, &decoderStatus);
            if(rc!=NEXUS_SUCCESS)
                break;

            if(decoderStatus.queuedFrames < 4)
                break;

            BKNI_Sleep(100);
        }
    }
    return;
}

static int complete_play_fragments(
        NEXUS_SimpleAudioDecoderHandle audioDecoder,
        NEXUS_SimpleVideoDecoderHandle videoDecoder,
        NEXUS_PlaypumpHandle videoPlaypump,
        NEXUS_PlaypumpHandle audioPlaypump,
        NEXUS_DisplayHandle display,
        NEXUS_PidChannelHandle audioPidChannel,
        NEXUS_PidChannelHandle videoPidChannel,
        NEXUS_VideoWindowHandle window,
        BKNI_EventHandle event)
{
    BSTD_UNUSED(window);
    BSTD_UNUSED(display);
    BSTD_UNUSED(event);
    wait_for_drain(videoPlaypump, videoDecoder, audioPlaypump, audioDecoder);

    if (event != NULL) BKNI_DestroyEvent(event);

    if (videoDecoder) {
        NEXUS_SimpleVideoDecoder_Stop(videoDecoder);
        NEXUS_Playpump_ClosePidChannel(videoPlaypump, videoPidChannel);
        NEXUS_Playpump_Stop(videoPlaypump);
        NEXUS_StopCallbacks(videoPlaypump);
    }
    if (audioDecoder) {
        NEXUS_SimpleAudioDecoder_Stop(audioDecoder);
        NEXUS_Playpump_ClosePidChannel(audioPlaypump, audioPidChannel);
        NEXUS_Playpump_Stop(audioPlaypump);
        NEXUS_StopCallbacks(audioPlaypump);
    }

    NEXUS_Playpump_Close(videoPlaypump);
    NEXUS_Playpump_Close(audioPlaypump);

    return 0;
}

void playback_piff( NEXUS_SimpleVideoDecoderHandle videoDecoder,
                    NEXUS_SimpleAudioDecoderHandle audioDecoder,
                    DRM_Prdy_Handle_t drm_context,
                    char *piff_file)
{
#if USE_SECURE_VIDEO_PLAYBACK
    uint8_t *pSecureVideoHeapBuffer = NULL;
#endif
#if USE_SECURE_AUDIO_PLAYBACK
    uint8_t *pSecureAudioHeapBuffer = NULL;
#endif
    NEXUS_ClientConfiguration clientConfig;
    NEXUS_MemoryAllocationSettings memSettings;
    NEXUS_SimpleStcChannelHandle stcChannel;
    NEXUS_SimpleVideoDecoderStartSettings videoProgram;
    NEXUS_SimpleAudioDecoderStartSettings audioProgram;
    NEXUS_PlaypumpOpenPidChannelSettings videoPidSettings;
    NEXUS_PlaypumpOpenPidChannelSettings audioPidSettings;
    NEXUS_PlaypumpOpenSettings videoplaypumpOpenSettings;
    NEXUS_PlaypumpOpenSettings audioplaypumpOpenSettings;
    NEXUS_SimpleStcChannelSettings stcSettings;
    NEXUS_PlaypumpSettings playpumpSettings;
    NEXUS_Error rc;
    CommonCryptoHandle commonCryptoHandle = NULL;
    CommonCryptoSettings  cmnCryptoSettings;

    NEXUS_DisplayHandle display;
    NEXUS_PidChannelHandle videoPidChannel = NULL;
    BKNI_EventHandle event = NULL;
    NEXUS_PlaypumpHandle videoPlaypump = NULL;
    NEXUS_PlaypumpHandle audioPlaypump = NULL;

    NEXUS_PidChannelHandle audioPidChannel = NULL;

    app_ctx app;
    bool moovBoxParsed = false;

    uint8_t resp_buffer[64*1024];
    char *pCh_url = NULL;
    char *pCh_data = NULL;
    uint8_t *pResponse = resp_buffer;
    size_t respLen;
    size_t respOffset;
    size_t urlLen;
    size_t chLen;
    piff_parser_handle_t piff_handle;
    bfile_io_read_t fd;
    uint8_t *pssh_data;
    uint32_t pssh_len;

    LOGV(("%s - %d\n", __FUNCTION__, __LINE__));
    if(piff_file == NULL ) {
        goto clean_exit;
    }

    LOGV(("PIFF file: %s\n",piff_file));
    fflush(stdout);

    BKNI_Memset(&app, 0, sizeof( app_ctx));
    app.last_video_fragment_time = 0;
    app.last_audio_fragment_time = 0;

    app.fp_piff = fopen(piff_file, "rb");
    if(app.fp_piff == NULL){
        fprintf(stderr,"failed to open %s\n", piff_file);
        goto clean_exit;
    }

    fd = bfile_stdio_read_attach(app.fp_piff);

    piff_handle = piff_parser_create(fd);
    if (!piff_handle) {
        LOGE(("Unable to create PIFF parser context"));
        goto clean_exit;
    }

    fseek(app.fp_piff, 0, SEEK_END);
    app.piff_filesize = ftell(app.fp_piff);
    fseek(app.fp_piff, 0, SEEK_SET);

    if( drm_context == NULL)
    {
       LOGE(("drm_context is NULL, quitting...."));
       goto clean_exit ;
    }

    CommonCrypto_GetDefaultSettings(&cmnCryptoSettings);
    commonCryptoHandle = CommonCrypto_Open(&cmnCryptoSettings);

    NEXUS_Platform_GetClientConfiguration(&clientConfig);
    NEXUS_Memory_GetDefaultAllocationSettings(&memSettings);
    memSettings.heap = clientConfig.heap[1]; /* heap 1 is the eFull heap for the nxclient. */

    app.pPayload = NULL;
    if( NEXUS_Memory_Allocate(BUF_SIZE, &memSettings, (void *)&app.pPayload) !=  NEXUS_SUCCESS) {
        fprintf(stderr,"NEXUS_Memory_Allocate failed");
        goto clean_up;
    }

    app.pOutBuf = NULL;
    if( NEXUS_Memory_Allocate(BUF_SIZE, &memSettings, (void *)&app.pOutBuf) !=  NEXUS_SUCCESS) {
        fprintf(stderr,"NEXUS_Memory_Allocate failed");
        goto clean_up;
    }

    /* Perform parsing of the movie information */
    moovBoxParsed = piff_parser_scan_movie_info(piff_handle);

    if(!moovBoxParsed) {
        LOGE(("Failed to parse moov box, can't continue..."));
        goto clean_up;
    }

    LOGV(("Successfully parsed the moov box, continue...\n\n"));

    /* EXTRACT AND PLAYBACK THE MDAT */

    NEXUS_Playpump_GetDefaultOpenSettings(&videoplaypumpOpenSettings);
    videoplaypumpOpenSettings.fifoSize *= 7;
    videoplaypumpOpenSettings.numDescriptors *= 7;
#if USE_SECURE_VIDEO_PLAYBACK
    videoplaypumpOpenSettings.dataNotCpuAccessible = true;
    pSecureVideoHeapBuffer = SRAI_Memory_Allocate(videoplaypumpOpenSettings.fifoSize,
            SRAI_MemoryType_SagePrivate);
    if ( pSecureVideoHeapBuffer == NULL ) {
        LOGE((" Failed to allocate from Secure Video heap"));
        BDBG_ASSERT( false );
    }
    videoplaypumpOpenSettings.memory = NEXUS_MemoryBlock_FromAddress(pSecureVideoHeapBuffer);
#else
    videoplaypumpOpenSettings.heap = clientConfig.heap[1];
    videoplaypumpOpenSettings.boundsHeap = clientConfig.heap[1];
#endif

    videoPlaypump = NEXUS_Playpump_Open(NEXUS_ANY_ID, &videoplaypumpOpenSettings);
    if (!videoPlaypump) {
        LOGE(("@@@ Video Playpump Open FAILED----"));
        goto clean_up;
    }
    BDBG_ASSERT(videoPlaypump != NULL);

#if USE_SECURE_AUDIO_PLAYBACK
    NEXUS_Playpump_GetDefaultOpenSettings(&audioplaypumpOpenSettings);
    audioplaypumpOpenSettings.dataNotCpuAccessible = true;
    pSecureAudioHeapBuffer = SRAI_Memory_Allocate(audioplaypumpOpenSettings.fifoSize,
            SRAI_MemoryType_SagePrivate);
    if ( pSecureAudioHeapBuffer == NULL ) {
        LOGE((" Failed to allocate from Secure Audio heap"));
        goto clean_up;
    }
    BDBG_ASSERT( pSecureAudioHeapBuffer != NULL );
    audioplaypumpOpenSettings.memory = NEXUS_MemoryBlock_FromAddress(pSecureAudioHeapBuffer);
#else
    NEXUS_Playpump_GetDefaultOpenSettings(&audioplaypumpOpenSettings);
    audioplaypumpOpenSettings.heap = clientConfig.heap[1];
    audioplaypumpOpenSettings.boundsHeap = clientConfig.heap[1];
#endif
    audioPlaypump = NEXUS_Playpump_Open(NEXUS_ANY_ID, &audioplaypumpOpenSettings);
    if (!audioPlaypump) {
        LOGE(("@@@ Video Playpump Open FAILED----"));
        goto clean_up;
    }
    BDBG_ASSERT(audioPlaypump != NULL);

    stcChannel = NEXUS_SimpleStcChannel_Create(NULL);
    NEXUS_SimpleStcChannel_GetSettings(stcChannel, &stcSettings);
    stcSettings.mode = NEXUS_StcChannelMode_eAuto;
    rc = NEXUS_SimpleStcChannel_SetSettings(stcChannel, &stcSettings);
    if (rc) {
       LOGW(("@@@ Stc Set FAILED ---------------"));
    }

    BKNI_CreateEvent(&event);

    NEXUS_Playpump_GetSettings(videoPlaypump, &playpumpSettings);
    playpumpSettings.dataCallback.callback = play_callback;
    playpumpSettings.dataCallback.context = event;
    playpumpSettings.transportType = NEXUS_TransportType_eMpeg2Pes;
    NEXUS_Playpump_SetSettings(videoPlaypump, &playpumpSettings);

    NEXUS_Playpump_GetSettings(audioPlaypump, &playpumpSettings);
    playpumpSettings.dataCallback.callback = play_callback;
    playpumpSettings.dataCallback.context = event;
    playpumpSettings.transportType = NEXUS_TransportType_eMpeg2Pes;
    NEXUS_Playpump_SetSettings(audioPlaypump, &playpumpSettings);

    /* already connected in main */
    NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&audioProgram);
    NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&videoProgram);

    NEXUS_Playpump_Start(videoPlaypump);
    NEXUS_Playpump_Start(audioPlaypump);

    NEXUS_PlaypumpOpenPidChannelSettings video_pid_settings;
    NEXUS_Playpump_GetDefaultOpenPidChannelSettings(&video_pid_settings);
    video_pid_settings.pidType = NEXUS_PidType_eVideo;

    videoPidChannel = NEXUS_Playpump_OpenPidChannel(videoPlaypump, REPACK_VIDEO_PES_ID, &video_pid_settings);
#if USE_SECURE_VIDEO_PLAYBACK
    NEXUS_SetPidChannelBypassKeyslot(videoPidChannel, NEXUS_BypassKeySlot_eGR2R);
#endif
    if ( !videoPidChannel )
      LOGW(("@@@ videoPidChannel NULL"));
    else
      LOGW(("@@@ videoPidChannel OK"));

    audioPidChannel = NEXUS_Playpump_OpenPidChannel(audioPlaypump, REPACK_AUDIO_PES_ID, NULL);
#if USE_SECURE_AUDIO_PLAYBACK
    NEXUS_SetPidChannelBypassKeyslot(audioPidChannel, NEXUS_BypassKeySlot_eGR2R);
#endif

    if ( !audioPidChannel )
      LOGW(("@@@ audioPidChannel NULL"));
    else
      LOGW(("@@@ audioPidChannel OK"));

    NEXUS_SimpleAudioDecoder_GetDefaultStartSettings(&audioProgram);
    NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(&videoProgram);

    if ( vc1_stream ) {
       LOGV(("@@@ set video audio program for vc1"));
       videoProgram.settings.codec = NEXUS_VideoCodec_eVc1;
       audioProgram.primary.codec = NEXUS_AudioCodec_eWmaPro;
    } else {
       LOGV(("@@@ set video audio program for h264"));
       videoProgram.settings.codec = NEXUS_VideoCodec_eH264;
       audioProgram.primary.codec = NEXUS_AudioCodec_eAacAdts;
    }

    videoProgram.settings.pidChannel = videoPidChannel;
    NEXUS_SimpleVideoDecoder_Start(videoDecoder, &videoProgram);

    audioProgram.primary.pidChannel = audioPidChannel;
    NEXUS_SimpleAudioDecoder_Start(audioDecoder, &audioProgram);

    if (videoProgram.settings.pidChannel) {
        LOGW(("@@@ set stc channel video"));
        NEXUS_SimpleVideoDecoder_SetStcChannel(videoDecoder, stcChannel);
    }

    if (audioProgram.primary.pidChannel) {
        LOGW(("@@@ set stc channel audio"));
        NEXUS_SimpleAudioDecoder_SetStcChannel(audioDecoder, stcChannel);
    }

    /***********************
     * now ready to decrypt
     ***********************/
    pssh_data = piff_parser_get_pssh(piff_handle, &pssh_len);
    if (!pssh_data) {
        LOGE(("Failed to obtain pssh data"));
        goto clean_exit;
    }

    if( DRM_Prdy_Content_SetProperty(drm_context,
                DRM_Prdy_contentSetProperty_eAutoDetectHeader,
                pssh_data, pssh_len) != DRM_Prdy_ok) {
        LOGE(("Failed to SetProperty for the KID, exiting..."));
        goto clean_exit;
    }

    if( DRM_Prdy_Get_Buffer_Size( drm_context, DRM_Prdy_getBuffer_licenseAcq_challenge,
                NULL, 0, &urlLen, &chLen) != DRM_Prdy_ok ) {
        LOGE(("DRM_Prdy_Get_Buffer_Size() failed, exiting"));
        goto clean_exit;
    }

    pCh_url = BKNI_Malloc(urlLen);

    if(pCh_url == NULL) {
        LOGE(("BKNI_Malloc(urlent) failed, exiting..."));
        goto clean_exit;
    }

    pCh_data = BKNI_Malloc(chLen);
    if(pCh_data == NULL) {
        LOGE(("BKNI_Malloc(chLen) failed, exiting..."));
        goto clean_exit;
    }

    if(DRM_Prdy_LicenseAcq_GenerateChallenge(drm_context, NULL,
                0, pCh_url, &urlLen, pCh_data, &chLen) != DRM_Prdy_ok ) {
        LOGE(("DRM_Prdy_License_GenerateChallenge() failed, exiting"));
        goto clean_exit;
    }

    pCh_data[ chLen ] = 0;

    if(DRM_Prdy_http_client_license_post_soap(pCh_url, pCh_data, 1,
        150, (unsigned char **)&pResponse, &respOffset, &respLen) != 0) {
        LOGE(("DRM_Prdy_http_client_license_post_soap() failed, exiting"));
        goto clean_exit;
    }

    if( DRM_Prdy_LicenseAcq_ProcessResponse(drm_context, (char *)&pResponse[respOffset],
                respLen, NULL) != DRM_Prdy_ok ) {
        LOGE(("DRM_Prdy_LicenseAcq_ProcessResponse() failed, exiting"));
        goto clean_exit;
    }

    if( DRM_Prdy_Reader_Bind( drm_context,
                &app.decryptor)!= DRM_Prdy_ok ) {
        LOGE(("Failed to Bind the license, the license may not exist. Exiting..."));
        goto clean_exit;
    }

    if( DRM_Prdy_Reader_Commit(drm_context) != DRM_Prdy_ok ) {
        LOGE(("Failed to Commit the license after Reader_Bind, exiting..."));
        goto clean_exit;
    }

    /* now go back to the begining and get all the moof boxes */
    fseek(app.fp_piff, 0, SEEK_END);
    app.piff_filesize = ftell(app.fp_piff);
    fseek(app.fp_piff, 0, SEEK_SET);

    video_decode_hdr = 0;

    /* Start parsing the the file to look for MOOFs and MDATs */
    while(!feof(app.fp_piff)) {
        piff_parse_frag_info frag_info;
        void *decoder_data;
        size_t decoder_len;

        if (!piff_parser_scan_movie_fragment(piff_handle, &frag_info, app.pPayload, BUF_SIZE)) {
            if (feof(app.fp_piff)) {
                LOGW(("Reached EOF"));
                break;
            } else {
                LOGE(("Unable to parse movie fragment"));
                goto clean_up;
            }
        }
        decoder_data = piff_parser_get_dec_data(piff_handle, &decoder_len, frag_info.trackId);

        if (frag_info.trackType == BMP4_SAMPLE_ENCRYPTED_VIDEO) {
#if USE_SECURE_VIDEO_PLAYBACK
            secure_process_fragment(commonCryptoHandle, &app, &frag_info, (frag_info.mdat_size - BOX_HEADER_SIZE),
                    decoder_data, decoder_len, videoPlaypump, event);
#else
        if(process_fragment(commonCryptoHandle, &app, &frag_info, (frag_info.mdat_size - BOX_HEADER_SIZE),
                    decoder_data, decoder_len) == 0) {
#if DEBUG_OUTPUT_CAPTURE
                fwrite(app.pOutBuf, 1, app.outBufSize, fp_vid);
#endif
                send_fragment_data(commonCryptoHandle, app.pOutBuf, app.outBufSize,
                        videoPlaypump, event);
            }
#endif
            } else if (frag_info.trackType == BMP4_SAMPLE_ENCRYPTED_AUDIO) {
#if USE_SECURE_AUDIO_PLAYBACK
            secure_process_fragment(commonCryptoHandle, &app, &frag_info, (frag_info.mdat_size - BOX_HEADER_SIZE),
                    decoder_data, decoder_len, audioPlaypump, event);
#else
            if(process_fragment(commonCryptoHandle, &app, &frag_info, (frag_info.mdat_size - BOX_HEADER_SIZE),
                        decoder_data, decoder_len) == 0) {
#if DEBUG_OUTPUT_CAPTURE
                fwrite(app.pOutBuf, 1, app.outBufSize, fp_aud);
#endif
                send_fragment_data(commonCryptoHandle, app.pOutBuf, app.outBufSize,
                        audioPlaypump, event);
            }
#endif
        }
        }
    complete_play_fragments(audioDecoder, videoDecoder, videoPlaypump,
            audioPlaypump, display, audioPidChannel, videoPidChannel, NULL, event);

clean_up:
    if(commonCryptoHandle) CommonCrypto_Close(commonCryptoHandle);
#if USE_SECURE_VIDEO_PLAYBACK
    if(pSecureVideoHeapBuffer) SRAI_Memory_Free(pSecureVideoHeapBuffer);
#endif
#if USE_SECURE_AUDIO_PLAYBACK
    if(pSecureAudioHeapBuffer) SRAI_Memory_Free(pSecureAudioHeapBuffer);
#endif
    if(app.decryptor.pDecrypt != NULL) DRM_Prdy_Reader_Close( &app.decryptor);
    if(app.pPayload) NEXUS_Memory_Free(app.pPayload);
    if(app.pOutBuf) NEXUS_Memory_Free(app.pOutBuf);

    if(piff_handle != NULL) piff_parser_destroy(piff_handle);

    bfile_stdio_read_detach(fd);
    if(app.fp_piff) fclose(app.fp_piff);

    if(pCh_url != NULL) BKNI_Free(pCh_url);
    if(pCh_data != NULL) BKNI_Free(pCh_data);

clean_exit:
    return;
}

int main(int argc, char* argv[])
{
    NxClient_JoinSettings joinSettings;
    NxClient_AllocSettings allocSettings;
    NxClient_AllocResults allocResults;
    NEXUS_Error rc;

    NxClient_ConnectSettings connectSettings;
    unsigned connectId;
    NEXUS_SurfaceComposition comp;
    NEXUS_SurfaceClientHandle surfaceClient = NULL;
    NEXUS_SurfaceClientHandle videoSurfaceClient = NULL;
    NEXUS_SimpleStcChannelHandle stcChannel;
    NEXUS_SimpleVideoDecoderHandle videoDecoder = NULL;
    NEXUS_SimpleAudioDecoderHandle audioDecoder = NULL;
    BKNI_EventHandle event;

    /* DRM_Prdy specific */
    DRM_Prdy_Init_t     prdyParamSettings;
    DRM_Prdy_Handle_t   drm_context;

#if USE_BDBG_LOGGING
    BDBG_SetModuleLevel( BDBG_MODULE_NAME, BDBG_eMsg );
#endif

    if (argc < 2) {
        LOGE(("Usage : %s <input_file> [-vc1]", argv[0]));
        return 0;
    }

    if ((argc == 3) && (strcmp(argv[2], "-vc1") == 0))
        vc1_stream = 1;

    LOGV(("@@@ MSG Check Point Start vc1_stream %d--", vc1_stream));

#ifdef NEED_TO_BE_TRUSTED_APP
	char nx_key[PROPERTY_VALUE_MAX];
	FILE *key = NULL;

        sprintf(nx_key, "%s/nx_key", NEXUS_TRUSTED_DATA_PATH);
        key = fopen(nx_key, "r");

        if (key == NULL) {
           fprintf(stderr, "%s: failed to open key file \'%s\', err=%d (%s)\n", __FUNCTION__, nx_key, errno, strerror(errno));
        } else {
           memset(nx_key, 0, sizeof(nx_key));
           fread(nx_key, PROPERTY_VALUE_MAX, 1, key);
           fclose(key);
        }
#endif

    NxClient_GetDefaultJoinSettings(&joinSettings);
    snprintf(joinSettings.name, NXCLIENT_MAX_NAME, "pr_piff_playback");
    joinSettings.ignoreStandbyRequest = true;

#ifdef NEED_TO_BE_TRUSTED_APP
        joinSettings.mode = NEXUS_ClientMode_eUntrusted;
        if (strlen(nx_key)) {
           if (strstr(nx_key, "trusted:") == nx_key) {
              const char *password = &nx_key[8];
              joinSettings.mode = NEXUS_ClientMode_eProtected;
              joinSettings.certificate.length = strlen(password);
              memcpy(joinSettings.certificate.data, password, joinSettings.certificate.length);
           }
        }
#endif

    rc = NxClient_Join(&joinSettings);
    if (rc)
        return -1;

    NxClient_GetDefaultAllocSettings(&allocSettings);
    allocSettings.simpleVideoDecoder = 1;
    allocSettings.simpleAudioDecoder = 1;
    allocSettings.surfaceClient = 1;
    rc = NxClient_Alloc(&allocSettings, &allocResults);
    if (rc)
        return BERR_TRACE(rc);

    DRM_Prdy_GetDefaultParamSettings(&prdyParamSettings);

    drm_context =  DRM_Prdy_Initialize( &prdyParamSettings);
    if( drm_context == NULL) {
       LOGE(("Failed to create drm_context, quitting...."));
       goto clean_exit ;
    }

#if DEBUG_OUTPUT_CAPTURE
    fp_vid = fopen ("/video.out", "wb");
    fp_aud = fopen ("/audio.out", "wb");
#endif
    LOGV(("@@@ Check Point #01"));

    if (allocResults.simpleVideoDecoder[0].id) {
        LOGV(("@@@ to acquire video decoder"));
        videoDecoder = NEXUS_SimpleVideoDecoder_Acquire(allocResults.simpleVideoDecoder[0].id);
    }
    BDBG_ASSERT(videoDecoder);
    if (allocResults.simpleAudioDecoder.id) {
        LOGV(("@@@ to acquire audio decoder"));
        audioDecoder = NEXUS_SimpleAudioDecoder_Acquire(allocResults.simpleAudioDecoder.id);
    }
    BDBG_ASSERT(audioDecoder);
    if (allocResults.surfaceClient[0].id) {
        LOGV(("@@@ to acquire surfaceclient"));
        /* surfaceClient is the top-level graphics window in which video will fit.
        videoSurfaceClient must be "acquired" to associate the video window with surface compositor.
        Graphics do not have to be submitted to surfaceClient for video to work, but at least an
        "alpha hole" surface must be submitted to punch video through other client's graphics.
        Also, the top-level surfaceClient ID must be submitted to NxClient_ConnectSettings below. */
        surfaceClient = NEXUS_SurfaceClient_Acquire(allocResults.surfaceClient[0].id);
        videoSurfaceClient = NEXUS_SurfaceClient_AcquireVideoWindow(surfaceClient, 0);

        NxClient_GetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
        comp.zorder = ZORDER_TOP;   /* try to stay on top most */
        NxClient_SetSurfaceClientComposition(allocResults.surfaceClient[0].id, &comp);
    }
    NxClient_GetDefaultConnectSettings(&connectSettings);
    connectSettings.simpleVideoDecoder[0].id = allocResults.simpleVideoDecoder[0].id;
    connectSettings.simpleVideoDecoder[0].surfaceClientId = allocResults.surfaceClient[0].id;
    connectSettings.simpleAudioDecoder.id = allocResults.simpleAudioDecoder.id;
    rc = NxClient_Connect(&connectSettings, &connectId);
    if (rc) return BERR_TRACE(rc);

    gui_init( surfaceClient );

    playback_piff(videoDecoder,
                  audioDecoder,
                  drm_context,
                  argv[1]);

#if DEBUG_OUTPUT_CAPTURE
    fclose (fp_vid);
    fclose (fp_aud);
#endif

    if (videoDecoder != NULL) {
        NEXUS_SimpleVideoDecoder_Release( videoDecoder );
    }

    if ( audioDecoder != NULL) {
        NEXUS_SimpleAudioDecoder_Release( audioDecoder );
    }

    if ( surfaceClient != NULL ) {
        NEXUS_SurfaceClient_Release( surfaceClient );
    }

    DRM_Prdy_Uninitialize(drm_context);

    NxClient_Disconnect(connectId);
    NxClient_Free(&allocResults);
    NxClient_Uninit();

clean_exit:

    return 0;
}

static int gui_init( NEXUS_SurfaceClientHandle surfaceClient )
{
    NEXUS_Graphics2DHandle gfx;
    NEXUS_SurfaceHandle surface;

    NEXUS_Graphics2DSettings gfxSettings;
    NEXUS_SurfaceCreateSettings createSettings;
    NEXUS_Graphics2DFillSettings fillSettings;
    int rc;

    if (!surfaceClient) return -1;

    LOGV(("@@@ gui_init surfaceclient %d", (int)surfaceClient));
    gfx = NEXUS_Graphics2D_Open(NEXUS_ANY_ID, NULL);
    NEXUS_Graphics2D_GetSettings(gfx, &gfxSettings);
    rc = NEXUS_Graphics2D_SetSettings(gfx, &gfxSettings);
    BDBG_ASSERT(!rc);

    NEXUS_Surface_GetDefaultCreateSettings(&createSettings);
    createSettings.pixelFormat = NEXUS_PixelFormat_eA8_R8_G8_B8;
    createSettings.width = 720;
    createSettings.height = 480;
    surface = NEXUS_Surface_Create(&createSettings);

    NEXUS_Graphics2D_GetDefaultFillSettings(&fillSettings);
    fillSettings.surface = surface;
    fillSettings.color = 0;
    rc = NEXUS_Graphics2D_Fill(gfx, &fillSettings);
    BDBG_ASSERT(!rc);

    rc = NEXUS_Graphics2D_Checkpoint(gfx, NULL); /* require to execute queue */

    rc = NEXUS_SurfaceClient_SetSurface(surfaceClient, surface);
    BDBG_ASSERT(!rc);

    return 0;
}

