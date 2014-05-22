/******************************************************************************
 *    (c)2011 Broadcom Corporation
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
 * $brcm_Workfile: nexus_interface.h $
 * $brcm_Revision: 3 $
 * $brcm_Date: 9/19/11 5:23p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/thirdparty/google/honeycomb/src/broadcom/honeycomb/vendor/broadcom/bcm_platform/libnexusservice/nexus_interface.h $
 * 
 * 3   9/19/11 5:23p fzhang
 * SW7425-1307: Add libaudio support on 7425 Honeycomb
 * 
 * 2   8/25/11 7:30p franktcc
 * SW7420-2020: Enable PIP/Dual Decode
 * 
 *****************************************************************************/
#ifndef _NEXUS_FRONTEND_INTERFACE_H_
#define _NEXUS_FRONTEND_INTERFACE_H_
#define NEXUS_FRONTEND_INTERFACE_NAME "com.broadcom.bse.nexusFrontendInterface"

#define android_DECLARE_META_FRONTENDINTERFACE(INTERFACE)                               \
static const android::String16 descriptor;                      \
static android::sp<I##INTERFACE> asInterface(const android::sp<android::IBinder>& obj); \
static android::String16 getInterfaceDescriptor();              \

#define android_IMPLEMENT_META_FRONTENDINTERFACE(INTERFACE, NAME)                       \
    const android::String16 I##INTERFACE::descriptor(NAME);         \
