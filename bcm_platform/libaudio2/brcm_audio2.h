/******************************************************************************
 *    (c)2014 Broadcom Corporation
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
 * $brcm_Workfile: brcm_audio2.h $
 * $brcm_Revision:  $
 * $brcm_Date: 7/20/14 12:05p $
 * $brcm_Author: ttrammel@broadcom.com
 * 
 * Module Description:
 * 
 * Revision History:
 * 
 * $brcm_Log:  $
 * 
 *****************************************************************************/
#ifndef BRCM_AUDIO2_H
#define BRCM_AUDIO2_H

#include <stdint.h>
#include <sys/types.h>

#include <hardware/audio.h>
#include "bstd.h"
#include "berr.h"
#include "nexus_platform.h"
#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_audio_playback.h"
#include "nexus_audio_mixer.h"
#include "nexus_audio_dac.h"
#include "nexus_audio_decoder.h"
#include "nexus_audio_output.h"
#include "nexus_audio_input.h"


#include <utils/SortedVector.h>
#include <utils/threads.h>
#include <utils/Vector.h>

#include "nexus_simple_audio_decoder.h"
#include "nexus_simple_audio_playback.h"

#include "nexus_ipc_client_factory.h"

#define AUDIO_NEXUS_OUT_LATENCY_MS 0  
#define AUDIO_NEXUS_OUT_SAMPLERATE 44100 
#define AUDIO_NEXUS_OUT_CHANNELS (AudioSystem::CHANNEL_OUT_STEREO) 
#define AUDIO_NEXUS_OUT_FORMAT (AudioSystem::PCM_16_BIT)  
#define AUDIO_NEXUS_OUT_BUFSZ (8192)  
 
#define AUDIO_NEXUS_IN_SAMPLERATE 8000
#define AUDIO_NEXUS_IN_CHANNELS (AudioSystem::CHANNEL_IN_MONO) 
#define AUDIO_NEXUS_IN_FORMAT (AudioSystem::PCM_16_BIT)  
#define AUDIO_NEXUS_IN_FRAGMENT (0x7fff0006) /* no limitation, 2^6 buffer size*/
#define AUDIO_NEXUS_IN_ACOUSTICS AudioSystem::AGC_ENABLE

// ----------------------------------------------------------------------------

class AudioStreamOutNEXUS : public AudioStreamOut {

    public:
    static  AudioStreamOutNEXUS *   create();
    virtual                         ~AudioStreamOutNEXUS();
    virtual status_t                set(uint32_t devices,int *pFormat, uint32_t *pChannels, uint32_t *pRate);
    virtual uint32_t                sampleRate() const { return mSampleRate;}    
    virtual size_t                  bufferSize() const { return mBufferSize;}    
    virtual uint32_t                channels() const { return mChannels;}    
    virtual int                     format() const { return mFormat;}    
#if 1
    virtual uint32_t                latency() const { return (1000*(bufferSize()/frameSize()))/sampleRate()+AUDIO_NEXUS_OUT_LATENCY_MS; }
#else
    virtual uint32_t                latency() const { return 0; }
#endif
    virtual status_t                setVolume(float left, float right);
    virtual ssize_t                 write(const void* buffer, size_t bytes);
    virtual status_t                standby();
    virtual status_t                dump(int fd, const Vector<String16>& args);
    virtual status_t                setParameters(const String8& keyValuePairs);
    virtual String8                 getParameters(const String8& keys);
    virtual status_t                getRenderPosition(uint32_t *dspFrames);
    uint32_t                        devices() { return mDevices; }

private:
                                    AudioStreamOutNEXUS();

    NEXUS_SimpleAudioPlaybackHandle mSimpleAudioHandle;
    NEXUS_SimpleAudioDecoderHandle  simpleAudioDecoder;
    BKNI_EventHandle                mEvent;
    int                             mFormat;    
    uint32_t                        mChannels;
    uint32_t                        mSampleRate;
    size_t                          mBufferSize;
    bool                            mStarted;
    bool                            mAudioSuspended;
    uint32_t                        mDevices;
    Mutex                           mLock;
    NexusClientContext *            nexus_client;
    NexusIPCClientBase *            ipcclient;

