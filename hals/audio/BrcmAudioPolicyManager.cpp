/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "BrcmAudioPolicyManager"
//#define LOG_NDEBUG 0
#include <media/AudioParameter.h>
#include <media/mediarecorder.h>
#include <utils/Log.h>
#include <utils/String16.h>
#include <utils/String8.h>
#include <utils/StrongPointer.h>

#include "BrcmAudioPolicyManager.h"

namespace android {

// ----------------------------------------------------------------------------
// Common audio policy manager code is implemented in AudioPolicyManager class
// ----------------------------------------------------------------------------

// ---  class factory


extern "C" AudioPolicyInterface* createAudioPolicyManager(
        AudioPolicyClientInterface *clientInterface)
{
    return new BrcmAudioPolicyManager(clientInterface);
}

extern "C" void destroyAudioPolicyManager(AudioPolicyInterface *interface)
{
    delete interface;
}

BrcmAudioPolicyManager::BrcmAudioPolicyManager(
        AudioPolicyClientInterface *clientInterface)
    : AudioPolicyManager(clientInterface), mForceSubmixInputSelection(false)
{
}

float BrcmAudioPolicyManager::computeVolume(audio_stream_type_t stream,
                                           int index,
                                           audio_devices_t device)
{
    // We only use master volume, so all audio flinger streams
    // should be set to maximum
    (void)stream;
    (void)index;
    (void)device;
    return 1.0;
}

status_t BrcmAudioPolicyManager::setDeviceConnectionState(audio_devices_t device,
                                                         audio_policy_dev_state_t state,
                                                         const char *device_address,
                                                         const char *device_name)
{
    audio_devices_t tmp = AUDIO_DEVICE_NONE;;
    ALOGI("setDeviceConnectionState %08x %x %s", device, state,
          device_address ? device_address : "(null)");

    // If the input device is the remote submix and an address starting with "force=" was
    // specified, enable "force=1" / disable "force=0" the forced selection of the remote submix
    // input device over hardware input devices (e.g RemoteControl).
    if (device == AUDIO_DEVICE_IN_REMOTE_SUBMIX && device_address) {
        AudioParameter parameters = AudioParameter(String8(device_address));
        int forceValue;
        if (parameters.getInt(String8("force"), forceValue) == OK) {
            mForceSubmixInputSelection = forceValue != 0;
        }
    }

    status_t ret = 0;
    if (device != AUDIO_DEVICE_IN_REMOTE_SUBMIX) {
        ret = AudioPolicyManager::setDeviceConnectionState(
                    device, state, device_address, device_name);
    }

    return ret;
}

audio_devices_t BrcmAudioPolicyManager::getDeviceForInputSource(audio_source_t inputSource)
{
    uint32_t device = AUDIO_DEVICE_NONE;

    if (inputSource == AUDIO_SOURCE_VOICE_RECOGNITION) {
        ALOGV("getDeviceForInputSource %s", mForceSubmixInputSelection ? "use virtual" : "");
        audio_devices_t availableDeviceTypes = mAvailableInputDevices.types() &
                                                ~AUDIO_DEVICE_BIT_IN;

        if (availableDeviceTypes & AUDIO_DEVICE_IN_REMOTE_SUBMIX &&
                mForceSubmixInputSelection) {
            // REMOTE_SUBMIX should always be avaible, let's make sure it's being forced at the moment
            ALOGV("Virtual remote available");
            device = AUDIO_DEVICE_IN_REMOTE_SUBMIX;
        }
    }
    else if (inputSource == AUDIO_SOURCE_CAMCORDER) {
        audio_devices_t availableDeviceTypes = mAvailableInputDevices.types() &
                                                ~AUDIO_DEVICE_BIT_IN;
        if (availableDeviceTypes & AUDIO_DEVICE_IN_USB_DEVICE) {
            ALOGV("Use USB MIC for CAMCORDER");
            device = AUDIO_DEVICE_IN_USB_DEVICE;
        }
    }

    // Fall back to default policy if no device is found
    if (device == AUDIO_DEVICE_NONE) {
        device = AudioPolicyManager::getDeviceForInputSource(inputSource);
    }

    ALOGV("getDeviceForInputSource() input source %d, device %08x", inputSource, device);
    return device;
}

}  // namespace android