android::String16 I##INTERFACE::getInterfaceDescriptor() { \
    return I##INTERFACE::descriptor;                                \
}                                                                   \
android::sp<I##INTERFACE> I##INTERFACE::asInterface(const android::sp<android::IBinder>& obj) \
{                                                                   \
    android::sp<I##INTERFACE> intr;                             \
    if (obj != NULL) {                                              \
        intr = static_cast<I##INTERFACE*>(                          \
                obj->queryLocalInterface(                               \
                    I##INTERFACE::descriptor).get());               \
        if (intr == NULL) {                                         \
            intr = new Bp##INTERFACE(obj);                          \
        }                                                           \
    }                                                               \
    return intr;                                                    \
}                                                                   \

/* transact id summary*/

typedef enum {
    NEXUS_FRONTEND_TUNE_PARAMETER_SET=1,
    NEXUS_FRONTEND_TUNE_PARAMETER_GET,
    NEXUS_FRONTEND_TUNE_TUNE,
    NEXUS_FRONTEND_TUNE_STATUS_GET,
    NEXUS_DVB_SERVICE_SEARCH_SERVICE,
    NEXUS_DVB_SERVICE_GET_TOTALNUM,
    NEXUS_DVB_SERVICE_GET_BYNUM,
    NEXUS_DVB_SERVICE_RESET,
    NEXUS_DVB_SERVICE_PLAY,
    NEXUS_DVB_MESSAGE_OPEN,
    NEXUS_DVB_MESSAGE_GET_MESSAGE_DATA,
    NEXUS_DVB_MESSAGE_CLOSE
} NEXUS_FRONTEND_TRANSACT_ID;


typedef enum {
    ANDROID_NEXUS_VideoCodec_eUnknown = 0,     /* unknown/not supported video codec */
    ANDROID_NEXUS_VideoCodec_eNone = 0,        /* unknown/not supported video codec */
    ANDROID_NEXUS_VideoCodec_eMpeg1,           /* MPEG-1 Video (ISO/IEC 11172-2) */
    ANDROID_NEXUS_VideoCodec_eMpeg2,           /* MPEG-2 Video (ISO/IEC 13818-2) */
    ANDROID_NEXUS_VideoCodec_eMpeg4Part2,      /* MPEG-4 Part 2 Video */
    ANDROID_NEXUS_VideoCodec_eH263,            /* H.263 Video. The value of the enum is not based on PSI standards. */
    ANDROID_NEXUS_VideoCodec_eH264,            /* H.264 (ITU-T) or ISO/IEC 14496-10/MPEG-4 AVC */
    ANDROID_NEXUS_VideoCodec_eH264_Svc,        /* Scalable Video Codec extension of H.264 */
    ANDROID_NEXUS_VideoCodec_eH264_Mvc,        /* Multi View Coding extension of H.264 */
    ANDROID_NEXUS_VideoCodec_eVc1,             /* VC-1 Advanced Profile */
    ANDROID_NEXUS_VideoCodec_eVc1SimpleMain,   /* VC-1 Simple & Main Profile */
    ANDROID_NEXUS_VideoCodec_eDivx311,         /* DivX 3.11 coded video */
    ANDROID_NEXUS_VideoCodec_eAvs,             /* AVS coded video */
    ANDROID_NEXUS_VideoCodec_eRv40,            /* RV 4.0 coded video */
    ANDROID_NEXUS_VideoCodec_eVp6,             /* VP6 coded video */
    ANDROID_NEXUS_VideoCodec_eMax
} ANDROID_NEXUS_video_codec;

typedef enum {
    ANDROID_NEXUS_AudioCodec_eUnknown = 0,    /* unknown/not supported audio format */
    ANDROID_NEXUS_AudioCodec_eMpeg,           /* MPEG1/2, layer 1/2. This does not support layer 3 (mp3). */
    ANDROID_NEXUS_AudioCodec_eMp3,            /* MPEG1/2, layer 3. */
    ANDROID_NEXUS_AudioCodec_eAac,            /* Advanced audio coding. Part of MPEG-4 */
    ANDROID_NEXUS_AudioCodec_eAacAdts=ANDROID_NEXUS_AudioCodec_eAac,
    ANDROID_NEXUS_AudioCodec_eAacLoas,        /* Advanced audio coding. Part of MPEG-4 */
    ANDROID_NEXUS_AudioCodec_eAacPlus,        /* AAC plus SBR. aka MPEG-4 High Efficiency (AAC-HE) with ADTS (Audio Data Transport Format) */
    ANDROID_NEXUS_AudioCodec_eAacPlusLoas =ANDROID_NEXUS_AudioCodec_eAacPlus,    /* AAC plus SBR. aka MPEG-4 High Efficiency (AAC-HE), with LOAS (Low Overhead Audio Stream) sync and LATM mux */
    ANDROID_NEXUS_AudioCodec_eAacPlusAdts,    /* AAC plus SBR. aka MPEG-4 High Efficiency (AAC-HE), with ADTS (Audio Data Transport Format) sync and LATM mux */
    ANDROID_NEXUS_AudioCodec_eAc3,            /* Dolby Digital AC3 audio */
    ANDROID_NEXUS_AudioCodec_eAc3Plus,        /* Dolby Digital Plus (AC3+ or DDP) audio */
    ANDROID_NEXUS_AudioCodec_eDts,            /* Digital Digital Surround sound, uses non-legacy frame-sync */
    ANDROID_NEXUS_AudioCodec_eLpcmDvd,        /* LPCM, DVD mode */
    ANDROID_NEXUS_AudioCodec_eLpcmHdDvd,      /* LPCM, HD-DVD mode */
    ANDROID_NEXUS_AudioCodec_eLpcmBluRay,     /* LPCM, Blu-Ray mode */
    ANDROID_NEXUS_AudioCodec_eDtsHd,          /* Digital Digital Surround sound, HD, uses non-legacy frame-sync, decodes only DTS part of DTS-HD stream */
    ANDROID_NEXUS_AudioCodec_eWmaStd,         /* WMA Standard */
    ANDROID_NEXUS_AudioCodec_eWmaPro,         /* WMA Professional */
    ANDROID_NEXUS_AudioCodec_eAvs,            /* AVS */ 
    ANDROID_NEXUS_AudioCodec_ePcm,            /* PCM audio - Generally used only with inputs such as SPDIF or HDMI. */ 
    ANDROID_NEXUS_AudioCodec_ePcmWav,         /* PCM audio with Wave header - Used with streams containing PCM audio */
    ANDROID_NEXUS_AudioCodec_eAmr,            /* Adaptive Multi-Rate compression (typically used w/3GPP) */
    ANDROID_NEXUS_AudioCodec_eDra,            /* Dynamic Resolution Adaptation.  Used in Blu-Ray and China Broadcasts. */
    ANDROID_NEXUS_AudioCodec_eCook,           /* Real Audio 8 LBR */
    ANDROID_NEXUS_AudioCodec_eAdpcm,          /* MS ADPCM audio format */
    ANDROID_NEXUS_AudioCodec_eSbc,            /* Sub Band Codec used in Bluetooth A2DP audio */
    ANDROID_NEXUS_AudioCodec_eDtsLegacy,      /* Digital Digital Surround sound, legacy mode (14 bit), uses legacy frame-sync */
    ANDROID_NEXUS_AudioCodec_eMax
} ANDROID_NEXUS_audio_codec;


typedef struct Nexus_Channel_Service_info
{
    unsigned short a,v,p;
    ANDROID_NEXUS_video_codec v_codec;
    ANDROID_NEXUS_audio_codec a_codec;

    unsigned short EcmPid;
    unsigned short EmmPid;
    char name[200];
    /*  Service_info* nextService_info;*/
} Nexus_Channel_Service_info;


typedef struct Frontend_status 
{
    bool receiverLock;          /* Do we have QAM lock? */
    bool fecLock;               /* Is the FEC locked? */
    bool opllLock;              /* Is the output PLL locked? */
    bool spectrumInverted;      /* Is the spectrum inverted? */

    unsigned symbolRate;        /* Baud. received symbol rate (in-band) */
    int      symbolRateError;   /* symbol rate error in Baud */

    int berEstimate;            /* deprecated */

    unsigned ifAgcLevel;        /* IF AGC level in units of 1/10 percent */
    unsigned rfAgcLevel;        /* tuner AGC level in units of 1/10 percent */
    unsigned intAgcLevel;       /* Internal AGC level in units of 1/10 percent */
    unsigned snrEstimate;       /* snr estimate in 1/100 dB (in-Band). */

    unsigned fecCorrected;      /* FEC corrected block count, accumulated since tune or NEXUS_Frontent_ResetStatus */
    unsigned fecUncorrected;    /* FEC uncorrected block count, accumulated since tune or NEXUS_Frontent_ResetStatus */
    unsigned fecClean;          /* FEC clean block count, accumulated since tune or NEXUS_Frontent_ResetStatus */
    unsigned bitErrCorrected;   /* deprecated: cumulative bit correctable errors. same as viterbiUncorrectedBits. */
    unsigned reacquireCount;    /* cumulative reacquisition count */

    unsigned viterbiUncorrectedBits; /* uncorrected error bits output from Viterbi, accumulated since tune or NEXUS_Frontent_ResetStatus */
    unsigned viterbiTotalBits;       /* total number of bits output from Viterbi, accumulated since tune or NEXUS_Frontent_ResetStatus */
    unsigned int viterbiErrorRate;       /* viterbi bit error rate (BER) in 1/2147483648 th units.
                                            This is the ratio of uncorrected bits / total bits.
                                            Convert to floating point by dividing by 2147483648.0 */

    int      carrierFreqOffset; /* carrier frequency offset in 1/1000 Hz */
    int      carrierPhaseOffset;/* carrier phase offset in 1/1000 Hz */

    unsigned goodRsBlockCount;  /* reset on every read */
    unsigned berRawCount;       /* reset on every read */

    int      dsChannelPower;    /* OCAP DPM support for video channels, in 10's of dBmV units */
    unsigned mainTap;           /* Channel main tap coefficient */
    unsigned equalizerGain;     /* Channel equalizer gain value in dBm */
    unsigned postRsBer;         /* OCAP requires postRsBer for all DS channels. postRsBer will be reset on every channel change. Same units as berEstimate. */
    unsigned postRsBerElapsedTime; /* postRsBer over this time. In units of seconds. */
    unsigned short interleaveDepth;   /* Used in DOCSIS */
    unsigned lnaAgcLevel;        /* LNA AGC level in units of 1/10 percent */
}Frontend_status; 


typedef enum FrontendQam
{
    Qam8 = 8,
    Qam16 = 16,
    Qam32 = 32,
    Qam64 = 64,
    Qam128,
    Qam256,
    Qam1024,
    QamUnknown 
}FrontendQam;


typedef struct FrontendParam
{
    int freq;
    int symbolrate;
    FrontendQam qammode;
}FrontendParam;


typedef struct DvbmessageFilter
{
    unsigned char value[16];
    unsigned char mask[16];
    unsigned char excl[16];
}DvbmessageFilter;


typedef struct DVBMessageParam
{
    int pid;
    DvbmessageFilter filter;
}DVBMessageParam;


typedef struct ServiceFrontend{
    int32_t* frontend;
    int inputband;
    int nexusfrontendnum;
    int freq;
}ServiceFrontend;


class INexusFrontendClient: public android::IInterface {
    public:
        android_DECLARE_META_FRONTENDINTERFACE(NexusFrontendClient)

        /* virtual function define*/
        virtual void NexusFrontendParamterSet(void* pParam) = 0;
        virtual void NexusFrontendParamterGet(void* pParam) = 0;
        virtual int NexusFrontendTune(ServiceFrontend* frontend ) = 0;
        virtual void NexusFrontendStatusGet(int32_t* frontend, void* pStatus) = 0;
        virtual void NexusDVBServiceSearch(int inputband ) = 0;
        virtual int NexusDVBServiceGetTotalNum( ) = 0;
        virtual void NexusDVBServiceGetByNum(int num,void* pService ) = 0; 
        virtual void NexusDVBServiceReset( ) = 0; 
        virtual void NexusDVBServicePlay(unsigned int ecmpid,unsigned int emmpid,unsigned int videopid, unsigned int audiopid,unsigned int vpidchannel,unsigned int apidchannel) = 0;
        virtual int32_t* NexusDVBMessageOpen(int inputband,void* pMessageParam) = 0;
        virtual int NexusDVBMessageGetMessageData(int32_t* pMessageHandle,unsigned char *pData,int32_t iDataLength) = 0;
        virtual void NexusDVBMessageClose(int32_t* pMessageHandle) = 0;
};

#endif

