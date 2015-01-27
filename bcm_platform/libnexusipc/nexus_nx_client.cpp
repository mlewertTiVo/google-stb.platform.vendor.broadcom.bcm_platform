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
 * $brcm_Workfile: nexus_ipc_client.cpp $
 * $brcm_Revision: 10 $
 * $brcm_Date: 12/14/12 2:28p $
 * 
 * Module Description:
 * This file contains the implementation of the class that uses Nexus bipc
 * communication with the Nexus Server process (nxserver) and Binder IPC
 * communication with the Nexus Service.  A client process should NOT instantiate
 * an object of this type directly.  Instead, they should use the "getClient()"
 * method of the NexusIPCClientFactory abstract factory class.
 * On the client side, the definition of these API functions either encapsulate
 * the API into a command + param format and sends the command over binder to
 * the server side for actual execution or via Nexus bipc to nxserver.
 * 
 * Revision History:
 * 
 * $brcm_Log: /AppLibs/opensource/android/src/broadcom/ics/vendor/broadcom/bcm_platform/libnexusipc/nexus_nx_client.cpp $
 * 
 *****************************************************************************/
  
#define LOG_TAG "NexusNxClient"
//#define LOG_NDEBUG 0

#include <utils/Log.h>
#include <string.h>
#include <cutils/atomic.h>
#include <utils/Errors.h>
#include "cutils/properties.h"

// required for binder api calls
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include "nexus_interface.h"
#include "nexusservice.h"
#include "nexus_ipc_priv.h"
#include "nexus_nx_client.h"
#include "blst_list.h"

/* Define the MAX timeout to wait after setting NxClient standby settings
   to allow standby monitor threads to complete. */
#define NXCLIENT_PM_TIMEOUT_IN_MS (2000)

/* Define the MAX number of times to monitor for the NxClient to have
   entered the desired standby mode. */
#define NXCLIENT_PM_TIMEOUT_COUNT (20)

#ifdef UINT32_C
#undef UINT32_C
#define UINT32_C(x)  (x ## U)
#endif

NexusNxClient::StandbyMonitorThread::~StandbyMonitorThread()
{
    ALOGD("%s: called", __PRETTY_FUNCTION__); 

    if (this->name != NULL) {
        free(name);
        this->name = NULL;
    }
}

android::status_t NexusNxClient::StandbyMonitorThread::run(const char* name, int32_t priority, size_t stack)
{
    android::status_t status;
    
    this->name = strdup(name);

    status = Thread::run(name, priority, stack);
    if (status == android::OK) {
        state = StandbyMonitorThread::STATE_RUNNING;
    }
    return android::OK;
}

bool NexusNxClient::StandbyMonitorThread::threadLoop()
{
    NEXUS_Error rc;
    NxClient_StandbyStatus standbyStatus, prevStatus;

    LOGD("%s: Entering for client \"%s\"", __PRETTY_FUNCTION__, getName());

    NxClient_GetStandbyStatus(&standbyStatus);
    
    prevStatus = standbyStatus;

    while (isRunning()) {
        rc = NxClient_GetStandbyStatus(&standbyStatus);
    
        if (rc == NEXUS_SUCCESS && standbyStatus.settings.mode != prevStatus.settings.mode) {
            bool ack = true;

            if (standbyStatus.settings.mode != NEXUS_PlatformStandbyMode_eOn) {
                if (mCallback != NULL) {
                    ack =  mCallback(mContext);
                }
            }
            if (ack) {
                LOGD("%s: Acknowledge state %d\n", getName(), standbyStatus.settings.mode);
                NxClient_AcknowledgeStandby(true);
                prevStatus = standbyStatus;
            }
        }
        BKNI_Sleep(NXCLIENT_STANDBY_MONITOR_TIMEOUT_IN_MS);
    }
    LOGD("%s: Exiting for client \"%s\"", __PRETTY_FUNCTION__, getName());
    return false;
}

