/*
 * Copyright 2014 The Android Open Source Project
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

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>

#define LOG_TAG "hdmi_tv_input"
//#define LOG_NDEBUG 0
#include <cutils/log.h>
#include <cutils/native_handle.h>
#include <cutils/properties.h>

#include <hardware/tv_input.h>

#include "nxclient.h"
#include "nexus_surface_client.h"
#include "nexus_simple_video_decoder.h"
#include "nexus_simple_audio_decoder.h"
#include "nexus_hdmi_input.h"
#include "nexus_hdmi_input_hdcp.h"
#include "nexus_hdmi_output_hdcp.h"
#include "nexus_display.h"
#include <bcmsideband.h>

/* This module was designed to look a lot like a standalone tv_input HAL. The
 * only difference is that we piggy-back it on top of the TV tuner HAL.
 */

#define DEVICE_ID_HDMI 1
#define STREAM_ID_HDMI 1
#define HDMI_PORT_ID 1 /* 1-based for Android, 0-based for Nexus */

/*****************************************************************************/

typedef struct tv_input_private {
    tv_input_device_t device;

    // Callback related data
    const tv_input_callback_ops_t* callback;
    void* callback_data;
    struct tv_input_device* callback_dev;

    tv_stream_config_t stream_config;

    NxClient_AllocResults allocResults;
    NEXUS_SimpleVideoDecoderHandle videoDecoder;
    NEXUS_SimpleAudioDecoderHandle audioDecoder;
    NEXUS_SurfaceClientHandle surfaceClient, videoSurfaceClient;
    unsigned connectId;
    bool downStreamAuth;
    bool upStreamAuth;
    NEXUS_HdmiInputHandle hdmiInput;
    NEXUS_HdmiOutputHandle hdmiOutput;
    struct bcmsideband_ctx *sidebandContext;
} tv_input_private_t;

static int tv_input_device_open(const struct hw_module_t* module,
        const char* name, struct hw_device_t** device);

static struct hw_module_methods_t tv_input_module_methods = {
    .open = tv_input_device_open
};

tv_input_module_t hdmi_module /*HAL_MODULE_INFO_SYM*/ = {
   .common = {
      .tag                = HARDWARE_MODULE_TAG,
      .module_api_version = TV_INPUT_MODULE_API_VERSION_0_1,
      .hal_api_version    = HARDWARE_HAL_API_VERSION,
      .id                 = TV_INPUT_HARDWARE_MODULE_ID,
      .name               = "hdmi_tv_input for set-top-box platforms",
      .author             = "Broadcom",
      .methods            = &tv_input_module_methods,
      .dso                = 0,
      .reserved           = {0},
   }
};

/*****************************************************************************/

static NEXUS_Error initializeHdmiInputHdcpSettings(tv_input_private_t* priv)
{
    ALOGV("%s", __FUNCTION__);
    NEXUS_HdmiInputHdcpKeyset hdmiRxKeyset;

    int rc = 0;
    int fileFd;
    uint8_t *buffer = NULL;
    NEXUS_Error errCode = NEXUS_SUCCESS;
    NEXUS_HdmiInputHdcpStatus hdcpStatus;
    size_t fileSize;
    off_t seekPos;
    static const unsigned char hdcp1xHeader[] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
    char tmp[8];

    ALOGD("Read HDCP 1.x Rx Keys");
    fileFd = open("/hwcfg/drm_hdcp1x.bin", O_RDONLY);
    if (fileFd < 0) {
        ALOGE("Open error %d for HDCP 1.x Rx Key file", fileFd);
        return NEXUS_UNKNOWN;
    }

    read(fileFd, tmp, 8);
    if (memcmp(hdcp1xHeader, tmp, 8))
        ALOGW("Invalid Header in HDCP 1.x Rx Key file");

    read(fileFd, tmp, 1);
    read(fileFd, tmp, 1);
    read(fileFd, (uint8_t *) &hdmiRxKeyset.alg, 1);

    read(fileFd, &hdmiRxKeyset.custKeyVarL, 1);
    read(fileFd, &hdmiRxKeyset.custKeyVarH, 1);
    read(fileFd, &hdmiRxKeyset.custKeySel, 1);

    read(fileFd, hdmiRxKeyset.rxBksv.data, NEXUS_HDMI_HDCP_KSV_LENGTH);
    read(fileFd, tmp, 3);

    read(fileFd, &hdmiRxKeyset.privateKey, NEXUS_HDMI_HDCP_NUM_KEYS * sizeof(NEXUS_HdmiInputHdcpKey));
    close(fileFd);

    errCode = NEXUS_HdmiInput_HdcpSetKeyset(priv->hdmiInput, &hdmiRxKeyset);
    if (errCode) {
        ALOGE("NEXUS_HdmiInput_HdcpSetKeyset failed (%d)", errCode);
        return NEXUS_UNKNOWN;
    }

    NEXUS_HdmiInput_HdcpGetStatus(priv->hdmiInput, &hdcpStatus);

    /* display message informing of result of HDCP Key Load */
    /* NOTE: use of otpState is overloaded... refers to status of key load */
    if (hdcpStatus.eOtpState != NEXUS_HdmiInputHdcpKeySetOtpState_eCrcMatch)
        ALOGE("NEXUS_HdmiInput_HdcpSetKeyset failed (%d)", errCode);
    else
        ALOGI("NEXUS_HdmiInput_HdcpSetKeyset succeeded");


    fileFd = open("/hwcfg/drm.bin", O_RDONLY);
    if (fileFd < 0) {
        ALOGE("Open error %d for HDCP 2.x Rx Key file", fileFd);
        return NEXUS_UNKNOWN;
    }

    seekPos = lseek(fileFd, 0, SEEK_END);
    if (seekPos < 0) {
        ALOGE("Unable to seek bin file size");
        close(fileFd);
        return 2;
    }
    fileSize = (size_t)seekPos;

    if (lseek(fileFd, 0, SEEK_SET) < 0) {
        ALOGE("Unable to get back to origin");
        close(fileFd);
        return 3;
    }

    buffer = new uint8_t[fileSize];
    if (read(fileFd, (void *)buffer, fileSize) != (ssize_t)fileSize)
    {
        ALOGE("Unable to read all binfile");
        delete buffer;
        close(fileFd);
        return 6;
    }

    ALOGD("drm.bin file loaded buff=%p, size=%u", buffer, (unsigned)fileSize);
    errCode = NEXUS_HdmiInput_SetHdcp2xBinKeys(priv->hdmiInput, buffer, (uint32_t)fileSize);
    if (errCode != NEXUS_SUCCESS) {
        ALOGE("Error setting Hdcp2x encrypted keys. HDCP2.x will not work.");
        delete buffer;
        close(fileFd);
        return errCode;
    }

    ALOGI("HDCP 2.2 Key Loading: SUCCESS");
    delete buffer;
    close(fileFd);
    return 0;
}

