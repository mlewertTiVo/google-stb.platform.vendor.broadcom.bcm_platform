/******************************************************************************
 *    (c)2011-2013 Broadcom Corporation
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
 * $brcm_Workfile: AudioHardwareNEXUS.cpp $
 * $brcm_Revision: 15 $
 * $brcm_Date: 2/22/13 7:31p $
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/libaudio/AudioHardwareNEXUS.cpp $
 * 
 * 15   2/22/13 7:31p robertwm
 * SWANDROID-320:  Warm Boot from S3 in ICS Phase 1.2.
 * 
 * SWANDROID-320/1   2/22/13 5:43p robertwm
 * SWANDROID-320:  Warm Boot from S3 in ICS Phase 1.2.
 * 
 * 14   12/14/12 2:27p kagrawal
 * SWANDROID-277: Wrapper IPC APIs for
 *  NEXUS_SimpleXXX_Acquire()/_Release()
 * 
 * 13   12/3/12 3:20p saranya
 * SWANDROID-266: Removed Non-IPC Standalone Mode
 * 
 * 12   9/4/12 6:53p nitinb
 * SWANDROID-197:Implement volume control functionality on nexus server
 *  side
 * 
 * 11   8/8/12 2:34p fzhang
 * SWANDROID-170: Speech Recorder app is not working
 * 
 * 10   7/25/12 6:27p nitinb
 * SWANDROID-134: Added HDMI volume control to
 *  AudioStreamOutNEXUS::setVolume. Also fixed spdif volume control
 * 
 * 9   6/5/12 2:36p kagrawal
 * SWANDROID-108:Added support to use simple decoder APIs
 * 
 * 8   5/7/12 3:43p ajitabhp
 * SWANDROID-96: Initial checkin for android side by side implementation.
 * 
 * 7   3/27/12 4:06p mzhuang
 * SW7425-2633: audio mixer errors after audio flinger restart
 * 
 * 6   3/15/12 4:52p mzhuang
 * SW7425-2633: audio mixer errors after audio flinger restart
 * 
 * 5   2/24/12 4:08p kagrawal
 * SWANDROID-12: Dynamic client creation using IPC over binder
 * 
 * 4   2/8/12 5:21p kagrawal
 * SWANDROID-12: Initial support for nexus client-server
 * 
 * 4   2/8/12 4:54p kagrawal
 * SWANDROID-12: Initial support for nexus client-server
 * 
 * 3   12/14/11 10:10a franktcc
 * SW7425-1964: Avoid AudioTrack ocassionally obtain get buffer timeout
 * 
 * 2   12/9/11 4:08a alexpan
 * SW7425-1908: Initial Check-in for Android Ice Cream Sandwich
 * 
 * 3   9/19/11 5:23p fzhang
 * SW7425-1307: Add libaudio support on 7425 Honeycomb
 * 
 * 2   8/12/11 4:16p fzhang
 * SW7425-1090:Add Speech Recoder support in HoneyComb
 *****************************************************************************/

//#define LOG_NDEBUG 0
#include <stdint.h>
#include <sys/types.h>

#include <stdlib.h>
#include <unistd.h>
#include <utils/String8.h>


#include "AudioHardwareNEXUS.h"
#include <media/AudioRecord.h>
#include <media/mediarecorder.h>

#include <fcntl.h>
#include <linux/soundcard.h>
#include <errno.h>

#include "nexus_audio_mixer.h"
#include "nexus_audio_decoder.h"
#include "nexus_simple_audio_playback.h"

// Uncomment line below to enable very verbose logging...
//#define VERY_VERBOSE_LOGGING

#ifdef VERY_VERBOSE_LOGGING
#define LOGVV LOGV
#else
#define LOGVV(a...) do { } while(0)
#endif

#define AUDIO_VOLUME_SETTING_MIN (0)
#define AUDIO_VOLUME_SETTING_MAX (100)
  
/* TODO: only test with buildin mic (usb camera */
#define AUDIO_INPUT_BUILTIN_MIC "/dev/snd/dsp"