#define NEXUS_TRUSTED_DATA_PATH "/data/misc/nexus"
NEXUS_Error NexusNxClient::clientJoin()
{
    NEXUS_Error rc = NEXUS_SUCCESS;
    NxClient_JoinSettings joinSettings;
    NEXUS_PlatformStatus status;
    char value[PROPERTY_VALUE_MAX];
    FILE *key = NULL;

    android::Mutex::Autolock autoLock(mLock);

    if (mJoinRefCount == 0) {
        NxClient_GetDefaultJoinSettings(&joinSettings);
        BKNI_Snprintf(&joinSettings.name[0], NXCLIENT_MAX_NAME, "%s", getClientName());

        sprintf(value, "%s/nx_key", NEXUS_TRUSTED_DATA_PATH);
        key = fopen(value, "r");
        joinSettings.mode = NEXUS_ClientMode_eUntrusted;
        if (key == NULL) {
           ALOGE("%s: failed to open key file \'%s\', err=%d (%s)\n", __FUNCTION__, value, errno, strerror(errno));
        } else {
           memset(value, 0, sizeof(value));
           fread(value, PROPERTY_VALUE_MAX, 1, key);
           if (strstr(value, "trusted:") == value) {
              const char *password = &value[8];
              joinSettings.mode = NEXUS_ClientMode_eVerified;
              joinSettings.certificate.length = strlen(password);
              memcpy(joinSettings.certificate.data, password, joinSettings.certificate.length);
           }
           fclose(key);
        }

        do {
            rc = NxClient_Join(&joinSettings);
            if (rc != NEXUS_SUCCESS) {
                LOGW("%s: NxServer is not ready, waiting...", __FUNCTION__);
                usleep(NXCLIENT_SERVER_TIMEOUT_IN_MS * 1000);
            }
        } while (rc != NEXUS_SUCCESS);

        LOGI("%s: \"%s\"; joins %s mode (%d)", __FUNCTION__, joinSettings.name,
             (joinSettings.mode == NEXUS_ClientMode_eVerified) ? "VERIFIED" : "UNTRUSTED",
             joinSettings.mode);

    }

    if (rc == NEXUS_SUCCESS) {
        mJoinRefCount++;
    }
    return rc;
}

NEXUS_Error NexusNxClient::clientUninit()
{
    NEXUS_Error rc = NEXUS_SUCCESS;
    android::Mutex::Autolock autoLock(mLock);
    void *res;

    if (mJoinRefCount > 0) {
        mJoinRefCount--;
        LOGV("*** %s: decrementing join count to %d for client \"%s\". ***", __PRETTY_FUNCTION__, mJoinRefCount, getClientName());

        if (mJoinRefCount == 0) {
            LOGI("---- %s: Calling NxClient_Uninit() for client \"%s\" ----", __PRETTY_FUNCTION__, getClientName());
            NxClient_Uninit();
        }
    } else {
        LOGE("%s: NEXUS is already uninitialised!", __PRETTY_FUNCTION__);
        rc = NEXUS_NOT_INITIALIZED;
    }
    return rc;
}

NexusNxClient::NexusNxClient(const char *client_name) : NexusIPCClient(client_name)
{
    LOGV("++++++ %s: \"%s\" ++++++", __PRETTY_FUNCTION__, getClientName());
}

NexusNxClient::~NexusNxClient()
{
    LOGV("~~~~~~ %s: \"%s\" ~~~~~~", __PRETTY_FUNCTION__, getClientName());
}

NEXUS_Error NexusNxClient::standbyCheck(NEXUS_PlatformStandbyMode mode)
{
    NxClient_StandbyStatus standbyStatus;
    int count = 0;

    while (count < NXCLIENT_PM_TIMEOUT_COUNT) {
        NxClient_GetStandbyStatus(&standbyStatus);
        if (standbyStatus.settings.mode == mode && standbyStatus.standbyTransition) {
            LOGD("%s: Entered S%d", __PRETTY_FUNCTION__, mode);
            break;
        }
        else {
            usleep(NXCLIENT_PM_TIMEOUT_IN_MS * 1000 / NXCLIENT_PM_TIMEOUT_COUNT);
            count++;
        }
    }
    return (count < NXCLIENT_PM_TIMEOUT_COUNT) ? NEXUS_SUCCESS : NEXUS_TIMEOUT;
}

#ifdef ANDROID_SUPPORTS_NXCLIENT_VIDEO_WINDOW_TYPE
static const b_video_window_type videoWindowTypeConversion[] =
{
    eVideoWindowType_eMain, /* full screen capable */
    eVideoWindowType_ePip,  /* reduced size only. typically quarter screen. */
    eVideoWindowType_eNone,  /* app will do video as graphics */
    eVideoWindowType_Max
};
#endif