/* changing output params to match input params is not required */
static void source_changed(void *context, int)
{
    ALOGV("%s", __FUNCTION__);
    tv_input_private_t* priv = (tv_input_private_t*)context;
    NEXUS_Error rc;
    NEXUS_HdmiInputStatus hdmiInputStatus;
    NxClient_DisplaySettings displaySettings;

    NxClient_GetDisplaySettings(&displaySettings);
    NEXUS_HdmiInput_GetStatus(priv->hdmiInput, &hdmiInputStatus);
    if (!hdmiInputStatus.validHdmiStatus) {
        displaySettings.hdmiPreferences.hdcp = NxClient_HdcpLevel_eNone;
    } else {
        NEXUS_HdmiOutputStatus hdmiOutputStatus;
        NEXUS_HdmiOutput_GetStatus(priv->hdmiOutput, &hdmiOutputStatus);
        if (displaySettings.format != hdmiInputStatus.originalFormat &&
            hdmiOutputStatus.videoFormatSupported[hdmiInputStatus.originalFormat]) {
            ALOGW("video format %d to %d", displaySettings.format, hdmiInputStatus.originalFormat);
            displaySettings.format = hdmiInputStatus.originalFormat;
        }

        if (hdmiInputStatus.colorSpace != displaySettings.hdmiPreferences.colorSpace) {
            ALOGW("color space %d -> %d", displaySettings.hdmiPreferences.colorSpace,
                hdmiInputStatus.colorSpace);
            displaySettings.hdmiPreferences.colorSpace = hdmiInputStatus.colorSpace;
        }
        if (hdmiInputStatus.colorDepth != displaySettings.hdmiPreferences.colorDepth) {
            ALOGW("color depth %u -> %u", displaySettings.hdmiPreferences.colorDepth, hdmiInputStatus.colorDepth);
            displaySettings.hdmiPreferences.colorDepth = hdmiInputStatus.colorDepth;
        }
    }
    rc = NxClient_SetDisplaySettings(&displaySettings);
    if (rc) {
        ALOGE("Unable to set Display Settings (%d)", rc);
    }
}

