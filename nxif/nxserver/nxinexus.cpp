/******************************************************************************
 * (c) 2017 Broadcom
 *
 * This program is the proprietary software of Broadcom and/or its licensors,
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
#define LOG_TAG "bcm-nxs-b"

#include <inttypes.h>
#include <cutils/log.h>
#include "nxinexus.h"

#include "nxclient.h"
#include "nexus_platform_common.h"
#include "nexus_platform.h"
#include "bkni.h"
#include "namevalue.h"
#include "namevalue.inc"

#define NX_HD_OUT_FMT             "nx.vidout.force"
#define NX_HD_OUT_OBR             "ro.nx.vidout.obr"
#define NX_HDCP_TOGGLE            "nx.hdcp.force"
#define NX_HD_OUT_COLOR_DEPTH_10B "ro.nx.colordepth10b.force"

Return<uint64_t> NexusImpl::client(int32_t pid) {
   Mutex::Autolock _l(mLock);

   NEXUS_PlatformObjectInstance *objects = NULL;
   NEXUS_ClientHandle nexusClient = NULL;
   NEXUS_InterfaceName interfaceName;
   size_t num = 64; /* starting size. */
   size_t max_num = 1024; /* some big value. */
   size_t i, cached_num;
   NEXUS_Error rc;

   uint64_t client = 0;

   NEXUS_Platform_GetDefaultInterfaceName(&interfaceName);
   strcpy(interfaceName.name, "NEXUS_Client");
   do {
      cached_num = num;
      if (objects != NULL) {
         free(objects);
      }
      objects = (NEXUS_PlatformObjectInstance *) malloc(num*sizeof(NEXUS_PlatformObjectInstance));
      if (objects == NULL) {
         ALOGE("failed allocation of %zu nexus platform objects!!!", num);
         goto out;
      }
      rc = NEXUS_Platform_GetObjects(&interfaceName, objects, num, &num);
      if (rc == NEXUS_PLATFORM_ERR_OVERFLOW) {
         if (num > max_num) {
            rc = NEXUS_SUCCESS;
            num = 0;
            free(objects);
            ALOGW("NEXUS_Platform_GetObjects overflowed - giving up...");
            goto out;
         } else if (num <= cached_num) {
            num = 2 * cached_num;
         }
      }
   } while (rc == NEXUS_PLATFORM_ERR_OVERFLOW);

   for (i=0; i < num; i++) {
      NEXUS_ClientStatus status;
      rc = NEXUS_Platform_GetClientStatus(reinterpret_cast<NEXUS_ClientHandle>(objects[i].object), &status);
      if (rc) continue;

      if (status.pid == (unsigned)pid) {
         nexusClient = reinterpret_cast<NEXUS_ClientHandle>(objects[i].object);
         break;
      }
   }

   if (objects != NULL) {
      free(objects);
   }

   client = (uint64_t)(intptr_t)nexusClient;

out:
   ALOGV("process: %d -> associated with nexus client: %" PRIu64 "", pid, client);
   return client;
}

Return<NexusStatus> NexusImpl::registerHpdCb(uint64_t cId, const ::android::sp<INexusHpdCb>& cb) {
   Vector<struct HpdCb>::iterator v;
   for (v = mHpdCb.begin(); v != mHpdCb.end(); ++v) {
      if ((*v).cId == cId) {
         if (cb == NULL) {
            mHpdCb.erase(v);
            ALOGI("[hpd]:client: %" PRIu64 ": unregistered", cId);
            return NexusStatus::SUCCESS;
         } else {
            ALOGW("[hpd]:client: %" PRIu64 ": already registered", cId);
            return NexusStatus::ALREADY_REGISTERED;
         }
      }
   }

   if (cb == NULL) {
      ALOGW("[hpd]:client: %" PRIu64 ": invalid registration", cId);
      return NexusStatus::BAD_VALUE;
   } else {
      struct HpdCb n = {cId, cb};
      mHpdCb.push_back(n);
      ALOGI("[hpd]:client: %" PRIu64 ": registered", cId);
      return NexusStatus::SUCCESS;
   }
}