void NexusNxClient::getDefaultConnectClientSettings(b_refsw_client_connect_resource_settings *settings)
{
    unsigned i;
    NxClient_ConnectSettings connectSettings;

    BKNI_Memset(settings, 0, sizeof(*settings));
    NxClient_GetDefaultConnectSettings(&connectSettings);

    // Setup simple video decoder caps...
    for (i = 0; i < CLIENT_MAX_IDS, i < NXCLIENT_MAX_IDS; i++) {
        memset(&settings->simpleVideoDecoder[i].decoderCaps, 0, sizeof(settings->simpleVideoDecoder[0].decoderCaps));
        settings->simpleVideoDecoder[i].decoderCaps.fifoSize     = connectSettings.simpleVideoDecoder[i].decoderCapabilities.fifoSize;
        settings->simpleVideoDecoder[i].decoderCaps.avc51Enabled = connectSettings.simpleVideoDecoder[i].decoderCapabilities.avc51Enabled;
        settings->simpleVideoDecoder[i].decoderCaps.maxWidth     = connectSettings.simpleVideoDecoder[i].decoderCapabilities.maxWidth;
        settings->simpleVideoDecoder[i].decoderCaps.maxHeight    = connectSettings.simpleVideoDecoder[i].decoderCapabilities.maxHeight;
        settings->simpleVideoDecoder[i].decoderCaps.colorDepth   = connectSettings.simpleVideoDecoder[i].decoderCapabilities.colorDepth;

        memcpy(&settings->simpleVideoDecoder[i].decoderCaps.supportedCodecs[0],
               &connectSettings.simpleVideoDecoder[i].decoderCapabilities.supportedCodecs[0],
               sizeof(settings->simpleVideoDecoder[0].decoderCaps.supportedCodecs));

        memset(&settings->simpleVideoDecoder[i].windowCaps, 0, sizeof(settings->simpleVideoDecoder[0].windowCaps));
        settings->simpleVideoDecoder[i].windowCaps.maxWidth      = connectSettings.simpleVideoDecoder[i].windowCapabilities.maxWidth;
        settings->simpleVideoDecoder[i].windowCaps.maxHeight     = connectSettings.simpleVideoDecoder[i].windowCapabilities.maxHeight;
        settings->simpleVideoDecoder[i].windowCaps.deinterlaced  = connectSettings.simpleVideoDecoder[i].windowCapabilities.deinterlaced;
        settings->simpleVideoDecoder[i].windowCaps.encoder       = connectSettings.simpleVideoDecoder[i].windowCapabilities.encoder;
#ifdef ANDROID_SUPPORTS_NXCLIENT_VIDEO_WINDOW_TYPE
        settings->simpleVideoDecoder[i].windowCaps.type          = videoWindowTypeConversion[connectSettings.simpleVideoDecoder[i].windowCapabilities.type];
#endif
    }

    // Setup simple audio decoder caps...
    memset(&settings->simpleAudioDecoder.decoderCaps, 0, sizeof(settings->simpleAudioDecoder.decoderCaps));
    settings->simpleAudioDecoder.decoderCaps.encoder             = connectSettings.simpleAudioDecoder.decoderCapabilities.encoder;
}

/* Client side implementation of the APIs that are transferred to the server process over binder */
NexusClientContext * NexusNxClient::createClientContext(const b_refsw_client_client_configuration *config)
{
    NexusClientContext *client;

    /* Call parent class to do the Binder IPC work... */
    client = NexusIPCClient::createClientContext(config);

    if (client != NULL) {
        // Stash info away for use later
        getClientInfo(client, &this->info);

        mStandbyMonitorThread = new NexusNxClient::StandbyMonitorThread(config->standbyMonitorCallback, config->standbyMonitorContext);
        mStandbyMonitorThread->run(&config->name.string[0], ANDROID_PRIORITY_NORMAL);
    }
    return client;
}

void NexusNxClient::destroyClientContext(NexusClientContext * client)
{
    /* Cancel the standby monitor thread... */
    if (mStandbyMonitorThread != NULL && mStandbyMonitorThread->isRunning()) {
        mStandbyMonitorThread->stop();
        mStandbyMonitorThread->join();
        mStandbyMonitorThread = NULL;
    }
    /* Call parent class to do the Binder IPC work... */
    NexusIPCClient::destroyClientContext(client);
    BKNI_Memset(&this->info, 0, sizeof(this->info));
    return;
}

#ifdef ANDROID_SUPPORTS_NXCLIENT_CONFIG
void NexusNxClient::getClientComposition(NexusClientContext * client, NEXUS_SurfaceComposition *composition)
{
    if (client == NULL) {
        LOGE("%s: FATAL: Client context argument is NULL!!!", __PRETTY_FUNCTION__);
    }
    else if (composition == NULL) {
        LOGE("%s: FATAL: Client composition argument is NULL!!!", __PRETTY_FUNCTION__);
    }
    else if (client->nexusClient == NULL) {
        LOGE("%s: Invalid nexus client handle for client \"%s\"!!!", __PRETTY_FUNCTION__, client->createConfig.name.string);
    }
    else if (client->resources.videoSurface == NULL) {
        LOGE("%s: Invalid surface client handle for client \"%s\"!!!", __PRETTY_FUNCTION__, client->createConfig.name.string);
    }
    else {
        NxClient_Config_GetSurfaceClientComposition(client->nexusClient, client->resources.videoSurface, composition);
        LOGD("%s: Nexus Client \"%s\" surfaceClientId=%d, %d,%d@%d,%d", __PRETTY_FUNCTION__, client->createConfig.name.string,
            client->info.surfaceClientId, composition->position.width, composition->position.height, composition->position.x, composition->position.y);
    }
    return;
}