static void hdmiRxHdcpStateChanged(void *context, int)
{
    ALOGV("%s", __FUNCTION__);
    tv_input_private_t* priv = (tv_input_private_t*)context;
    NEXUS_HdmiInputHdcpStatus hdmiRxHdcpStatus;
    NEXUS_HdmiOutputHdcpStatus hdmiTxHdcpStatus;
    NxClient_HdcpVersion hdcpVersion;
    bool rxAuthenticated = false;
    bool repeaterAuthenticated;
    NEXUS_Error rc;

    /* check the authentication state and process accordingly */

    /***********************/
    /* HDMI Rx HDCP status */
    /***********************/
    rc = NEXUS_HdmiInput_HdcpGetStatus(priv->hdmiInput, &hdmiRxHdcpStatus);
    if (rc) {
        ALOGE("%s: Error getting Rx HDCP status (%d)", __FUNCTION__, rc);
        return;
    }

    /***********************/
    /* HDMI Tx HDCP status */
    /***********************/
    rc = NEXUS_HdmiOutput_GetHdcpStatus(priv->hdmiOutput, &hdmiTxHdcpStatus);
    if (rc) {
        ALOGE("%s: Error getting Tx HDCP status (%d)", __FUNCTION__, rc);
        return;
    }

    /**************************************/
    /*  Rx HDCP 2.x Authentication Status */
    /**************************************/
    if (hdmiRxHdcpStatus.version == NEXUS_HdcpVersion_e2x) {
        rxAuthenticated =
            hdmiRxHdcpStatus.hdcpState == NEXUS_HdmiInputHdcpState_eAuthenticated;
        repeaterAuthenticated =
            hdmiRxHdcpStatus.hdcpState == NEXUS_HdmiInputHdcpState_eRepeaterAuthenticated;

        ALOGD("%s: [-->Rx-STB-Tx] HDCP Ver: %s; Content Stream Protection: %d; Upstream Status: %s Authenticated ",
            __FUNCTION__,
            (hdmiRxHdcpStatus.version == NEXUS_HdcpVersion_e2x) ? "2.2" : "1.x",
            hdmiRxHdcpStatus.hdcp2xContentStreamControl,
            rxAuthenticated ? "Rx" :
            repeaterAuthenticated ? "Repeater" : "FAILED");

        ALOGD("%s: [Rx-STB-Tx-->] HDCP 2.2 support downstream: %s; HDCP 1.x device downstream: %s",
            __FUNCTION__,
            hdmiTxHdcpStatus.hdcp2_2Features ? "Yes" : "No",
            !hdmiTxHdcpStatus.hdcp2_2Features ? "Yes" :
                hdmiTxHdcpStatus.hdcp2_2RxInfo.hdcp1_xDeviceDownstream ? "Yes" : "No");

        if ((hdmiRxHdcpStatus.hdcpState != NEXUS_HdmiInputHdcpState_eAuthenticated) &&
            (hdmiRxHdcpStatus.hdcpState != NEXUS_HdmiInputHdcpState_eRepeaterAuthenticated)) {
            ALOGW("%s: HDCP2.2 Auth from upstream Tx: FAILED", __FUNCTION__);
            return;
        }

        priv->upStreamAuth = true;

        if (hdmiRxHdcpStatus.hdcp2xContentStreamControl == NEXUS_Hdcp2xContentStream_eType1) {
            /* upstream auth requires HDCP 2.2; Start Tx auth only if connected to HDCP 2.2 device */
            ALOGD("Upstream HDCP Content Stream Control is Type 1");
            if (!hdmiTxHdcpStatus.hdcp2_2Features) {
               /*** Rx does not support HDCP 2.2; CONTENT MUST BE BLOCKED ***/
                ALOGW("Attached device does not support HDCP 2.2; Downstream authentication blocked");
                return;
            }

            if (hdmiTxHdcpStatus.hdcp2_2RxInfo.hdcp1_xDeviceDownstream) {
                /* upstream auth also requires no HDCP 1.x devices downstream; CONTENT MUST BE BLOCKED ***/
                ALOGW("Attached 1.x devices downstream;  Downstream authentication blocked");
                return;
            }
        }
    }
    /*******************************/
    /*  Rx HDCP 1.x Authentication */
    /*******************************/
    else
    {
        hdcpVersion = NxClient_HdcpVersion_eHdcp1x;

        ALOGV("%s: HDCP Authentication State: %d",
            __FUNCTION__, hdmiRxHdcpStatus.eAuthState);

        switch (hdmiRxHdcpStatus.eAuthState) {
        case NEXUS_HdmiInputHdcpAuthState_eKeysetInitialization:
            ALOGD("%s Change in HDCP Key Set detected: %u",
                __FUNCTION__, hdmiRxHdcpStatus.eKeyStorage);
            break;
        case NEXUS_HdmiInputHdcpAuthState_eWaitForKeyloading:
            ALOGD("%s: Upstream HDCP 1.x Authentication request ...", __FUNCTION__);
            break;
        case NEXUS_HdmiInputHdcpAuthState_eWaitForDownstreamKsvs:
            /* Repeater Rx is considered authenticated when upstream requests KSvs */
            ALOGD("%s KSV FIFO Request; Start hdmiOutput Authentication...", __FUNCTION__);
            rxAuthenticated = true;
            break;
        case NEXUS_HdmiInputHdcpAuthState_eKsvFifoReady:
            ALOGD("%s KSV FIFO Ready...", __FUNCTION__);
            break;
        case NEXUS_HdmiInputHdcpAuthState_eIdle:
            ALOGV("%s: Auth State: Idle...", __FUNCTION__);
            return;
        default:
            ALOGW("%s: Unknown State: %d", __FUNCTION__,  hdmiRxHdcpStatus.eAuthState);
            return;
        }
    }

    ALOGD("RxAuthenticated: %d", rxAuthenticated);
    if (rxAuthenticated) {
        ALOGD("%s: Repeater's Rx is authenticated. Start Repeater's Tx downstream authentication",
            __FUNCTION__);

        rc = NxClient_SetHdmiInputRepeater(priv->hdmiInput);
        if (rc != NEXUS_SUCCESS) {
            ALOGE("%s: Error %d starting HDCP downstream authentication",
                    __FUNCTION__, rc);
        }
    } else if (repeaterAuthenticated) {
        ALOGD("%s: Repeater Tx downstream authentication completed", __FUNCTION__);
    } else {
        /* TODO - report current state */
    }
}

static void notify_stream_changed(tv_input_private_t* priv)
{
    tv_input_event_t tuner_event;
    tuner_event.type = TV_INPUT_EVENT_STREAM_CONFIGURATIONS_CHANGED;
    tuner_event.device_info.device_id = DEVICE_ID_HDMI;
    tuner_event.device_info.audio_type = AUDIO_DEVICE_NONE;
    tuner_event.device_info.audio_address = NULL;
    priv->callback->notify(priv->callback_dev, &tuner_event, priv->callback_data);
}