Return<NexusStatus> NexusImpl::registerDspCb(uint64_t cId, const ::android::sp<INexusDspCb>& cb) {
   Vector<struct DspCb>::iterator v;
   for (v = mDspCb.begin(); v != mDspCb.end(); ++v) {
      if ((*v).cId == cId) {
         if (cb == NULL) {
            mDspCb.erase(v);
            ALOGI("[dsp]:client: %" PRIu64 ": unregistered", cId);
            return NexusStatus::SUCCESS;
         } else {
            ALOGW("[dsp]:client: %" PRIu64 ": already registered", cId);
            return NexusStatus::ALREADY_REGISTERED;
         }
      }
   }

   if (cb == NULL) {
      ALOGW("[dsp]:client: %" PRIu64 ": invalid registration", cId);
      return NexusStatus::BAD_VALUE;
   } else {
      struct DspCb n = {cId, cb};
      mDspCb.push_back(n);
      ALOGI("[dsp]:client: %" PRIu64 ": registered", cId);
      return NexusStatus::SUCCESS;
   }
}

Return<NexusStatus> NexusImpl::setPwr(const NexusPowerState& p) {
   struct pmlib_state_t s;
   memcpy(&s, &p, sizeof(struct pmlib_state_t));
   mPmLib.setState(&s);
   return NexusStatus::SUCCESS;
}

Return<void> NexusImpl::getPwr(getPwr_cb _hidl_cb) {
   struct pmlib_state_t s;
   NexusPowerState p;
   mPmLib.getState(&s);
   memcpy(&p, &s, sizeof(struct pmlib_state_t));
   _hidl_cb(p);
   return Void();
}

void NexusImpl::start_middleware() {
   init_hdmi_out();
   init_ir();
}

void NexusImpl::stop_middleware() {
   deinit_ir();
   deinit_hdmi_out();
}

static bool parseNexusIrMode(const char *name, NEXUS_IrInputMode *mode)
{
    struct irmode {
        const char * name;
        NEXUS_IrInputMode value;
    };

    #define DEFINE_IRINPUTMODE(mode) { #mode, NEXUS_IrInputMode_e##mode }
    static const struct irmode ir_modes[] = {
        DEFINE_IRINPUTMODE(TwirpKbd),            /* TWIRP */
        DEFINE_IRINPUTMODE(Sejin38KhzKbd),       /* Sejin IR keyboard (38.4KHz) */
        DEFINE_IRINPUTMODE(Sejin56KhzKbd),       /* Sejin IR keyboard (56.0KHz) */
        DEFINE_IRINPUTMODE(RemoteA),             /* remote A */
        DEFINE_IRINPUTMODE(RemoteB),             /* remote B */
        DEFINE_IRINPUTMODE(CirGI),               /* Consumer GI */
        DEFINE_IRINPUTMODE(CirSaE2050),          /* Consumer SA E2050 */
        DEFINE_IRINPUTMODE(CirTwirp),            /* Consumer Twirp */
        DEFINE_IRINPUTMODE(CirSony),             /* Consumer Sony */
        DEFINE_IRINPUTMODE(CirRecs80),           /* Consumer Rec580 */
        DEFINE_IRINPUTMODE(CirRc5),              /* Consumer Rc5 */
        DEFINE_IRINPUTMODE(CirUei),              /* Consumer UEI */
        DEFINE_IRINPUTMODE(CirRfUei),            /* Consumer RF UEI */
        DEFINE_IRINPUTMODE(CirEchoStar),         /* Consumer EchoRemote */
        DEFINE_IRINPUTMODE(SonySejin),           /* Sony Sejin keyboard using UART D */
        DEFINE_IRINPUTMODE(CirNec),              /* Consumer NEC */
        DEFINE_IRINPUTMODE(CirRC6),              /* Consumer RC6 */
        DEFINE_IRINPUTMODE(CirGISat),            /* Consumer GI Satellite */
        DEFINE_IRINPUTMODE(Custom),              /* Customer specific type. See NEXUS_IrInput_SetCustomSettings. */
        DEFINE_IRINPUTMODE(CirDirectvUhfr),      /* DIRECTV uhfr (In IR mode) */
        DEFINE_IRINPUTMODE(CirEchostarUhfr),     /* Echostar uhfr (In IR mode) */
        DEFINE_IRINPUTMODE(CirRcmmRcu),          /* RCMM Remote Control Unit */
        DEFINE_IRINPUTMODE(CirRstep),            /* R-step Remote Control Unit */
        DEFINE_IRINPUTMODE(CirXmp),              /* XMP-2 */
        DEFINE_IRINPUTMODE(CirXmp2Ack),          /* XMP-2 Ack/Nak */
        DEFINE_IRINPUTMODE(CirRC6Mode0),         /* Consumer RC6 Mode 0 */
        DEFINE_IRINPUTMODE(CirRca),              /* Consumer RCA */
        DEFINE_IRINPUTMODE(CirToshibaTC9012),    /* Consumer Toshiba */
        DEFINE_IRINPUTMODE(CirXip),              /* Consumer Tech4home XIP protocol */

        /* end of table marker */
        { NULL, NEXUS_IrInputMode_eMax }
    };
    #undef DEFINE_IRINPUTMODE

    const struct irmode *ptr = ir_modes;
    while(ptr->name) {
        if (strcmp(ptr->name, name) == 0) {
            *mode = ptr->value;
            return true;
        }
        ptr++;
    }
    return false; /* not found */
}