void NexusNxClient::setClientComposition(NexusClientContext * client, NEXUS_SurfaceComposition *composition)
{
    if (client == NULL) {
        LOGE("%s: FATAL: Client context argument is NULL!!!", __PRETTY_FUNCTION__);
        //BDBG_ASSERT(client != NULL);
    }

    if (composition == NULL) {
        LOGE("%s: FATAL: Client composition argument is NULL!!!", __PRETTY_FUNCTION__);
        //BDBG_ASSERT(composition != NULL);
    }

    if (client->nexusClient == NULL) {
        LOGE("%s: Invalid nexus client handle for client \"%s\"!!!", __PRETTY_FUNCTION__, client->createConfig.name.string);
    }
    else {
        if (client->resources.videoSurface == NULL) {
            LOGE("%s: Invalid surface client handle for client \"%s\"!!!", __PRETTY_FUNCTION__, client->createConfig.name.string);
        }
        else {
            NxClient_Config_SetSurfaceClientComposition(client->nexusClient, client->resources.videoSurface, composition);
            LOGD("%s: Nexus Client \"%s\" surfaceClientId=%d, %d,%d@%d,%d", __PRETTY_FUNCTION__, client->createConfig.name.string,
                client->info.surfaceClientId, composition->position.width, composition->position.height, composition->position.x, composition->position.y);
        }
    }
}

void NexusNxClient::getVideoWindowSettings(NexusClientContext * client, uint32_t window_id, b_video_window_settings *settings)
{
    if (client == NULL) {
        LOGE("%s: FATAL: Client context argument is NULL!!!", __PRETTY_FUNCTION__);
        //BDBG_ASSERT(client != NULL);
    }

    LOGD("%s: Client \"%s\" surfaceClientId=%d pid=%d", __PRETTY_FUNCTION__, client->createConfig.name.string, client->info.surfaceClientId, client->pid);
    return;
}

void NexusNxClient::setVideoWindowSettings(NexusClientContext * client, uint32_t window_id, b_video_window_settings *settings)
{
    if (client == NULL) {
        LOGE("%s: FATAL: Client context argument is NULL!!!", __PRETTY_FUNCTION__);
        //BDBG_ASSERT(client != NULL);
    }

    LOGD("%s: Client \"%s\" surfaceClientId=%d pid=%d", __PRETTY_FUNCTION__, client->createConfig.name.string, client->info.surfaceClientId, client->pid);

    return;
}
#endif // ANDROID_SUPPORTS_NXCLIENT_CONFIG

void NexusNxClient::getDisplaySettings(uint32_t display_id, NEXUS_DisplaySettings *settings)
{
    NEXUS_Error rc;
    NxClient_DisplaySettings nxSettings;

    // NxClient exposes only HD display
    if(display_id == 0)
    {
        rc = clientJoin();

        if (rc == NEXUS_SUCCESS) {
            NxClient_GetDisplaySettings(&nxSettings);
            settings->format = nxSettings.format;
            settings->aspectRatio = nxSettings.aspectRatio;

            clientUninit();
        }
        else {
            LOGE("%s: Could not join client \"%s\"!!!", __PRETTY_FUNCTION__, getClientName());
        }
    }
    
    return;
}

void NexusNxClient::setDisplaySettings(uint32_t display_id, NEXUS_DisplaySettings *settings)
{
    NEXUS_Error rc;
    NxClient_DisplaySettings nxSettings;

    // NxClient exposes only HD display
    if(display_id == 0)
    {
        rc = clientJoin();

        if (rc == NEXUS_SUCCESS) {
            do {
                NxClient_GetDisplaySettings(&nxSettings);
                nxSettings.format = settings->format;
                nxSettings.aspectRatio = settings->aspectRatio;
                rc = NxClient_SetDisplaySettings(&nxSettings);
            } while (rc == NXCLIENT_BAD_SEQUENCE_NUMBER);

            clientUninit();
        }
        else {
            LOGE("%s: Could not join client \"%s\"!!!", __PRETTY_FUNCTION__, getClientName());
        }
    }

    return;
}

#define AUDIO_VOLUME_SETTING_MIN (0)
#define AUDIO_VOLUME_SETTING_MAX (99)

/****************************************************
* Volume Table in dB, Mapping as linear attenuation *
****************************************************/
static uint32_t Gemini_VolTable[AUDIO_VOLUME_SETTING_MAX+1] =
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
    2602,   2796,   3046,   3398,   9000    
};