static void hdmiTxHotplugCallback(void *context, int)
{
    ALOGV("%s", __FUNCTION__);
    tv_input_private_t* priv = (tv_input_private_t*)context;
    NEXUS_HdmiOutputStatus hdmiTxStatus;
    NEXUS_HdmiOutputBasicEdidData hdmiOutputBasicEdidData;
    NEXUS_HdmiOutputEdidBlock edidBlock;
    uint8_t *attachedRxEdid = NULL;
    uint16_t attachedRxEdidSize = 0;
    unsigned i, j;
    NEXUS_Error rc;

    NEXUS_HdmiOutput_GetStatus(priv->hdmiOutput, &hdmiTxStatus);
    priv->downStreamAuth = false;
    priv->upStreamAuth = false;

    if (!hdmiTxStatus.connected) {
        /* device disconnected. Load internal EDID. */
        rc = NEXUS_HdmiInput_LoadEdidData(priv->hdmiInput, attachedRxEdid, attachedRxEdidSize);
        if (rc)
            ALOGE("NEXUS_HdmiInput_LoadEdidData returned %d", rc);
        goto done;
    }

    /* Get EDID of attached receiver */
    rc = NEXUS_HdmiOutput_GetBasicEdidData(priv->hdmiOutput, &hdmiOutputBasicEdidData);
    if (rc) {
        ALOGE("Unable to get downstream EDID; Use declared EDID in app for repeater's EDID (%d)", rc);
        goto load_edid;
    }

    /* allocate space to hold the EDID blocks */
    attachedRxEdidSize = (hdmiOutputBasicEdidData.extensions + 1) * sizeof(edidBlock.data);
    attachedRxEdid = new uint8_t[attachedRxEdidSize];
    if (!attachedRxEdid) {
        ALOGE("Unable to allocate space for EDID blocks");
        attachedRxEdid = NULL;
        attachedRxEdidSize = 0;
        goto load_edid;
    }

    /* copy Attached EDID for presentation to the upstream Tx */
    for (i = 0; i <= hdmiOutputBasicEdidData.extensions; i++) {
        rc = NEXUS_HdmiOutput_GetEdidBlock(priv->hdmiOutput, i, &edidBlock);
        if (rc) {
            ALOGE("Error retrieving EDID Block %d from attached receiver;", i);
            attachedRxEdidSize = 0;
            goto load_edid;
        }

        for (j=0; j < sizeof(edidBlock.data); j++) {
            attachedRxEdid[i*sizeof(edidBlock.data)+j] = edidBlock.data[j];
        }
    }

load_edid:
    /* TODO: manipulate EDID to add/remove capabilities */
    rc = NEXUS_HdmiInput_LoadEdidData(priv->hdmiInput, attachedRxEdid, attachedRxEdidSize);
    if (rc) {
        ALOGE("Unable to load EDID data");
    }
    if (attachedRxEdid) {
        delete attachedRxEdid;
    }

    if (hdmiTxStatus.rxPowered) {
        rc = initializeHdmiInputHdcpSettings(priv);
        if (rc != NEXUS_SUCCESS) {
            ALOGE("%s: Error InitializeHdmiInputHdcpSettings (%d)", __FUNCTION__, rc);
        }
    }

done:
    ALOGD("%s: device %s; toggle Rx HPD to notify upstream device...",
        __FUNCTION__, hdmiTxStatus.connected ? "connected" : "removed");
    NEXUS_HdmiInput_ToggleHotPlug(priv->hdmiInput);
}

static void retryAuthentication(tv_input_private_t* priv, bool rxAuthenticated)
{
    /* check the authentication state and process accordingly */
    NEXUS_HdmiInputHdcpStatus hdmiRxHdcpStatus;
    NEXUS_HdmiInput_HdcpGetStatus(priv->hdmiInput, &hdmiRxHdcpStatus);
    NEXUS_Error rc;

    ALOGD("%s: version[%d], state [%d]", __FUNCTION__,
          hdmiRxHdcpStatus.version, hdmiRxHdcpStatus.hdcpState);

    if (hdmiRxHdcpStatus.hdcpState == NEXUS_HdmiInputHdcpState_eAuthenticated) {
        ALOGD("%s: HDCP2.2 Authentication status from with upstream Tx: AUTHENTICATED", __FUNCTION__);
        if (!rxAuthenticated) {
            /* Authenticate downstream device */
            /* TODO: New API/setting to force authentication */
            ALOGD("%s: ReSTART Downtream Authentication", __FUNCTION__);
            rc = NxClient_SetHdmiInputRepeater(priv->hdmiInput);
            if (rc) {
                ALOGE("NxClient_SetHdmiInputRepeater returned %d", rc);
            }
        }
    }
}