bool NexusImpl::init_ir(void) {
   static const char * ir_map_path = "/vendor/usr/irkeymap";
   static const char * ir_map_ext = ".ikm";
   static const NEXUS_IrInputMode ir_mode_default_enum = NEXUS_IrInputMode_eCirNec;
   static const char * ir_mode_default = "CirNec";
   static const char * ir_map_default = "broadcom_silver";
   static const char * ir_mask_default = "0";

   char ir_mode_property[PROPERTY_VALUE_MAX];
   char ir_map_property[PROPERTY_VALUE_MAX];
   char ir_mask_property[PROPERTY_VALUE_MAX];

   NEXUS_IrInputMode mode;
   sp<NexusIrMap> map;
   uint64_t mask;

   memset(ir_mode_property, 0, sizeof(ir_mode_property));
   property_get("ro.ir_remote.mode", ir_mode_property, ir_mode_default);
   if (parseNexusIrMode(ir_mode_property, &mode)) {
       ALOGI("Nexus IR remote mode: %s", ir_mode_property);
   } else {
       ALOGW("Unknown IR remote mode: '%s', falling back to default '%s'",
               ir_mode_property, ir_mode_default);
       mode = ir_mode_default_enum;
   }

   memset(ir_map_property, 0, sizeof(ir_map_property));
   property_get("ro.ir_remote.map", ir_map_property, ir_map_default);
   String8 map_path(ir_map_path);
   map_path += "/";
   map_path += ir_map_property;
   map_path += ir_map_ext;
   ALOGI("Nexus IR remote map: %s (%s)", ir_map_property, map_path.string());
   status_t status = NexusIrMap::load(map_path, &map);
   if (status)
   {
      ALOGE("Nexus IR map load failed: %s", map_path.string());
      return false;
   }

   memset(ir_mask_property, 0, sizeof(ir_mask_property));
   property_get("ro.ir_remote.mask", ir_mask_property, ir_mask_default);
   mask = strtoull(ir_mask_property, NULL, 0);
   ALOGI("Nexus IR remote mask: %s (0x%llx)", ir_mask_property,
           (unsigned long long)mask);

   return mIrHandler.start(mode, map, mask, mask);
}