void NexusNxClient::setAudioVolume(float leftVol, float rightVol)
{
    NEXUS_Error rc;
    NxClient_AudioSettings settings;
    int32_t leftVolume;
    int32_t rightVolume;

    LOGD("nexus_nx_client %s:%d left=%f right=%f\n",__PRETTY_FUNCTION__,__LINE__,leftVol,rightVol);

    leftVolume = leftVol*AUDIO_VOLUME_SETTING_MAX;
    rightVolume = rightVol*AUDIO_VOLUME_SETTING_MAX;

    /* Check for boundary */ 
    if (leftVolume > AUDIO_VOLUME_SETTING_MAX)
        leftVolume = AUDIO_VOLUME_SETTING_MAX;
    if (leftVolume < AUDIO_VOLUME_SETTING_MIN)
        leftVolume = AUDIO_VOLUME_SETTING_MIN;
    if (rightVolume > AUDIO_VOLUME_SETTING_MAX)
        rightVolume = AUDIO_VOLUME_SETTING_MAX;
    if (rightVolume < AUDIO_VOLUME_SETTING_MIN)
        rightVolume = AUDIO_VOLUME_SETTING_MIN;

    rc = clientJoin();

    if (rc == NEXUS_SUCCESS) {
        do {
            NxClient_GetAudioSettings(&settings);
            settings.volumeType = NEXUS_AudioVolumeType_eDecibel;
            settings.leftVolume = -Gemini_VolTable[AUDIO_VOLUME_SETTING_MAX-leftVolume];
            settings.rightVolume = -Gemini_VolTable[AUDIO_VOLUME_SETTING_MAX-rightVolume];
            rc = NxClient_SetAudioSettings(&settings);
        } while (rc == NXCLIENT_BAD_SEQUENCE_NUMBER);

        clientUninit();
    }
    else {
        LOGE("%s: Could not join client \"%s\"!!!", __PRETTY_FUNCTION__, getClientName());
    }

    return;
}

bool NexusNxClient::setPowerState(b_powerState pmState)
{
    NEXUS_Error rc = NEXUS_SUCCESS;
    NxClient_StandbySettings standbySettings;

    LOGD("%s: pmState = %d",__PRETTY_FUNCTION__, pmState);

    rc = clientJoin();

    if (rc == NEXUS_SUCCESS) {
        NxClient_GetDefaultStandbySettings(&standbySettings);
        standbySettings.settings.wakeupSettings.ir = 1;
        standbySettings.settings.wakeupSettings.uhf = 1;
        standbySettings.settings.wakeupSettings.transport = 1;
        standbySettings.settings.wakeupSettings.cec = isCecEnabled(0) && isCecAutoWakeupEnabled(0);
        standbySettings.settings.wakeupSettings.gpio = 1;
        standbySettings.settings.wakeupSettings.timeout = 0;

        switch (pmState)
        {
            case ePowerState_S0:
            {
                LOGD("%s: About to set power state S0...", __PRETTY_FUNCTION__);
                standbySettings.settings.mode = NEXUS_PlatformStandbyMode_eOn;
                break;
            }

            case ePowerState_S1:
            {
                LOGD("%s: About to set power state S1...", __PRETTY_FUNCTION__);
                standbySettings.settings.mode = NEXUS_PlatformStandbyMode_eActive;
                break;
            }

            case ePowerState_S2:
            {
                LOGD("%s: About to set power state S2...", __PRETTY_FUNCTION__);
                standbySettings.settings.mode = NEXUS_PlatformStandbyMode_ePassive;
                break;
            }

            case ePowerState_S3:
            case ePowerState_S5:
            {
                LOGD("%s: About to set power state S%d...", __PRETTY_FUNCTION__, pmState-ePowerState_S0);
                standbySettings.settings.mode = NEXUS_PlatformStandbyMode_eDeepSleep;
                break;
            }

            default:
            {
                LOGE("%s: invalid power state %d!", __PRETTY_FUNCTION__, pmState);
                rc = NEXUS_INVALID_PARAMETER;
                break;
            }
        }

        if (rc == NEXUS_SUCCESS) {
            rc = NxClient_SetStandbySettings(&standbySettings);
            if (rc != NEXUS_SUCCESS) {
                LOGE("%s: NxClient_SetStandbySettings failed [rc=%d]!", __PRETTY_FUNCTION__, rc);
            }
        }

        /* Now check whether Nexus Platform has entered the desired standby mode (excluding S0)... */
        if (rc == NEXUS_SUCCESS && pmState != ePowerState_S0) {
            rc = standbyCheck(standbySettings.settings.mode);
            if (rc != NEXUS_SUCCESS) {
                LOGE("%s: standbyCheck failed [rc=%d]!", __PRETTY_FUNCTION__, rc);
            }
        }

        clientUninit();
    }
    else {
        LOGE("%s: Could not join client \"%s\"!!!", __PRETTY_FUNCTION__, getClientName());
    }

    return (rc == NEXUS_SUCCESS) ? true : false;
}