static void hdmiTxHdcpStateChanged(void *context, int)
{
    ALOGV("%s", __FUNCTION__);
    tv_input_private_t* priv = (tv_input_private_t*)context;
    NEXUS_Error rc;
    NEXUS_HdmiOutputHdcpStatus hdmiTxHdcpStatus;
    NEXUS_HdmiOutputHdcpSettings hdmiTxHdcpSettings;
    NEXUS_HdmiHdcpDownStreamInfo downStream;
    NEXUS_HdmiHdcpKsv *pKsvs;
    unsigned returnedDevices;
    bool rxAuthenticated = false;
    uint8_t i;

    rc = NEXUS_HdmiOutput_GetHdcpStatus(priv->hdmiOutput, &hdmiTxHdcpStatus);
    if (rc) {
        ALOGE("Unable to get HDMI output HDCP status");
        return;
    }

    if (hdmiTxHdcpStatus.hdcpState == NEXUS_HdmiOutputHdcpState_eUnpowered) {
        ALOGW("%s: Attached Device is unpowered", __FUNCTION__);
        return;
    }

    if ((hdmiTxHdcpStatus.hdcpError == NEXUS_HdmiOutputHdcpError_eSuccess) &&
        (hdmiTxHdcpStatus.linkReadyForEncryption || hdmiTxHdcpStatus.transmittingEncrypted)) {
        ALOGD("%s: Hdcp Auth with downstream device: Successful",  __FUNCTION__);
        rxAuthenticated = true;
        priv->downStreamAuth = true;
    } else if (hdmiTxHdcpStatus.hdcpState == NEXUS_HdmiOutputHdcpState_eUnauthenticated) {
        /* as a repeater, wait to be told to re-authenticate */
        ALOGD("%s: Tx is unauthenticated; wait for upstream authentication request; upstream authenticated? %s",
        __FUNCTION__, priv->upStreamAuth ? "Yes" : "No");
        priv->downStreamAuth = false;
        if (priv->upStreamAuth) {
            ALOGD("Upstream is already authenticated; retry downstream auth");
            retryAuthentication(priv, rxAuthenticated);
        } else {
            ALOGD("Waiting for upstream authentication request");
            return;
        }
    } else {
        ALOGE("%s: Hdcp Auth with downstream device: FAILED;  HDCP State: %d, last HDCP error: %d",
            __FUNCTION__,
            hdmiTxHdcpStatus.hdcpState, hdmiTxHdcpStatus.hdcpError);

        switch (hdmiTxHdcpStatus.hdcpError) {
        case NEXUS_HdmiOutputHdcpError_eRxDevicesExceeded:
        case NEXUS_HdmiOutputHdcpError_eRepeaterDepthExceeded:
        case NEXUS_HdmiOutputHdcpError_eRepeaterAuthenticationError:
            /* upload error information */
            break;
        default:
            retryAuthentication(priv, rxAuthenticated);
            break;
        }
    }

uploadDownstreamInfo:
    ALOGD("%s Uploading downstream info...", __FUNCTION__);

    /* Load Rx KSV FIFO for upstream device */
    NEXUS_HdmiOutput_GetHdcpSettings(priv->hdmiOutput, &hdmiTxHdcpSettings);

    /* If HDCP 2.2 version was selected or AUTO mode was selected AND the Rx support HDCP 2.2 */
    if (hdmiTxHdcpStatus.hdcp2_2Features &&
        ((hdmiTxHdcpSettings.hdcp_version == NEXUS_HdmiOutputHdcpVersion_e2_2) ||
         (hdmiTxHdcpSettings.hdcp_version == NEXUS_HdmiOutputHdcpVersion_eAuto))) {
    } else {
        /* HDCP 1.x */
        NEXUS_HdmiOutput_HdcpGetDownstreamInfo(priv->hdmiOutput, &downStream);

        /* allocate space to hold ksvs for the downstream devices */
        pKsvs = new NEXUS_HdmiHdcpKsv[downStream.devices];
        rc = NEXUS_HdmiOutput_HdcpGetDownstreamKsvs(priv->hdmiOutput,
            pKsvs, downStream.devices, &returnedDevices);
        if (rc)
            ALOGE("NEXUS_HdmiOutput_HdcpGetDownstreamKsvs returned %d", rc);

        ALOGD("hdmiOutput Downstream Levels:  %d  Devices: %d",
            downStream.depth, downStream.devices);

        /* display the downstream device KSVs */
        for (i = 0; i < downStream.devices; i++) {
            ALOGD("Device %02d BKsv: %02X %02X %02X %02X %02X",
                i + 1,
                *(pKsvs->data + i + 4), *(pKsvs->data + i + 3),
                *(pKsvs->data + i + 2), *(pKsvs->data + i + 1),
                *(pKsvs->data + i));
        }

        rc = NEXUS_HdmiInput_HdcpLoadKsvFifo(priv->hdmiInput,
            &downStream, pKsvs, downStream.devices);
        if (rc)
            ALOGE("NEXUS_HdmiInput_HdcpLoadKsvFifo returned %d", rc);

        delete pKsvs;
    }
    /* Dowstream device IDs/KSVs have been uploaded */
    return;
}

static void sb_geometry_update(void *context, unsigned int x, unsigned int y,
    unsigned int width, unsigned int height)
{
    ALOGV("%s", __FUNCTION__);

    tv_input_private_t* priv = (tv_input_private_t*)context;

    NEXUS_SurfaceComposition comp;
    NxClient_GetSurfaceClientComposition(priv->allocResults.surfaceClient[0].id, &comp);
    comp.position.x = x;
    comp.position.y = y;
    comp.position.width = width;
    comp.position.height = height;
    NEXUS_Error rc;
    rc = NxClient_SetSurfaceClientComposition(priv->allocResults.surfaceClient[0].id, &comp);
    if (rc) {
        ALOGE("Failed to apply new composition (%d)", rc);
    }
}