void NexusImpl::deinit_ir() {
   mIrHandler.stop();
}

void NexusImpl::init_hdmi_out() {
   NxClient_CallbackThreadSettings settings;
   NEXUS_Error rc;

   NxClient_GetDefaultCallbackThreadSettings(&settings);
   settings.hdmiOutputHotplug.callback = cbHotplug;
   settings.hdmiOutputHotplug.context  = this;
   settings.hdmiOutputHotplug.param    = 0;
#if ANDROID_ENABLE_HDMI_HDCP
   settings.hdmiOutputHdcpChanged.callback = cbHdcp;
   settings.hdmiOutputHdcpChanged.context  = this;
   settings.hdmiOutputHdcpChanged.param    = 0;
#endif
   settings.displaySettingsChanged.callback = cbDisplay;
   settings.displaySettingsChanged.context  = this;
   settings.displaySettingsChanged.param    = 0;
   rc = NxClient_StartCallbackThread(&settings);

   cbHotplug(this, 0);
#if ANDROID_ENABLE_HDMI_HDCP
   cbHdcp(this, 0);
#endif
}

void NexusImpl::deinit_hdmi_out() {
   NxClient_StopCallbackThread();
}

void NexusImpl::cbHotplug(void *context, int param __unused) {
   NEXUS_Error rc;
   NxClient_StandbyStatus standbyStatus;
   NexusImpl *pNexusImpl = reinterpret_cast<NexusImpl *>(context);

   Mutex::Autolock autoLock(pNexusImpl->mLock);
   rc = NxClient_GetStandbyStatus(&standbyStatus);
   if (rc) {
      return;
   }

   if (standbyStatus.settings.mode == NEXUS_PlatformStandbyMode_eOn) {
      hdmi_state isConnected;
      NxClient_DisplayStatus status;

      rc = NxClient_GetDisplayStatus(&status);
      if (rc) {
         return;
      }

      isConnected = (status.hdmi.status.connected) ? HDMI_PLUGGED : HDMI_UNPLUGGED;

      if (isConnected == pNexusImpl->mHpd && isConnected == HDMI_PLUGGED) {
         pNexusImpl->cbHpdAction(HDMI_UNPLUGGED);
      }
      pNexusImpl->mHpd = isConnected;
      pNexusImpl->cbHpdAction(isConnected);
   }
}