b_powerState NexusNxClient::getPowerState()
{
    NEXUS_Error rc;
    NxClient_StandbyStatus standbyStatus;
    b_powerState state = ePowerState_Max;

    rc = clientJoin();

    if (rc == NEXUS_SUCCESS) {
        rc = NxClient_GetStandbyStatus(&standbyStatus);

        if (rc == NEXUS_SUCCESS) {
            switch (standbyStatus.settings.mode)
            {
                case NEXUS_PlatformStandbyMode_eOn:
                    state = ePowerState_S0;
                    break;
                case NEXUS_PlatformStandbyMode_eActive:
                    state = ePowerState_S1;
                    break;
                case NEXUS_PlatformStandbyMode_ePassive:
                    state = ePowerState_S2;
                    break;
                case NEXUS_PlatformStandbyMode_eDeepSleep:
                    state = ePowerState_S3;
                    break;
                default:
                    state = ePowerState_Max;
            }

            if (state != ePowerState_Max) {
                LOGD("%s: Standby Status : \n"
                     "State   : S%d\n"
                     "IR      : %d\n"
                     "UHF     : %d\n"
                     "XPT     : %d\n"
                     "CEC     : %d\n"
                     "GPIO    : %d\n"
                     "Timeout : %d\n", __PRETTY_FUNCTION__,
                     state,
                     standbyStatus.status.wakeupStatus.ir, 
                     standbyStatus.status.wakeupStatus.uhf,
                     standbyStatus.status.wakeupStatus.transport,
                     standbyStatus.status.wakeupStatus.cec,
                     standbyStatus.status.wakeupStatus.gpio,
                     standbyStatus.status.wakeupStatus.timeout);
            }
        }

        clientUninit();
    }
    else {
        LOGE("%s: Could not join client \"%s\"!!!", __PRETTY_FUNCTION__, getClientName());
    }

    return state;
}

/* -----------------------------------------------------------------------------
   Trellis BPM server expects clients to acquire SimpleVideoDecoder, SimpleAudioDecoder and 
   SimpleAudioPlayback through it. An attempt to directly acquire them may fail. 
   For SBS, these API's go via RPC to Trellis BPM and return a handle. 
   For standalone Android, these are simple wrapper functions. */
NEXUS_SimpleVideoDecoderHandle NexusNxClient::acquireVideoDecoderHandle()
{
    return NEXUS_SimpleVideoDecoder_Acquire(this->info.videoDecoderId);
}

void NexusNxClient::releaseVideoDecoderHandle(NEXUS_SimpleVideoDecoderHandle handle)
{
    NEXUS_SimpleVideoDecoder_Release(handle);
}

NEXUS_SimpleAudioDecoderHandle NexusNxClient::acquireAudioDecoderHandle()
{
    return NEXUS_SimpleAudioDecoder_Acquire(this->info.audioDecoderId);
}

void NexusNxClient::releaseAudioDecoderHandle(NEXUS_SimpleAudioDecoderHandle handle)
{
    NEXUS_SimpleAudioDecoder_Release(handle);
}

NEXUS_SimpleAudioPlaybackHandle NexusNxClient::acquireAudioPlaybackHandle()
{
    return NEXUS_SimpleAudioPlayback_Acquire(this->info.audioPlaybackId);
}

void NexusNxClient::releaseAudioPlaybackHandle(NEXUS_SimpleAudioPlaybackHandle handle)
{
    NEXUS_SimpleAudioPlayback_Release(handle);
}

NEXUS_SimpleEncoderHandle NexusNxClient::acquireSimpleEncoderHandle()
{
    return NEXUS_SimpleEncoder_Acquire(this->info.encoderId);
}

void NexusNxClient::releaseSimpleEncoderHandle(NEXUS_SimpleEncoderHandle handle)
{
    NEXUS_SimpleEncoder_Release(handle);
}