/*****************************************************************************/

static int tv_input_initialize(struct tv_input_device* dev,
        const tv_input_callback_ops_t* callback, void* data)
{
    int err;

    ALOGV("%s: dev=%p, callback=%p, data=%p", __FUNCTION__, dev, callback, data);
    if (dev == NULL || callback == NULL) {
        return -EINVAL;
    }
    tv_input_private_t* priv = (tv_input_private_t*)dev;
    if (priv->callback != NULL) {
        return -EEXIST;
    }

    NEXUS_Error rc;
    NxClient_JoinSettings joinSettings;
    NxClient_GetDefaultJoinSettings(&joinSettings);
    rc = NxClient_Join(&joinSettings);
    if (rc) {
        printf("cannot join: %d\n", rc);
        err = -ENODEV;
        goto failed_join;
    }

    NEXUS_HdmiInputSettings hdmiInputSettings;
    NEXUS_HdmiInput_GetDefaultSettings(&hdmiInputSettings);
    hdmiInputSettings.frontend.hpdDisconnected = false;
    hdmiInputSettings.secureVideo = false;
    hdmiInputSettings.sourceChanged.callback = source_changed;
    hdmiInputSettings.sourceChanged.context = priv;
    priv->hdmiInput = NEXUS_HdmiInput_Open(HDMI_PORT_ID-1, &hdmiInputSettings);
    if (!priv->hdmiInput) {
        ALOGE("HdmiInput %d not available", HDMI_PORT_ID-1);
        err = -ENODEV;
        goto failed_hdmiinput_open;
    }

    /* open a read-only alias to get EDID. any changes must go through nxclient. */
    priv->hdmiOutput = NEXUS_HdmiOutput_Open(0 + NEXUS_ALIAS_ID, NULL);
    if (!priv->hdmiOutput) {
        ALOGE("Can't get hdmi output read-only alias");
        err = -ENODEV;
        goto failed_hdmioutput_open;
    }

    initializeHdmiInputHdcpSettings(priv);

    NEXUS_HdmiInput_GetSettings(priv->hdmiInput, &hdmiInputSettings);
    hdmiInputSettings.sourceChanged.callback = source_changed;
    hdmiInputSettings.sourceChanged.context = priv;
    rc = NEXUS_HdmiInput_SetSettings(priv->hdmiInput, &hdmiInputSettings);
    if (rc) {
        ALOGE("NEXUS_HdmiInput_SetSettings failed (%d)", rc);
        err = -ENODEV;
        goto failed_hdmiinput_source;
    }

    NEXUS_HdmiInputHdcpSettings hdmiInputHdcpSettings;
    NEXUS_HdmiInput_HdcpGetDefaultSettings(priv->hdmiInput, &hdmiInputHdcpSettings);
    /* chips with both hdmi rx and tx cores should always set repeater to TRUE */
    hdmiInputHdcpSettings.repeater = true;
    hdmiInputHdcpSettings.hdcpRxChanged.callback = hdmiRxHdcpStateChanged;
    hdmiInputHdcpSettings.hdcpRxChanged.context = priv;
    rc = NEXUS_HdmiInput_HdcpSetSettings(priv->hdmiInput, &hdmiInputHdcpSettings);
    if (rc) {
        ALOGE("Can't set hdmi input HDCP settings (%d)", rc);
        err = -ENODEV;
        goto failed_hdmiinput_hdcp;
    }

    NxClient_CallbackThreadSettings callbackThreadSettings;
    NxClient_GetDefaultCallbackThreadSettings(&callbackThreadSettings);
    callbackThreadSettings.hdmiOutputHotplug.callback = hdmiTxHotplugCallback;
    callbackThreadSettings.hdmiOutputHotplug.context = priv;
    callbackThreadSettings.hdmiOutputHdcpChanged.callback = hdmiTxHdcpStateChanged;
    callbackThreadSettings.hdmiOutputHdcpChanged.context = priv;
    rc = NxClient_StartCallbackThread(&callbackThreadSettings);
    if (rc) {
        ALOGE("Can't set callback thread settings (%d)", rc);
        err = -ENODEV;
        goto failed_callback_settings;
    }

    priv->sidebandContext = NULL;
    priv->callback = callback;
    priv->callback_data = data;

    tv_input_event_t tuner_event;
    tuner_event.type = TV_INPUT_EVENT_DEVICE_AVAILABLE;
    tuner_event.device_info.type = TV_INPUT_TYPE_HDMI;
    tuner_event.device_info.device_id = DEVICE_ID_HDMI;
    tuner_event.device_info.hdmi.port_id = HDMI_PORT_ID;
    tuner_event.device_info.audio_type = AUDIO_DEVICE_NONE;
    tuner_event.device_info.audio_address = NULL;
    priv->callback->notify(priv->callback_dev, &tuner_event, priv->callback_data);

    return 0;

failed_callback_settings:
failed_hdmiinput_hdcp:
failed_hdmiinput_source:
    NEXUS_HdmiOutput_Close(priv->hdmiOutput);
failed_hdmioutput_open:
    NEXUS_HdmiInput_Close(priv->hdmiInput);
failed_hdmiinput_open:
    NxClient_Uninit();
failed_join:
    libbcmsideband_release(priv->sidebandContext);
    priv->sidebandContext = NULL;
failed_sideband_init:
    return err;
}