void NexusImpl::cbHpdAction(hdmi_state state) {
   int hdpSwitch;
   NxClient_DisplayStatus status;
   NxClient_DisplaySettings settings;
   const char *hpdDevice = "/dev/hdmi_hpd";
   NEXUS_VideoFormat format;
   bool update;
   NEXUS_Error rc;
   unsigned limitedColorDepth;
   NEXUS_ColorSpace limitedColorSpace;
   bool limitedColorSettings;
   NEXUS_DisplayCapabilities caps;
   Vector<struct HpdCb>::const_iterator v;

   rc = NxClient_GetDisplayStatus(&status);
   if (rc) {
      ALOGE("%s: Could not get display status!!!", __PRETTY_FUNCTION__);
      return;
   }
   do {
      update = false;
      // check if we need to change the output format.
      NxClient_GetDisplaySettings(&settings);
      NEXUS_GetDisplayCapabilities(&caps);
      if (state == HDMI_PLUGGED) {
         format = forcedOutputFmt();
         if (!status.hdmi.status.videoFormatSupported[format] ||
             !caps.displayFormatSupported[format]) {
            format = NEXUS_VideoFormat_eUnknown;
         }
         if (format == NEXUS_VideoFormat_eUnknown) {
            format = bestOutputFmt(&status.hdmi.status, &caps);
         }
         if ((format != NEXUS_VideoFormat_eUnknown) && (settings.format != format)) {
            settings.format = format;
            update = true;
         }
         limitedColorSettings = getLimitedColorSettings(limitedColorDepth, limitedColorSpace);
         if (limitedColorSettings &&
             ((limitedColorDepth != status.hdmi.status.colorDepth)
             || (limitedColorSpace != status.hdmi.status.colorSpace))) {
            settings.hdmiPreferences.colorDepth = limitedColorDepth;
            settings.hdmiPreferences.colorSpace = limitedColorSpace;
            update = true;
         }
      }
#if ANDROID_ENABLE_HDMI_HDCP
      // check if we need  to re-enable hdcp.
      NxClient_HdcpLevel hdcp = settings.hdmiPreferences.hdcp;
      if (status.hdmi.status.connected && status.hdmi.status.rxPowered) {
         settings.hdmiPreferences.hdcp =
            (getForcedHdcp() == 0) ? NxClient_HdcpLevel_eNone : NxClient_HdcpLevel_eOptional;
            if (getForcedHdcp() == 0) {
               ALOGW("%s: HDCP disabled by run-time, some features may not work.", __PRETTY_FUNCTION__);
            }
      }
      if (hdcp != settings.hdmiPreferences.hdcp) {
         update = true;
      }
#endif
      if (update) {
         rc = NxClient_SetDisplaySettings(&settings);
         if (!rc) {
            switch (settings.format) {
            case NEXUS_VideoFormat_e4096x2160p60hz:
            case NEXUS_VideoFormat_e3840x2160p60hz:
            case NEXUS_VideoFormat_e4096x2160p50hz:
            case NEXUS_VideoFormat_e3840x2160p50hz:
            case NEXUS_VideoFormat_e4096x2160p30hz:
            case NEXUS_VideoFormat_e3840x2160p30hz:
            case NEXUS_VideoFormat_e4096x2160p25hz:
            case NEXUS_VideoFormat_e3840x2160p25hz:
            case NEXUS_VideoFormat_e4096x2160p24hz:
            case NEXUS_VideoFormat_e3840x2160p24hz:
               property_set("sys.display-size", "3840x2160");
            break;
            default:
               property_set("sys.display-size", "1920x1080");
            break;
            }
         }
      }
   } while (rc == NXCLIENT_BAD_SEQUENCE_NUMBER);

   // propagate the hpd state change to the registered listeners.
   for (v = mHpdCb.begin(); v != mHpdCb.end(); ++v) {
      ALOGI("[hpd]:client: %" PRIu64 ": issuing %s", (*v).cId,
            (state == HDMI_PLUGGED) ? "CONNECTED" : "DISCONNECTED");
      if ((*v).cb != NULL) {
         (*v).cb->onHpd(state == HDMI_PLUGGED);
      }
   }

   // finally, update the hpd switch state for android framework.
   hdpSwitch = open(hpdDevice, O_WRONLY);
   ioctl(hdpSwitch, HDMI_HPD_IOCTL_SET_SWITCH, &state);
   close(hdpSwitch);
}

NEXUS_VideoFormat NexusImpl::forcedOutputFmt(void) {
   NEXUS_VideoFormat forced_format = NEXUS_VideoFormat_eUnknown;
   char value[PROPERTY_VALUE_MAX];
   char name[PROPERTY_VALUE_MAX];

   memset(value, 0, sizeof(value));
   sprintf(name, "persist.%s", NX_HD_OUT_FMT);
   if (property_get(name, value, "")) {
      if (strlen(value)) {
         forced_format = (NEXUS_VideoFormat)lookup(g_videoFormatStrs, value);
      }
   }

   if ((forced_format == NEXUS_VideoFormat_eUnknown) || (forced_format >= NEXUS_VideoFormat_eMax)) {
      memset(value, 0, sizeof(value));
      sprintf(name, "ro.%s", NX_HD_OUT_FMT);
      if (property_get(name, value, "")) {
         if (strlen(value)) {
            forced_format = (NEXUS_VideoFormat)lookup(g_videoFormatStrs, value);
         }
      }
   }

   return forced_format;
}