void NexusNxClient::getPictureCtrlCommonSettings(uint32_t window_id, NEXUS_PictureCtrlCommonSettings *settings)
{
    NEXUS_Error rc;

    if (window_id >= MAX_VIDEO_WINDOWS_PER_DISPLAY) {
        LOGE("%s: window_id(%d) cannot be >= %d!", __PRETTY_FUNCTION__, window_id, MAX_VIDEO_WINDOWS_PER_DISPLAY);
        return;
    }

    if (settings != NULL) {
        rc = clientJoin();

        if (rc == NEXUS_SUCCESS) {
#ifdef ANDROID_SUPPORTS_NXCLIENT_PICTURE_QUALITY_SETTINGS
            NEXUS_SimpleVideoDecoderHandle simpleVideoDecoder = acquireVideoDecoderHandle();
            if (simpleVideoDecoder != NULL) {
                NEXUS_SimpleVideoDecoderPictureQualitySettings simpleVideoDecoderPictureQualitySettings;

                NEXUS_SimpleVideoDecoder_GetPictureQualitySettings( simpleVideoDecoder, &simpleVideoDecoderPictureQualitySettings);
                *settings = simpleVideoDecoderPictureQualitySettings.common;
                releaseVideoDecoderHandle(simpleVideoDecoder);
            }
            else {
                LOGE("%s: Could not acquire video decoder handle for window %d!", __PRETTY_FUNCTION__, window_id);
            }
#endif
            clientUninit();
        }
        else {
            LOGE("%s: Could not join client \"%s\"!!!", __PRETTY_FUNCTION__, getClientName());
        }
    }
    else {
        LOGE("%s: invalid parameter - \"settings\" is NULL!", __PRETTY_FUNCTION__);
    }
    return;
}

void NexusNxClient::setPictureCtrlCommonSettings(uint32_t window_id, NEXUS_PictureCtrlCommonSettings *settings)
{
    NEXUS_Error rc;

    if (window_id >= MAX_VIDEO_WINDOWS_PER_DISPLAY) {
        LOGE("%s: window_id(%d) cannot be >= %d!", __PRETTY_FUNCTION__, window_id, MAX_VIDEO_WINDOWS_PER_DISPLAY);
        return;
    }

    if (settings != NULL) {
        rc = clientJoin();

        if (rc == NEXUS_SUCCESS) {
#ifdef ANDROID_SUPPORTS_NXCLIENT_PICTURE_QUALITY_SETTINGS
            NEXUS_SimpleVideoDecoderHandle simpleVideoDecoder = acquireVideoDecoderHandle();
            if (simpleVideoDecoder != NULL) {
                NEXUS_SimpleVideoDecoderPictureQualitySettings simpleVideoDecoderPictureQualitySettings;

                NEXUS_SimpleVideoDecoder_GetPictureQualitySettings( simpleVideoDecoder, &simpleVideoDecoderPictureQualitySettings);
                simpleVideoDecoderPictureQualitySettings.common = *settings;
                NEXUS_SimpleVideoDecoder_SetPictureQualitySettings( simpleVideoDecoder, &simpleVideoDecoderPictureQualitySettings);
                releaseVideoDecoderHandle(simpleVideoDecoder);
            }
            else {
                LOGE("%s: Could not acquire video decoder handle for window %d!", __PRETTY_FUNCTION__, window_id);
            }
#endif
            clientUninit();
        }
        else {
            LOGE("%s: Could not join client \"%s\"!!!", __PRETTY_FUNCTION__, getClientName());
        }
    }
    else {
        LOGE("%s: invalid parameter - \"settings\" is NULL!", __PRETTY_FUNCTION__);
    }
    return;
}

void NexusNxClient::getGraphicsColorSettings(uint32_t display_id, NEXUS_GraphicsColorSettings *settings)
{
    NEXUS_Error rc;

    if (display_id >= 1) {  // NxClient only exposes a single display at the moment
        LOGE("%s: display_id(%d) cannot be >= %d!", __PRETTY_FUNCTION__, display_id, 1);
        return;
    }

    if (settings != NULL) {
        rc = clientJoin();

        if (rc == NEXUS_SUCCESS) {
#ifdef ANDROID_SUPPORTS_NXCLIENT_PICTURE_QUALITY_SETTINGS
            NxClient_PictureQualitySettings pictureQualitySettings;

            NxClient_GetPictureQualitySettings( &pictureQualitySettings );
            *settings = pictureQualitySettings.graphicsColor;

            clientUninit();
#endif
        }
        else {
            LOGE("%s: Could not join client \"%s\"!!!", __PRETTY_FUNCTION__, getClientName());
        }
    }
    else {
        LOGE("%s: invalid parameter - \"settings\" is NULL!", __PRETTY_FUNCTION__);
    }

    return;
}

void NexusNxClient::setGraphicsColorSettings(uint32_t display_id, NEXUS_GraphicsColorSettings *settings)
{
    NEXUS_Error rc;

    if (display_id >= 1) {  // NxClient only exposes a single display at the moment
        LOGE("%s: display_id(%d) cannot be >= %d!", __PRETTY_FUNCTION__, display_id, 1);
        return;
    }

    if (settings != NULL) {
        rc = clientJoin();

        if (rc == NEXUS_SUCCESS) {
#ifdef ANDROID_SUPPORTS_NXCLIENT_PICTURE_QUALITY_SETTINGS
            NxClient_PictureQualitySettings pictureQualitySettings;

            NxClient_GetPictureQualitySettings( &pictureQualitySettings );
            pictureQualitySettings.graphicsColor = *settings;
            NxClient_SetPictureQualitySettings( &pictureQualitySettings );
#endif
            clientUninit();
        }
        else {
            LOGE("%s: Could not join client \"%s\"!!!", __PRETTY_FUNCTION__, getClientName());
        }
    }
    else {
        LOGE("%s: invalid parameter - \"settings\" is NULL!", __PRETTY_FUNCTION__);
    }
    return;
}