static int tv_input_get_stream_configurations(
        const struct tv_input_device* dev, int device_id,
        int* num_configurations, const tv_stream_config_t** configs)
{
    ALOGV("%s: dev=%p, device_id=%d, num_configurations=%p configs=%p",
          __FUNCTION__, dev, device_id, num_configurations, configs);
    if (dev == NULL || device_id != DEVICE_ID_HDMI ||
        num_configurations == NULL || configs == NULL) {
        return -EINVAL;
    }
    tv_input_private_t* priv = (tv_input_private_t*)dev;

    NEXUS_Error rc;
    NEXUS_HdmiInputStatus status;
    rc = NEXUS_HdmiInput_GetStatus(priv->hdmiInput, &status);
    if (rc) {
        ALOGE("%s: NEXUS_HdmiInput_GetStatus failed (%d)", __FUNCTION__, rc);
        return -1;
    }
    if (status.deviceAttached) {
        ALOGV("%s: device attached! (%dx%d)", __FUNCTION__, status.width, status.height);
        priv->stream_config.stream_id = STREAM_ID_HDMI;
        priv->stream_config.type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE;
        priv->stream_config.max_video_width = status.width ? status.width : 1920;
        priv->stream_config.max_video_height = status.height ? status.height : 1080;
        *num_configurations = 1;
        *configs = &priv->stream_config;
    } else {
        ALOGV("%s: device not attached!", __FUNCTION__);
        *num_configurations = 0;
    }
    return 0;
}

static int tv_input_open_stream(struct tv_input_device *dev, int device_id, tv_stream_t *stream)
{
    ALOGV("%s", __FUNCTION__);
    int err;
    tv_input_private_t* priv = (tv_input_private_t*)dev;

    if (device_id != DEVICE_ID_HDMI || stream->stream_id != STREAM_ID_HDMI) {
        return -EINVAL;
    }
    if (priv->sidebandContext) {
        return -EEXIST;
    }
    stream->type = TV_STREAM_TYPE_INDEPENDENT_VIDEO_SOURCE;
    priv->sidebandContext = libbcmsideband_init_sideband_tif(
        &stream->sideband_stream_source_handle, NULL, NULL, NULL, NULL, priv);
    if (!priv->sidebandContext) {
        ALOGE("Unable to initalize the sideband");
        err = -ENODEV;
        goto failed_sideband_init;
    }

    NEXUS_Error rc;
    NxClient_AllocSettings allocSettings;
    NxClient_GetDefaultAllocSettings(&allocSettings);
    allocSettings.simpleVideoDecoder = 1;
    allocSettings.simpleAudioDecoder = 1;
    allocSettings.surfaceClient = 1; /* surface client required for video window */
    rc = NxClient_Alloc(&allocSettings, &priv->allocResults);
    if (rc ||
        !priv->allocResults.simpleVideoDecoder[0].id ||
        !priv->allocResults.simpleAudioDecoder.id ||
        !priv->allocResults.surfaceClient[0].id) {
        ALOGE("unable to alloc transcode resources");
        err = -ENODEV;
        goto failed_alloc;
    }

    priv->videoDecoder = NEXUS_SimpleVideoDecoder_Acquire(priv->allocResults.simpleVideoDecoder[0].id);
    if (!priv->videoDecoder) {
        ALOGE("unable to acquire video decoder");
        err = -ENODEV;
        goto failed_acquire_video_decoder;
    }
    priv->audioDecoder = NEXUS_SimpleAudioDecoder_Acquire(priv->allocResults.simpleAudioDecoder.id);
    if (!priv->audioDecoder) {
        ALOGE("unable to acquire audio decoder");
        err = -ENODEV;
        goto failed_acquire_audio_decoder;
    }
    priv->surfaceClient = NEXUS_SurfaceClient_Acquire(priv->allocResults.surfaceClient[0].id);
    if (!priv->surfaceClient) {
        ALOGE("unable to acquire surface client");
        err = -ENODEV;
        goto failed_acquire_surface_client;
    }
    priv->videoSurfaceClient = NEXUS_SurfaceClient_AcquireVideoWindow(priv->surfaceClient, 0);
    if (!priv->videoSurfaceClient) {
        ALOGE("unable to acquire video window");
        err = -ENODEV;
        goto failed_acquire_video_window;
    }

    NxClient_ConnectSettings connectSettings;
    NxClient_GetDefaultConnectSettings(&connectSettings);
    connectSettings.simpleVideoDecoder[0].id = priv->allocResults.simpleVideoDecoder[0].id;
    connectSettings.simpleVideoDecoder[0].surfaceClientId = priv->allocResults.surfaceClient[0].id;
    connectSettings.simpleVideoDecoder[0].decoderCapabilities.connectType = NxClient_VideoDecoderConnectType_eWindowOnly;
    connectSettings.simpleVideoDecoder[0].windowCapabilities.type = NxClient_VideoWindowType_eMain;
    connectSettings.simpleAudioDecoder.id = priv->allocResults.simpleAudioDecoder.id;
    rc = NxClient_Connect(&connectSettings, &priv->connectId);
    if (rc) {
        ALOGE("unable to acquire video window");
        err = -ENODEV;
        goto failed_connect;
    }

    NEXUS_SimpleStcChannelHandle stcChannel;
    stcChannel = NEXUS_SimpleStcChannel_Create(NULL);
    NEXUS_SimpleVideoDecoder_SetStcChannel(priv->videoDecoder, stcChannel);
    NEXUS_SimpleAudioDecoder_SetStcChannel(priv->audioDecoder, stcChannel);

    rc = NEXUS_SimpleVideoDecoder_StartHdmiInput(priv->videoDecoder, priv->hdmiInput, NULL);
    if (rc) {
        ALOGE("Unable to start HDMI input video");
        err = -ENODEV;
        goto failed_start_hdmiinput_video;
    }
    rc = NEXUS_SimpleAudioDecoder_StartHdmiInput(priv->audioDecoder, priv->hdmiInput, NULL);
    if (rc) {
        ALOGE("Unable to start HDMI input audio");
        err = -ENODEV;
        goto failed_start_hdmiinput_audio;
    }
    ALOGD("HdmiInput %d is ACTIVE", HDMI_PORT_ID);
    NEXUS_HdmiInput_ToggleHotPlug(priv->hdmiInput);
    return 0;

failed_start_hdmiinput_audio:
    NEXUS_SimpleVideoDecoder_StopHdmiInput(priv->videoDecoder);
failed_start_hdmiinput_video:
    NxClient_Disconnect(priv->connectId);
failed_connect:
    NEXUS_SurfaceClient_Release(priv->videoSurfaceClient);
failed_acquire_video_window:
    NEXUS_SurfaceClient_Release(priv->surfaceClient);
failed_acquire_surface_client:
    NEXUS_SimpleAudioDecoder_Release(priv->audioDecoder);
failed_acquire_audio_decoder:
    NEXUS_SimpleVideoDecoder_Release(priv->videoDecoder);
failed_acquire_video_decoder:
    NxClient_Free(&priv->allocResults);
failed_alloc:
    libbcmsideband_release(priv->sidebandContext);
    priv->sidebandContext = NULL;
failed_sideband_init:
    return err;
}