NEXUS_VideoFormat NexusImpl::bestOutputFmt(NEXUS_HdmiOutputStatus *status, NEXUS_DisplayCapabilities *caps) {
   int i;
   NEXUS_VideoFormat format = NEXUS_VideoFormat_eUnknown;

   NEXUS_VideoFormat ordered_res_list[] = {
      NEXUS_VideoFormat_e4096x2160p60hz,
      NEXUS_VideoFormat_e3840x2160p60hz,
      NEXUS_VideoFormat_e4096x2160p50hz,
      NEXUS_VideoFormat_e3840x2160p50hz,
      NEXUS_VideoFormat_e4096x2160p30hz,
      NEXUS_VideoFormat_e3840x2160p30hz,
      NEXUS_VideoFormat_e4096x2160p25hz,
      NEXUS_VideoFormat_e3840x2160p25hz,
      NEXUS_VideoFormat_e4096x2160p24hz,
      NEXUS_VideoFormat_e3840x2160p24hz,
      NEXUS_VideoFormat_e1080p,
      NEXUS_VideoFormat_e1080p50hz,
      NEXUS_VideoFormat_e1080p30hz,
      NEXUS_VideoFormat_e1080p25hz,
      NEXUS_VideoFormat_e1080p24hz,
      NEXUS_VideoFormat_e1080i,
      NEXUS_VideoFormat_e1080i50hz,
      NEXUS_VideoFormat_e720p,
      NEXUS_VideoFormat_e720p50hz,
      NEXUS_VideoFormat_e720p30hz,
      NEXUS_VideoFormat_e720p25hz,
      NEXUS_VideoFormat_e720p24hz,
      NEXUS_VideoFormat_ePal,
      NEXUS_VideoFormat_eSecam,
      NEXUS_VideoFormat_eUnknown,
   };

   NEXUS_VideoFormat ordered_fps_list[] = {
      NEXUS_VideoFormat_e4096x2160p60hz,
      NEXUS_VideoFormat_e3840x2160p60hz,
      NEXUS_VideoFormat_e4096x2160p50hz,
      NEXUS_VideoFormat_e3840x2160p50hz,
      NEXUS_VideoFormat_e1080p,
      NEXUS_VideoFormat_e1080p50hz,
      NEXUS_VideoFormat_e720p,
      NEXUS_VideoFormat_e720p50hz,
      NEXUS_VideoFormat_e4096x2160p30hz,
      NEXUS_VideoFormat_e3840x2160p30hz,
      NEXUS_VideoFormat_e4096x2160p25hz,
      NEXUS_VideoFormat_e3840x2160p25hz,
      NEXUS_VideoFormat_e4096x2160p24hz,
      NEXUS_VideoFormat_e3840x2160p24hz,
      NEXUS_VideoFormat_e1080p30hz,
      NEXUS_VideoFormat_e1080p25hz,
      NEXUS_VideoFormat_e1080p24hz,
      NEXUS_VideoFormat_e1080i,
      NEXUS_VideoFormat_e1080i50hz,
      NEXUS_VideoFormat_e720p30hz,
      NEXUS_VideoFormat_e720p25hz,
      NEXUS_VideoFormat_e720p24hz,
      NEXUS_VideoFormat_ePal,
      NEXUS_VideoFormat_eSecam,
      NEXUS_VideoFormat_eUnknown,
   };

   NEXUS_VideoFormat *ordered_list;
   ordered_list = property_get_bool(NX_HD_OUT_OBR, 0) ?
      &ordered_res_list[0] : &ordered_fps_list[0];

   for (i = 0 ; *(ordered_list+i) != NEXUS_VideoFormat_eUnknown; i++) {
      if (status->videoFormatSupported[*(ordered_list+i)] &&
          caps->displayFormatSupported[*(ordered_list+i)]) {
         format = *(ordered_list+i);
         break;
      }
   }

   return format;
}