void NexusNxClient::setDisplayOutputs(int display)
{
    NEXUS_Error rc;
    NxClient_DisplaySettings nxSettings;

    // NxClient exposes only HD display
    rc = clientJoin();

    if (rc == NEXUS_SUCCESS) {
        do {
            NxClient_GetDisplaySettings(&nxSettings);
#ifdef ANDROID_SUPPORTS_NXCLIENT_EXTENDED_DISPLAY_SETTINGS
            nxSettings.hdmiPreferences.enabled = display;
            nxSettings.componentPreferences.enabled = display;
            nxSettings.compositePreferences.enabled = display;
#endif
            rc = NxClient_SetDisplaySettings(&nxSettings);
        } while (rc == NXCLIENT_BAD_SEQUENCE_NUMBER);

        clientUninit();
    }
    else {
        LOGE("%s: Could not join client \"%s\"!!!", __PRETTY_FUNCTION__, getClientName());
    }
    return;
}

void NexusNxClient::setAudioMute(int mute)
{
    NEXUS_Error rc;
    NxClient_AudioSettings settings;

    rc = clientJoin();

    if (rc == NEXUS_SUCCESS) {
        do {
            NxClient_GetAudioSettings(&settings);
            settings.muted = mute;
            rc = NxClient_SetAudioSettings(&settings);
        } while (rc == NXCLIENT_BAD_SEQUENCE_NUMBER);

        clientUninit();
    }
    return;
}

bool NexusNxClient::getHdmiOutputStatus(uint32_t portId, b_hdmiOutputStatus *pHdmiOutputStatus)
{
    NEXUS_Error rc = NEXUS_NOT_SUPPORTED;
#ifdef ANDROID_SUPPORTS_NXCLIENT_HDMI_STATUS
#if NEXUS_HAS_HDMI_OUTPUT
    if (portId < NEXUS_NUM_HDMI_OUTPUTS) {
        unsigned loops;
        NxClient_DisplayStatus status;

        memset(pHdmiOutputStatus, 0, sizeof(*pHdmiOutputStatus));
        
        for (loops = 0; loops < 4; loops++) {
            rc = NxClient_GetDisplayStatus(&status);
            if ((rc == NEXUS_SUCCESS) && status.hdmi.status.connected) {
                break;
            }
            LOGV("%s: Waiting for HDMI%d output to be connected...", __PRETTY_FUNCTION__, portId);
            usleep(250 * 1000);
        }
        
        if (rc == NEXUS_SUCCESS) {
            if (status.hdmi.status.connected) {
                pHdmiOutputStatus->connected            = status.hdmi.status.connected;
                pHdmiOutputStatus->rxPowered            = status.hdmi.status.rxPowered;
                pHdmiOutputStatus->hdmiDevice           = status.hdmi.status.hdmiDevice;
                pHdmiOutputStatus->videoFormat          = status.hdmi.status.videoFormat;
                pHdmiOutputStatus->preferredVideoFormat = status.hdmi.status.preferredVideoFormat;
                pHdmiOutputStatus->aspectRatio          = status.hdmi.status.aspectRatio;
                pHdmiOutputStatus->colorSpace           = status.hdmi.status.colorSpace;
                pHdmiOutputStatus->audioFormat          = status.hdmi.status.audioFormat;
                pHdmiOutputStatus->audioSamplingRate    = status.hdmi.status.audioSamplingRate;
                pHdmiOutputStatus->audioSamplingSize    = status.hdmi.status.audioSamplingSize;
                pHdmiOutputStatus->audioChannelCount    = status.hdmi.status.audioChannelCount;
                pHdmiOutputStatus->physicalAddress[0]   = status.hdmi.status.physicalAddressA << 4 | status.hdmi.status.physicalAddressB;
                pHdmiOutputStatus->physicalAddress[1]   = status.hdmi.status.physicalAddressC << 4 | status.hdmi.status.physicalAddressD;
            }
        }
        else {
            LOGE("%s: Could not get HDMI%d output status!!!", __PRETTY_FUNCTION__, portId);
        }
    }
    else
#endif
    {
        LOGE("%s: No HDMI%d output on this device!!!", __PRETTY_FUNCTION__, portId);
    }
#else
#warning Reference software does not support obtaining HDMI output status in NxClient mode
    rc = NEXUS_SUCCESS;
#endif
    return (rc == NEXUS_SUCCESS);
}