static int tv_input_close_stream(struct tv_input_device *dev, int device_id,
    int stream_id)
{
    ALOGV("%s", __FUNCTION__);
    tv_input_private_t* priv = (tv_input_private_t*)dev;

    if (device_id != DEVICE_ID_HDMI || stream_id != STREAM_ID_HDMI) {
        return -EINVAL;
    }
    if (!priv->sidebandContext) {
        return -ENOENT;
    }

    NEXUS_SimpleAudioDecoder_StopHdmiInput(priv->audioDecoder);
    NEXUS_SimpleVideoDecoder_StopHdmiInput(priv->videoDecoder);
    NEXUS_SurfaceClient_Release(priv->videoSurfaceClient);
    NEXUS_SurfaceClient_Release(priv->surfaceClient);
    NEXUS_SimpleAudioDecoder_Release(priv->audioDecoder);
    NEXUS_SimpleVideoDecoder_Release(priv->videoDecoder);
    NxClient_Free(&priv->allocResults);
    libbcmsideband_release(priv->sidebandContext);
    priv->sidebandContext = NULL;
    return 0;
}

static int tv_input_request_capture(
        struct tv_input_device*, int, int, buffer_handle_t, uint32_t)
{
    ALOGV("%s", __FUNCTION__);
    return -EINVAL;
}

static int tv_input_cancel_capture(struct tv_input_device*, int, int, uint32_t)
{
    ALOGV("%s", __FUNCTION__);
    return -EINVAL;
}

/*****************************************************************************/

static int tv_input_device_close(struct hw_device_t *dev)
{
    ALOGV("%s", __FUNCTION__);
    tv_input_private_t* priv = (tv_input_private_t*)dev;

    if (priv) {
        NEXUS_HdmiOutput_Close(priv->hdmiOutput);
        NEXUS_HdmiInput_Close(priv->hdmiInput);
        NxClient_Disconnect(priv->connectId);
        NxClient_Uninit();
        free(priv);
    }
    return 0;
}

/*****************************************************************************/

static int tv_input_device_open(const struct hw_module_t* module,
        const char* name, struct hw_device_t** device)
{
    ALOGV("%s", __FUNCTION__);
    int status = -EINVAL;
    if (property_get_bool("ro.nx.trim.hdmiin", 1)) {
        *device = NULL; // The feature is disabled (trimmed-out)
        status = 0;
    } else if (!strcmp(name, TV_INPUT_DEFAULT_DEVICE)) {
        tv_input_private_t* dev = (tv_input_private_t*)malloc(sizeof(*dev));

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = TV_INPUT_DEVICE_API_VERSION_0_1;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = tv_input_device_close;

        dev->device.initialize = tv_input_initialize;
        dev->device.get_stream_configurations =
                tv_input_get_stream_configurations;
        dev->device.open_stream = tv_input_open_stream;
        dev->device.close_stream = tv_input_close_stream;
        dev->device.request_capture = tv_input_request_capture;
        dev->device.cancel_capture = tv_input_cancel_capture;

        // Receive a pointer to the tuner device (required for callbacks)
        dev->callback_dev = *(struct tv_input_device**)device;
        // Return a pointer to the hdmi private data
        *device = (struct hw_device_t*)dev;
        status = 0;
    }
    return status;
}