bool NexusImpl::getForcedHdcp(void)
{
   char name[PROPERTY_VALUE_MAX];
#if ANDROID_ENABLE_HDMI_HDCP
   int def_val = 1;
#else
   int def_val = 0;
#endif

   sprintf(name, "dyn.%s", NX_HDCP_TOGGLE);
   return property_get_bool(name, def_val);
}

bool NexusImpl::getLimitedColorSettings(unsigned &limitedColorDepth,
   NEXUS_ColorSpace &limitedColorSpace) {
   NEXUS_HdmiOutputHandle hdmiOutput;
   NEXUS_HdmiOutputEdidData edid;
   NEXUS_Error errCode;

   if (!property_get_bool(NX_HD_OUT_COLOR_DEPTH_10B, 0))
      return false;

   // Force 10bpp color-depth only if the hdmi Rx supports 12bits or higher
   // and if it supports color space 4:2:0. Using 10bpp and 4:2:0 color space
   // reduces the hdmi TMDS clock rate, giving more stability to the hdmi
   // connection on some TV's.
   hdmiOutput = NEXUS_HdmiOutput_Open(0+NEXUS_ALIAS_ID, NULL);
   errCode = NEXUS_HdmiOutput_GetEdidData(hdmiOutput, &edid);
   if (errCode) {
       ALOGW("%s: Unable to read edid, err:%d", __FUNCTION__, errCode);
       return false;
   }

   if (edid.hdmiVsdb.valid && edid.hdmiVsdb.deepColor30bit &&
         (edid.hdmiVsdb.deepColor36bit || edid.hdmiVsdb.deepColor48bit)
         && edid.hdmiForumVsdb.deepColor420_30bit) {
      limitedColorDepth = 10;
      limitedColorSpace = NEXUS_ColorSpace_eYCbCr420;
      return true;
   } else {
      limitedColorDepth = 0;
      limitedColorSpace = NEXUS_ColorSpace_eAuto;
      return true;
   }

   return false;
}

void NexusImpl::cbDisplay(void *context, int param __unused) {
   NEXUS_Error rc;
   NxClient_StandbyStatus standbyStatus;
   NexusImpl *pNexusImpl = reinterpret_cast<NexusImpl *>(context);
   Mutex::Autolock autoLock(pNexusImpl->mLock);

   rc = NxClient_GetStandbyStatus(&standbyStatus);
   if (rc != NEXUS_SUCCESS) {
      return;
   }

   if (standbyStatus.settings.mode == NEXUS_PlatformStandbyMode_eOn) {
      pNexusImpl->cbDisplayAction();
   }
}

void NexusImpl::cbDisplayAction() {
   Vector<struct DspCb>::const_iterator v;
   for (v = mDspCb.begin(); v != mDspCb.end(); ++v) {
      ALOGI("[dsp]:client: %" PRIu64 ": signalling", (*v).cId);
      if ((*v).cb != NULL) {
         (*v).cb->onDsp();
      }
   }
}

void NexusImpl::cbHdcp(void *context, int param) {
#if ANDROID_ENABLE_HDMI_HDCP
   int rc;
   NxClient_StandbyStatus standbyStatus;
   NexusImpl *pNexusImpl = reinterpret_cast<NexusImpl *>(context);
   Mutex::Autolock autoLock(pNexusImpl->mLock);

   (void)param;

   rc = NxClient_GetStandbyStatus(&standbyStatus);
   if (rc != NEXUS_SUCCESS) {
      return;
   }

   // bit of useless callback at the moment...
   if (standbyStatus.settings.mode == NEXUS_PlatformStandbyMode_eOn) {
      NxClient_DisplayStatus status;
      rc = NxClient_GetDisplayStatus(&status);
      if (rc) {
         return;
      }
   }
#else
   (void)context;
   (void)param;
#endif
}