    // Disallow copy constructor and assignment operator
    AudioStreamOutNEXUS(const AudioStreamOutNEXUS &);
    AudioStreamOutNEXUS &operator=(const AudioStreamOutNEXUS &);
};

class AudioStreamInNEXUS : public AudioStreamIn {
public:
    static AudioStreamInNEXUS *    create();
    virtual                         ~AudioStreamInNEXUS();
    virtual status_t                set(uint32_t devices,int *pFormat, uint32_t *pChannels, uint32_t *pRate, AudioSystem::audio_in_acoustics acoustics);
    virtual uint32_t                sampleRate() const { return mSampleRate; }
    virtual size_t                  bufferSize() const { return 1 << (mFragment &0xFF); }
    virtual uint32_t                channels() const { return mChannels;}
    virtual int                     format() const { return mFormat;}
    virtual status_t                setGain(float gain) { return NO_ERROR; }
    virtual ssize_t                 read(void* buffer, ssize_t bytes);
    virtual status_t                dump(int fd, const Vector<String16>& args);
    virtual status_t                standby() ;
    virtual status_t                setParameters(const String8& keyValuePairs);
    virtual String8                 getParameters(const String8& keys);
    virtual unsigned int            getInputFramesLost() const { return 0; }
    virtual status_t                addAudioEffect(effect_handle_t effect) { return NO_ERROR; }
    virtual status_t                removeAudioEffect(effect_handle_t effect) { return NO_ERROR; }
    uint32_t                        devices() { return mDevices; }
private :
                                    AudioStreamInNEXUS();

    static const uint32_t           streamInSamplingRates[];
    int                             mAudioInputDevice;
    int                             mFormat;
    uint32_t                        mChannels;
    uint32_t                        mSampleRate;
    int                             mFragment;
    bool                            mStarted;
    uint32_t                        mDevices;

    // Disallow copy constructor and assignment operator
    AudioStreamInNEXUS(const AudioStreamInNEXUS &);
    AudioStreamInNEXUS &operator=(const AudioStreamInNEXUS &);
};

class AudioHardware : public  AudioHardwareBase
{
public:
                                    AudioHardware();
    virtual                         ~AudioHardware();
    virtual status_t                initCheck();
    virtual status_t                setVoiceVolume(float volume);
    virtual status_t                setMasterVolume(float volume);

    // mic mute
    virtual status_t                setMicMute(bool state) { mMicMute = state;  return  NO_ERROR; }
    virtual status_t                getMicMute(bool* state) { *state = mMicMute ; return NO_ERROR; }

    // create I/O streams
    virtual AudioStreamOut*         openOutputStream(
                                        uint32_t devices,
                                        int      *format=0,
                                        uint32_t *channels=0,
                                        uint32_t *sampleRate=0,
                                        status_t *status=0);
    virtual    void                 closeOutputStream(AudioStreamOut* out);

    virtual AudioStreamIn*          openInputStream(
                                        uint32_t devices,
                                        int      *format,
                                        uint32_t *channels,
                                        uint32_t *sampleRate,
                                        status_t *status,
                                        AudioSystem::audio_in_acoustics acoustics);
    virtual    void                 closeInputStream(AudioStreamIn* in);
    // Returns audio input buffer size according to parameters passed or 0 if one of the
    // parameters is not supported
    virtual size_t                  getInputBufferSize(uint32_t sampleRate, int format, int channelCount);

    virtual status_t                setParameters(const String8& keyValuePairs);

protected:
    virtual status_t                dump(int fd, const Vector<String16>& args);
    bool                            mMicMute;
private:
    status_t                        dumpInternals(int fd, const Vector<String16>& args);
    SortedVector <AudioStreamOutNEXUS*> mAudioOutputs;
    SortedVector <AudioStreamInNEXUS*>  mAudioInputs;
    Mutex                           mLock;
    bool                            mInited;
};

// ----------------------------------------------------------------------------

#endif // BRCM_AUDIO2_H