/****************************************************
* Volume Table in dB, Mapping as linear attenuation *
****************************************************/
static uint32_t Gemini_VolTable[101] =
{
    0,      9,      17,     26,     35,
    45,     54,     63,     72,     82,
    92,     101,    111,    121,    131,
    141,    151,    162,    172,    183,
    194,    205,    216,    227,    239,
    250,    262,    273,    285,    297,
    310,    322,    335,    348,    361,
    374,    388,    401,    415,    429,
    444,    458,    473,    488,    504,
    519,    535,    551,    568,    585,
    602,    620,    638,    656,    674,
    694,    713,    733,    754,    774,
    796,    818,    840,    864,    887,
    912,    937,    963,    990,    1017,
    1046,   1075,   1106,   1137,   1170,
    1204,   1240,   1277,   1315,   1356,
    1398,   1442,   1489,   1539,   1592,
    1648,   1708,   1772,   1842,   1917,
    2000,   2092,   2194,   2310,   2444,
    2602,   2796,   3046,   3398,   9001,
    13900
};



 namespace android_audio_legacy {
    
// ----------------------------------------------------------------------------

AudioHardware::AudioHardware() : mMicMute(false),mInited(false)
{
    LOGV("%s: at %d\n",__FUNCTION__,__LINE__);
    Mutex::Autolock lock(mLock);
    mAudioOutputs.clear();
    mAudioInputs.clear();
    mInited = true;
}

AudioHardware::~AudioHardware()
{
    LOGV("%s: at %d\n",__FUNCTION__,__LINE__);
    Mutex::Autolock lock(mLock);
    for(size_t i =0; i < mAudioInputs.size(); i ++ ) {
        closeInputStream(mAudioInputs[i]);
    }
    mAudioInputs.clear();
    for(size_t i =0; i < mAudioOutputs.size(); i ++ ) {
        closeOutputStream(mAudioOutputs[i]);
    }
    mAudioOutputs.clear();
    mInited = false;

}

status_t AudioHardware::initCheck()
{
    LOGV("%s: at %d\n",__FUNCTION__,__LINE__);
    return mInited?NO_ERROR:NO_INIT;
}

AudioStreamOut* AudioHardware::openOutputStream(
        uint32_t devices, int *format, uint32_t *channels, uint32_t *sampleRate, status_t *status)
{
    LOGV("%s: at %d format is %d channel is %d smapleRate is %d \n",__FUNCTION__,__LINE__,*format,*channels, *sampleRate);
    Mutex::Autolock lock(mLock);
    size_t index = 0;
    status_t lStatus = NO_ERROR;

    // Check whether device is supported
    switch (devices) {
        case AudioSystem::DEVICE_OUT_SPEAKER:
        case AudioSystem::DEVICE_OUT_AUX_DIGITAL:
            break;
        default:
            if (status) {
                *status = BAD_VALUE;
            }
            LOGE("%s: device %d is not supported!", __FUNCTION__, devices);
            return NULL;
    }

    for(index = 0; index < mAudioOutputs.size(); index ++ ) {
        if(devices == mAudioOutputs[index]->devices()) {
            LOGE("%s: at %d output %d is opened. Just one time\n",__FUNCTION__,__LINE__,devices);
            if(status) {
                *status = INVALID_OPERATION;
            }
            return NULL;
        }
    }

    AudioStreamOutNEXUS* out = AudioStreamOutNEXUS::create();
    if (out == NULL) {
        LOGE("%s: Could not create Nexus audio output stream!", __FUNCTION__);
        return NULL;
    }
    lStatus = out->set(devices,format, channels, sampleRate);
    if (status) {
        *status = lStatus;
    }
    if (lStatus == NO_ERROR) {
        mAudioOutputs.add(reinterpret_cast<AudioStreamOutNEXUS*>(out));
        return out;
    }
    delete out;
    return NULL;
}

void AudioHardware::closeOutputStream(AudioStreamOut* out)
{
    LOGV("%s: at %d\n",__FUNCTION__,__LINE__);
    Mutex::Autolock lock(mLock);
    ssize_t index = mAudioOutputs.indexOf(reinterpret_cast<AudioStreamOutNEXUS*>(out));
    if(index >=0) {
        delete out;
        mAudioOutputs.removeAt(index);
    }
    else {
        LOGE("%s: at %d, invalid operation \n",__FUNCTION__,__LINE__);
        return;
    }
}

AudioStreamIn* AudioHardware::openInputStream(
        uint32_t devices, int *format, uint32_t *channels, uint32_t *sampleRate,
        status_t *status, AudioSystem::audio_in_acoustics acoustics)
{
    LOGV("%s: at %d\n",__FUNCTION__,__LINE__);
    Mutex::Autolock lock(mLock);
    // check for valid input source
    if (!AudioSystem::isInputDevice((AudioSystem::audio_devices)devices)) {
        return 0;
    }
    AudioStreamInNEXUS* in = AudioStreamInNEXUS::create();
    if (in == NULL) {
        LOGE("%s: Could not create Nexus audio input stream!", __FUNCTION__);
        return NULL;
    }
    status_t lStatus = in->set(devices,format, channels, sampleRate, acoustics);
    if (status) {
        *status = lStatus;
    }
    if (lStatus == NO_ERROR) {
        size_t index = 0;
        for(index =0; index < mAudioInputs.size(); index ++ ) {
            if(devices == mAudioInputs[index]->devices()) {
                LOGE("%s: at %d input %d is opened. Just one time\n",__FUNCTION__,__LINE__,devices);
                if(status) {
                    *status = INVALID_OPERATION;
                }
                break;
            }
        }
        if(index >= mAudioInputs.size()) {
            mAudioInputs.add(reinterpret_cast<AudioStreamInNEXUS*>(in));
            return in;
        }
    }
    delete in;
    return NULL;
}

void AudioHardware::closeInputStream(AudioStreamIn* in)
{
    LOGV("%s: at %d\n",__FUNCTION__,__LINE__);
    Mutex::Autolock lock(mLock);
    ssize_t index = mAudioInputs.indexOf(reinterpret_cast<AudioStreamInNEXUS*>(in));
    if(index >=0) {
        delete in;
        mAudioInputs.removeAt(index);
    }
    else {
        LOGE("%s: at %d, invalid operation \n",__FUNCTION__,__LINE__);
    }
}

size_t AudioHardware::getInputBufferSize(uint32_t sampleRate, int format, int channelCount)
{
    size_t bufSize;
    if (sampleRate < 8000 || sampleRate > 48000) {
        LOGW("getInputBufferSize bad sampling rate: %d", sampleRate);
        return 0;
    }
    if ((format != AudioSystem::PCM_16_BIT) && (format != AudioSystem::PCM_8_BIT)) {
        LOGW("getInputBufferSize bad format: %d", format);
        return 0;
    }
    if ( (channelCount != 1) && (channelCount != 2)) {
        LOGW("getInputBufferSize bad channel count: %d", channelCount);
        return 0;
    }    
    if (sampleRate < 11025) {
        bufSize = 256;
    } 
    else if (sampleRate < 22050) {
        bufSize = 512;
    } 
    else if (sampleRate < 32000) {
        bufSize = 768;
    } 
    else if (sampleRate < 44100) {
        bufSize = 1024;
    } 
    else {
        bufSize = 2048;
    }
    return bufSize*channelCount;
}

status_t AudioHardware::setVoiceVolume(float volume)
{
    LOGV("%s: at %d\n",__FUNCTION__,__LINE__);
    return NO_ERROR;
}

status_t AudioHardware::setMasterVolume(float volume)
{
    LOGV("%s: at %d\n",__FUNCTION__,__LINE__);
    return NO_ERROR;
}

status_t AudioHardware::dumpInternals(int fd, const Vector<String16>& args)
{
    LOGV("%s: at %d\n",__FUNCTION__,__LINE__);
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;
    result.append("AudioHardware::dumpInternals\n");
    snprintf(buffer, SIZE, "\tmMicMute: %s\n", mMicMute? "true": "false");
    result.append(buffer);
    ::write(fd, result.string(), result.size());
    return NO_ERROR;
}

status_t AudioHardware::dump(int fd, const Vector<String16>& args)
{
    LOGV("%s: at %d\n",__FUNCTION__,__LINE__);
    dumpInternals(fd, args);
    return NO_ERROR;
}

status_t AudioHardware::setParameters(const String8& keyValuePairs)
{
    LOGV("setParameters() %s", keyValuePairs.string());
    Mutex::Autolock lock(mLock);

    if (keyValuePairs.length() == 0) return BAD_VALUE;
    
    for(size_t i =0; i < mAudioOutputs.size(); i ++ ) {
        mAudioOutputs[i]->setParameters(keyValuePairs);
    }

    for(size_t i =0; i < mAudioInputs.size(); i ++ ) {
        mAudioInputs[i]->setParameters(keyValuePairs);
    }
    return NO_ERROR;
}

//-------------------------------------------------


// ----------------------------------------------------------------------------

static void data_callback(void *pParam1, int param2)
{
    pParam1=pParam1;    /*unused*/
    BKNI_SetEvent((BKNI_EventHandle)param2);
}

AudioStreamOutNEXUS::AudioStreamOutNEXUS() : mSimpleAudioHandle(NULL),
                                             simpleAudioDecoder(NULL),
                                             mEvent(NULL),
                                             mFormat(AUDIO_NEXUS_OUT_FORMAT),
                                             mChannels(AUDIO_NEXUS_OUT_CHANNELS),
                                             mSampleRate(AUDIO_NEXUS_OUT_SAMPLERATE),
                                             mBufferSize(AUDIO_NEXUS_OUT_BUFSZ),
                                             mStarted(false),
                                             mAudioSuspended(false),
                                             mDevices(0),
                                             nexus_client(NULL),
                                             ipcclient(NULL)
{
    LOGV("AudioStreamOutNEXUS %s: at %d\n",__FUNCTION__,__LINE__);
}

AudioStreamOutNEXUS * AudioStreamOutNEXUS::create()
{
    b_refsw_client_client_configuration config;
    b_refsw_client_client_info client_info;
    b_refsw_client_connect_resource_settings connectSettings;
    AudioStreamOutNEXUS *audioStreamOutNexus;

    LOGV("AudioStreamOutNEXUS %s: at %d\n",__FUNCTION__,__LINE__);

    audioStreamOutNexus = new AudioStreamOutNEXUS();

    if (audioStreamOutNexus == NULL) {
        LOGE("%s: Could not instantiate AudioStreamOutNEXUS!", __FUNCTION__);
        return NULL;
    }

    audioStreamOutNexus->ipcclient = NexusIPCClientFactory::getClient("AudioStreamOutNEXUS");

    if (audioStreamOutNexus->ipcclient == NULL) {
        LOGE("%s: could not get NexusIPCClient!", __FUNCTION__);
        return NULL;
    }

    BKNI_Memset(&config, 0, sizeof(config));
    BKNI_Snprintf(config.name.string,sizeof(config.name.string),"AudioStreamOutNEXUS");
    config.resources.audioPlayback = true;
    audioStreamOutNexus->nexus_client = audioStreamOutNexus->ipcclient->createClientContext(&config);

    audioStreamOutNexus->ipcclient->getClientInfo(audioStreamOutNexus->nexus_client, &client_info);

    audioStreamOutNexus->ipcclient->getDefaultConnectClientSettings(&connectSettings);
    connectSettings.simpleAudioPlayback[0].id = client_info.audioPlaybackId;

    if (audioStreamOutNexus->ipcclient->connectClientResources(audioStreamOutNexus->nexus_client, &connectSettings) == 0) {
        LOGE("%s: Unable to connect playback resource!", __PRETTY_FUNCTION__);
        delete audioStreamOutNexus->ipcclient;
        audioStreamOutNexus->ipcclient = NULL;
        delete audioStreamOutNexus;
        return NULL;
    }

    audioStreamOutNexus->mSimpleAudioHandle = audioStreamOutNexus->ipcclient->acquireAudioPlaybackHandle();
    if ( NULL == audioStreamOutNexus->mSimpleAudioHandle ) {
        LOGE("%s: Unable to acquire Simple Playback!", __PRETTY_FUNCTION__);
        audioStreamOutNexus->ipcclient->disconnectClientResources(audioStreamOutNexus->nexus_client);
        delete audioStreamOutNexus->ipcclient;
        audioStreamOutNexus->ipcclient = NULL;
        delete audioStreamOutNexus;
        return NULL;
    }

    LOGV("AudioStreamOutNEXUS %s: at %d mBuffersize is %d \n",__FUNCTION__,__LINE__,audioStreamOutNexus->mBufferSize);
    BKNI_CreateEvent(&audioStreamOutNexus->mEvent);
    return audioStreamOutNexus;
}

AudioStreamOutNEXUS::~AudioStreamOutNEXUS()
{
    NEXUS_Error aud_err;
    NEXUS_PlatformConfiguration platformConfig;
    LOGV(" AudioStreamOutNEXUS %s: at %d\n",__FUNCTION__,__LINE__);
    Mutex::Autolock lock(mLock);
#if 1
    standby();
#else

    if(mSimpleAudioHandle)
        NEXUS_SimpleAudioPlayback_Stop(mSimpleAudioHandle);

    mStarted = false;
#endif

    if (mSimpleAudioHandle) {
        ipcclient->releaseAudioPlaybackHandle(mSimpleAudioHandle);
        mSimpleAudioHandle = NULL;
    }

    if(ipcclient) {
        if(nexus_client) {
            ipcclient->disconnectClientResources(nexus_client);
            ipcclient->destroyClientContext(nexus_client);
            nexus_client = NULL;
        }
        delete ipcclient;
        ipcclient = NULL;
    }

    BKNI_DestroyEvent(mEvent);
    mEvent = NULL;
}


status_t AudioStreamOutNEXUS::set(uint32_t devices,int *pFormat, uint32_t *pChannels, uint32_t *pRate)
{
    status_t ret = NO_ERROR;

    int iFormat = 0;
    uint32_t uChannels = 0;
    uint32_t uRate = 0;

    LOGV(" AudioStreamOutNEXUS %s: at %d pFormat is %d pChannels is %d pRate is %d\n",__FUNCTION__,__LINE__, *pFormat, *pChannels, *pRate);

    if(pFormat) {
        iFormat = *pFormat;
        if(iFormat == 0) {
            *pFormat = mFormat;
            iFormat = mFormat;
        }
        if(iFormat != mFormat) {
            *pFormat = mFormat;
            ret = BAD_VALUE;
        }
    }
    else {
        iFormat = mFormat;
    }

    if(pChannels) {
        uChannels = *pChannels;
        if(uChannels == 0) {
            *pChannels = mChannels;
            uChannels = mChannels;
        }
        if(uChannels != mChannels) {
            *pChannels = mChannels;
            ret = BAD_VALUE;
        }
    }
    else {
        uChannels= mChannels;
    }

    if(pRate) {
        uRate = *pRate;
        if(uRate == 0) {
            *pRate = mSampleRate;
            uRate = mSampleRate;
        }
        if(uRate != mSampleRate) {
            *pRate = mSampleRate;
            ret = BAD_VALUE;
        }
    }
    else {
        uRate = mSampleRate;
    }

    mFormat = iFormat;
    mChannels = uChannels;
    mSampleRate = uRate;
    mDevices = devices;
    

    LOGV(" AudioStreamOutNEXUS %s: at %d pFormat is %d pChannels is %d pRate is %d\n",__FUNCTION__,__LINE__, *pFormat, *pChannels, *pRate);
    return ret;
}

ssize_t AudioStreamOutNEXUS::write(const void* buffer, size_t bytes)
{
    BERR_Code errCode;
    void *pBuffer = NULL;
    size_t bufferSize;
    size_t bytesPlayed=0;
    size_t bytesToCopy=0;
    size_t bytesToPlay=bytes;
    size_t offset=0;
    NEXUS_Error aud_err;
    NEXUS_SimpleAudioPlaybackStartSettings startSettings;
    
    Mutex::Autolock lock(mLock);

    if (!mAudioSuspended && mSimpleAudioHandle) {
        LOGVV("AudioStreamOutNEXUS %s: at %d bytes to play=%d\n",__FUNCTION__,__LINE__,bytes);
        if(!mStarted) {
            int iformat= format();
            uint32_t uChannels = channels();
            uint32_t uRate = sampleRate();

            NEXUS_SimpleAudioPlayback_GetDefaultStartSettings(&startSettings);
            startSettings.signedData = true;
            switch(iformat) {
                case AudioSystem::PCM_16_BIT:
                    startSettings.bitsPerSample = 16;
                    break;

                case AudioSystem::PCM_8_BIT:
                    startSettings.bitsPerSample = 8;
                    startSettings.signedData = false;
                    break;

                default:
                    startSettings.bitsPerSample = 16;
                    break;
            }
            switch(uChannels) {
                case AudioSystem::CHANNEL_OUT_MONO:
                    startSettings.stereo = false;
                    break;
                case AudioSystem::CHANNEL_OUT_STEREO:
                    startSettings.stereo = true;
                    break;
                default:
                    startSettings.stereo = true;
                    break;

            }                
            startSettings.sampleRate = uRate;
            startSettings.dataCallback.callback = data_callback;
            startSettings.dataCallback.context = mSimpleAudioHandle;
            startSettings.dataCallback.param = (int)mEvent;
            startSettings.startThreshold = 128;
            aud_err = NEXUS_SimpleAudioPlayback_Start (mSimpleAudioHandle, &startSettings);
            LOGV("AudioStreamOutNEXUS %s: at %d  startThreshold is %d\n",__FUNCTION__,__LINE__,startSettings.startThreshold);
            BDBG_ASSERT(!aud_err);
            mStarted = true;
        }

        do
        {
            /* Check available buffer space */
            errCode = NEXUS_SimpleAudioPlayback_GetBuffer(mSimpleAudioHandle, &pBuffer, &bufferSize);
            if ( errCode ) {
                LOGE("AudioStreamOutNEXUS %s: at %d Error getting playback buffer\n",__FUNCTION__,__LINE__);
                break;
            }

            if (bufferSize) {
                bytesToCopy = (bytesToPlay<=bufferSize) ? bytesToPlay : bufferSize;
                memcpy(pBuffer, reinterpret_cast<const void *>(reinterpret_cast<const char *>(buffer) + bytesPlayed), bytesToCopy);
                errCode = NEXUS_SimpleAudioPlayback_WriteComplete(mSimpleAudioHandle, bytesToCopy);
                if ( errCode ) {
                    LOGE("%s: at %d Error committing playback buffer\n",__FUNCTION__,__LINE__);
                    break;
                }
                bytesPlayed += bytesToCopy;
                bytesToPlay = bytes - bytesPlayed;
            }
            else {
                /* Wait for data callback */
                LOGVV("%s: waiting for space to become available...", __PRETTY_FUNCTION__);
                errCode = BKNI_WaitForEvent(mEvent, 500);
                if(mStarted) {
                    LOGV("%s: Stopping playback for some reason.", __PRETTY_FUNCTION__);
                    NEXUS_SimpleAudioPlayback_Stop(mSimpleAudioHandle);
                    mStarted=false;
                    break;
                }
            }
        } while ( BERR_SUCCESS == errCode &&  bytesToPlay > 0 );

        LOGVV("Waiting for buffer to empty\n");
        /* remove the audio delay here */
        for ( ;; ) {
            NEXUS_SimpleAudioPlaybackStatus status;
            NEXUS_SimpleAudioPlayback_GetStatus(mSimpleAudioHandle, &status);
            if ( status.queuedBytes > mBufferSize*4) {
                BKNI_Sleep(10);
            }
            else {
                break;
            }
        }
    }
    else {
        LOGVV("AudioStreamOutNEXUS %s: at %d faking bytes to play=%d\n", __FUNCTION__, __LINE__, bytes);
        bytesPlayed = bytes;
    }

    return bytesPlayed;
}

status_t AudioStreamOutNEXUS::setVolume(float left, float right)
{

    ipcclient->setAudioVolume(left, right);

    return NO_ERROR;
}


status_t AudioStreamOutNEXUS::standby()
{
    LOGV("AudioStreamOutNEXUS %s: at %d\n",__FUNCTION__,__LINE__);
    if(mStarted) {
        if(mSimpleAudioHandle) 
            NEXUS_SimpleAudioPlayback_Stop(mSimpleAudioHandle);
        mStarted = false;
    }
    
    return NO_ERROR;
}

status_t AudioStreamOutNEXUS::dump(int fd, const Vector<String16>& args)
{
    LOGV("AudioStreamOutNEXUS %s: at %d\n",__FUNCTION__,__LINE__);
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;
    snprintf(buffer, SIZE, "AudioStreamOutNEXUS::dump\n");
    snprintf(buffer, SIZE, "\tsample rate: %d\n", sampleRate());
    snprintf(buffer, SIZE, "\tbuffer size: %d\n", bufferSize());
    snprintf(buffer, SIZE, "\tchannels: %d\n", channels());
    snprintf(buffer, SIZE, "\tformat: %d\n", format());
    result.append(buffer);
    ::write(fd, result.string(), result.size());
    return NO_ERROR;
}

String8 AudioStreamOutNEXUS::getParameters(const String8& keys)
{
    LOGV(" AudioStreamOutNEXUS %s: at %d\n",__FUNCTION__,__LINE__);
    AudioParameter param = AudioParameter(keys);
    LOGV("AudioStreamOutNEXUS: getParameters() %s", keys.string());
    
    return param.toString();
}

status_t   AudioStreamOutNEXUS::setParameters(const String8& keyValuePairs)
{
    AudioParameter param = AudioParameter(keyValuePairs);
    String8 value;
    String8 key;
    const char STANDBY_KEY[] = "screen_state";

    LOGV("AudioStreamOutNEXUS: setParameters() %s", keyValuePairs.string());

    if (keyValuePairs.length() == 0) return BAD_VALUE;
    
    key = String8(STANDBY_KEY);
    if (param.get(key, value) == NO_ERROR) {
        if (value == "off") {
            LOGV("%s: Need to enter power saving mode...", __FUNCTION__);
            mAudioSuspended = true;
            standby();
        }
        else {
            LOGV("%s: Need to exit power saving mode...", __FUNCTION__);
            mAudioSuspended = false;
        }
    }
    return NO_ERROR;
}

status_t AudioStreamOutNEXUS::getRenderPosition(uint32_t *dspFrames)
{
    LOGV("AudioStreamOutNEXUS %s: at %d\n",__FUNCTION__,__LINE__);
    return INVALID_OPERATION;
}

// ----------------------------------------------------------------------------

const uint32_t AudioStreamInNEXUS::streamInSamplingRates[] = {
        8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};


AudioStreamInNEXUS::AudioStreamInNEXUS():mAudioInputDevice(-1),mFormat(AUDIO_NEXUS_IN_FORMAT),mChannels(AUDIO_NEXUS_IN_CHANNELS),mSampleRate(AUDIO_NEXUS_IN_SAMPLERATE),mFragment(AUDIO_NEXUS_IN_FRAGMENT),mStarted(false),mDevices(0)
{
    LOGV("AudioStreamInNEXUS %s: at %d\n",__FUNCTION__,__LINE__);
}

AudioStreamInNEXUS * AudioStreamInNEXUS::create()
{
    LOGV("AudioStreamInNEXUS %s: at %d\n",__FUNCTION__,__LINE__);
    return new AudioStreamInNEXUS();
}

AudioStreamInNEXUS::~AudioStreamInNEXUS()
{
    LOGV("AudioStreamInNEXUS %s: at %d\n",__FUNCTION__,__LINE__);
    
    standby();
}


status_t AudioStreamInNEXUS::set(uint32_t devices,int *pFormat, uint32_t *pChannels, uint32_t *pRate,
                AudioSystem::audio_in_acoustics acoustics)
{    
    LOGV(" AudioStreamInNEXUS %s: at %d AudioSystem::CHANNEL_IN_MONO is %d ,AudioSystem::CHANNEL_IN_STEREO is %d  \n",__FUNCTION__,__LINE__, AudioSystem::CHANNEL_IN_MONO, AudioSystem::CHANNEL_IN_STEREO);
    /*mFragment= 0x7fff0006;*/ /* no limitation, 2^6 buffer size*/
    int iFormat = 0;
    uint32_t uChannels = 0;
    uint32_t uRate = 0;


    LOGV("AudioStreamInNEXUS %s: at %d format is %d ,channels is %d rate is %d \n",__FUNCTION__,__LINE__, *pFormat, *pChannels, *pRate);

    if(pFormat) {
        iFormat = *pFormat;
        if(iFormat == 0) {
            *pFormat = mFormat;
            iFormat = mFormat;
        }
        if(iFormat != mFormat) {
            if((iFormat == AudioSystem::PCM_16_BIT) || (iFormat == AudioSystem::PCM_8_BIT) ) {
                mFormat = iFormat;
            }
            else {
                *pFormat = mFormat;
                return BAD_VALUE;
            }
        }
    }
    else {
        iFormat = mFormat;
    }

    if(pChannels) {
        uChannels = *pChannels;
        if(uChannels == 0) {
            *pChannels = mChannels;
            uChannels = mChannels;
        }
        if(uChannels != mChannels) {
            if((uChannels == AudioSystem::CHANNEL_IN_MONO) || (uChannels == AudioSystem::CHANNEL_IN_STEREO) ) {
                mChannels= uChannels;
            }
            else {
                *pChannels = mChannels;
                return BAD_VALUE;
            }
        }
    }
    else {
        uChannels = mChannels;
    }
    
    if(pRate) {
        uRate = *pRate;
        if(uRate == 0) {
            *pRate = mSampleRate;
            uRate = mSampleRate;
        }
        if(uRate != mSampleRate) {
            uint32_t i ;
            for(i=0; i< sizeof(streamInSamplingRates)/sizeof(streamInSamplingRates[0]); i++) {
                if(uRate == streamInSamplingRates[i]) {    
                    mSampleRate = uRate;
                    break;
                }
            }
            if(i >= sizeof(streamInSamplingRates)/sizeof(streamInSamplingRates[0])) {
                *pRate = mSampleRate;
                return BAD_VALUE;
            }
        }
    }
    else {
        uRate = mSampleRate;
    }

    mFormat = iFormat;
    mChannels = uChannels;
    mSampleRate = uRate;
    mDevices = devices;
    
    LOGV(" AudioStreamInNEXUS 222%s: at %d format is %d ,channels is %d rate is %d \n",__FUNCTION__,__LINE__, mFormat, mChannels, mSampleRate);

    return NO_ERROR;
}

ssize_t AudioStreamInNEXUS::read(void* buffer, ssize_t bytes)
{
    /*LOGV("AudioStreamInNEXUS %s: at %d bytes is %d \n",__FUNCTION__,__LINE__,static_cast<uint32_t>(bytes));*/

    ssize_t size = 0;
    ssize_t len = 0;
    int i =0;

    if(mAudioInputDevice <= 0 ) {
        return INVALID_OPERATION;
    }
    
    if(!mStarted) {
        int result = 0;
        int tmp = 0;
        int iFormat = format();
        uint32_t uChannels = channels();
        uint32_t uRate = sampleRate();
        switch(iFormat) {
            case AudioSystem::PCM_16_BIT:
                tmp =  AFMT_S16_LE;
                break;
            case AudioSystem::PCM_8_BIT:
                tmp =  AFMT_S8;
                break;
            default:
                tmp =  AFMT_S16_LE;
                break;
        }
        result = ioctl(mAudioInputDevice,SNDCTL_DSP_SETFMT, &tmp);
        if (result  == -1) {
            LOGE("%s: at %d format failed\n",__FUNCTION__,__LINE__);
        }
        switch(uChannels) {
            case AudioSystem::CHANNEL_IN_MONO:
                tmp =  0;
                break;
            case AudioSystem::CHANNEL_IN_STEREO:
                tmp =  1;
                break;
            default:
                mChannels =  0;
                break;
        }
        result = ioctl(mAudioInputDevice,SNDCTL_DSP_CHANNELS, &tmp);
        if (result  == -1) {
            LOGE("%s: at %d after channel failed\n",__FUNCTION__,__LINE__);
        }

        tmp = uRate;
        result = ioctl(mAudioInputDevice,SNDCTL_DSP_SPEED, &tmp);
        if (result  == -1) {
            LOGE("%s: at %d sampleRate failed\n",__FUNCTION__,__LINE__);
        }
        tmp = mFragment;
        result = ioctl(mAudioInputDevice,SNDCTL_DSP_SETFRAGMENT, &tmp);
        if (result  == -1) {
            LOGE("%s: at %d fragment failed\n",__FUNCTION__,__LINE__);
        }
        mStarted = true;
    }

    while (size < bytes) {
        len = ::read(mAudioInputDevice, reinterpret_cast<void *>(reinterpret_cast<char *>(buffer) + size), bytes - size);
        /*LOGV("%s: at %d read %d  total read is %d , target %d bytes \n",__FUNCTION__,__LINE__, static_cast<uint32_t>(len),static_cast<uint32_t>(size),static_cast<uint32_t>(bytes));*/
        if(len < 0) {
            if(errno != EAGAIN) {
                LOGE("%s: at %d read failed \n",__FUNCTION__,__LINE__);
                break;
            }
            else {
                usleep(1000);
            }
        }
        else {
            size +=len;
            usleep(100);
        }
    }
    return size;
}

status_t AudioStreamInNEXUS::dump(int fd, const Vector<String16>& args)
{
    LOGV("AudioStreamInNEXUS %s: at %d\n",__FUNCTION__,__LINE__);
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;
    snprintf(buffer, SIZE, "AudioStreamInNEXUS::dump\n");
    result.append(buffer);
    snprintf(buffer, SIZE, "\tsample rate: %d\n", sampleRate());
    result.append(buffer);
    snprintf(buffer, SIZE, "\tbuffer size: %d\n", bufferSize());
    result.append(buffer);
    snprintf(buffer, SIZE, "\tchannels: %d\n", channels());
    result.append(buffer);
    snprintf(buffer, SIZE, "\tformat: %d\n", format());
    result.append(buffer);
    ::write(fd, result.string(), result.size());
    return NO_ERROR;
}

status_t AudioStreamInNEXUS::standby()
{
    LOGV("AudioStreamInNEXUS %s: at %d\n",__FUNCTION__,__LINE__);
    if(mStarted) {
        if(mAudioInputDevice > 0) {
            close(mAudioInputDevice);
        }
        mAudioInputDevice = -1;
        mStarted = false;
    }
    return NO_ERROR;
}

String8 AudioStreamInNEXUS::getParameters(const String8& keys)
{
    AudioParameter param = AudioParameter(keys);
    String8 value;
    String8 key = String8(AudioParameter::keyRouting);
    LOGV("AudioStreamInNEXUS %s: at %d, keys %s\n",__FUNCTION__,__LINE__,keys.string());
    if (param.get(key, value) == NO_ERROR) {
        LOGV("the device is  %x\n", mDevices);
        param.addInt(key, static_cast<int>(mDevices));
    }
    LOGV("AudioStreamInNEXUS %s: at %d, keys %s\n",__FUNCTION__,__LINE__,param.toString().string());
    return param.toString();
}

status_t   AudioStreamInNEXUS::setParameters(const String8& keyValuePairs)
{
    AudioParameter param = AudioParameter(keyValuePairs);
    int device;
    String8 key = String8(AudioParameter::keyInputSource);
    status_t ret = NO_ERROR;
    
    LOGV("AudioStreamInNEXUS: setParameters() %s\n", keyValuePairs.string());
    if (keyValuePairs.length() == 0) return BAD_VALUE;

    if (param.getInt(key, device) == NO_ERROR) {
        /* TODO: to recongnition */
        if (AUDIO_SOURCE_VOICE_RECOGNITION  == device) {
            LOGV("AudioStreamInNEXUS set input source %d", device);
        }
        param.remove(key);
    }
     // get the device 
    key = String8(AudioParameter::keyRouting);
    if (param.getInt(key, device) == NO_ERROR) {
        LOGV("%s:at %d set input device %x", __FUNCTION__,__LINE__,device);
        /* only one bit set */
        if ((device == 0) || (device & (device - 1))) {
            ret = BAD_VALUE;
        }
        else {
            mDevices = device;
            /* the table is in AudioSystem.h */
            /* TODO: only test with buildin mic (usb camera) */
            if(mDevices  == AudioSystem::DEVICE_IN_BUILTIN_MIC) {
                LOGV("%s:at%d Open input device %x", __FUNCTION__,__LINE__,device);
                const char *devName = reinterpret_cast<const char *>(AUDIO_INPUT_BUILTIN_MIC);
                mAudioInputDevice = open(devName, O_RDWR);
                if(mAudioInputDevice <=0) {
                    LOGE("AudioStreamInNEXUS %s: at %d Open audio input failed\n",__FUNCTION__,__LINE__);
                }
                fcntl(mAudioInputDevice, F_SETFL, O_NONBLOCK);
            }
        }
        param.remove(key);
    }
    return ret;
}


// ----------------------------------------------------------------------------

extern "C" AudioHardwareInterface* createAudioHardware(void) {
    return new AudioHardware();
}

}; // namespace android_audio_legacy
