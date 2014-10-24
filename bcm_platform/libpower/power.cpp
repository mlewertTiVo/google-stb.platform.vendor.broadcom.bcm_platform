/*
 * Copyright (C) 2012 The Android Open Source Project
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
// Verbose messages removed
//#define LOG_NDEBUG 0
#define LOG_TAG "Brcmstb PowerHAL"

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <utils/Log.h>

#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <hardware/power.h>
#include "PmLibService.h"

#include "nexus_power.h"

using namespace android;

static const char *nexus_power_state_string[] = {
    "S0",
    "S1",
    "S2",
    "S3",
    "S4",
    "S5"
};

static NexusPowerState get_nexus_power_state()
{
    NexusPowerState nexusPowerOffState = eNexusPowerState_S3;
    char value[PROPERTY_VALUE_MAX] = "";

    property_get("ro.pm.offstate", value, "s3");

    if (strcasecmp(value, "1") == 0 || strcasecmp(value, "s1") == 0) {
        nexusPowerOffState = eNexusPowerState_S1;
    }
    else if (strcasecmp(value, "2") == 0 || strcasecmp(value, "s2") == 0) {
        nexusPowerOffState = eNexusPowerState_S2;
    }
    else if (strcasecmp(value, "3") == 0 || strcasecmp(value, "s3") == 0) {
        nexusPowerOffState = eNexusPowerState_S3;
    }
    else if (strcasecmp(value, "5") == 0 || strcasecmp(value, "s5") == 0) {
        nexusPowerOffState = eNexusPowerState_S5;
    }
    return nexusPowerOffState;
}

static void power_init(struct power_module *module)
{
    LOGD("%s: ",__FUNCTION__);
}

static void power_set_shutdown()
{
    property_set("sys.powerctl", "shutdown");
}

static int power_set_pmlibservice_state(NexusPowerState state)
{
    int rc = 0;
    pmlib_state_t pmlib_state;
    sp<IBinder> binder = defaultServiceManager()->getService(IPmLibService::descriptor);
    sp<IPmLibService> service = interface_cast<IPmLibService>(binder);

    if (service.get() == NULL) {
        ALOGE("%s: Cannot get \"PmLibService\" service!!!", __FUNCTION__);
        return -1;
    }

    rc = service->getState(&pmlib_state);
    if (rc != 0) {
        ALOGE("%s: Could not get PmLibService state [rc=%d]!!!", __FUNCTION__, rc);
        return rc;
    }

    if (state == eNexusPowerState_S1) {
        char value[PROPERTY_VALUE_MAX] = "";
        bool usboff   = true;
        bool ethoff   = true;
        bool mocaoff  = true;
        bool sataoff  = false;
        bool tp1off   = false;
        bool tp2off   = false;
        bool tp3off   = false;
        bool ddroff   = false;
        bool memc1off = false;

        /* usboff == true means leave USB ON during standby */
        property_get("ro.pm.usboff", value, NULL);
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            usboff = false;
        }
        else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            usboff = true;
        }

        /* ethoff == true means leave ethernet ON during standby */
        property_get("ro.pm.ethoff", value, NULL);
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            ethoff = false;
        }
        else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            ethoff = true;
        }

        /* mocaoff == true means leave MOCA ON during standby */
        property_get("ro.pm.mocaoff", value, NULL);
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            mocaoff = false;
        }
        else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            mocaoff = true;
        }

        /* sataoff == true means leave SATA ON during standby */
        property_get("ro.pm.sataoff", value, NULL);
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            sataoff = false;
        }
        else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            sataoff = true;
        }

        /* tp1off == true means leave CPU thread 1 ON during standby */
        property_get("ro.pm.tp1off", value, NULL);
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            tp1off = false;
        }
        else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            tp1off = true;
        }

        /* tp2off == true means leave CPU thread 2 ON during standby */
        property_get("ro.pm.tp2off", value, NULL);
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            tp2off = false;
        }
        else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            tp2off = true;
        }

        /* tp3off == true means leave CPU thread 3 ON during standby */
        property_get("ro.pm.tp3off", value, NULL);
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            tp3off = false;
        }
        else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            tp3off = true;
        }

        /* ddroff == true means leave DDR timeout to default during standby */
        property_get("ro.pm.ddroff", value, NULL);
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            ddroff = false;
        }
        else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            ddroff = true;
        }

        /* memc1off == true means enable MEMC1 status during standby */
        property_get("ro.pm.memc1off", value, NULL);
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            memc1off = false;
        }
        else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            memc1off = true;
        }

        pmlib_state.usb   = usboff;
        pmlib_state.enet  = ethoff;
        pmlib_state.moca  = mocaoff;
        pmlib_state.sata  = sataoff;
        pmlib_state.tp1   = tp1off;
        pmlib_state.tp2   = tp2off;
        pmlib_state.tp3   = tp3off;
        pmlib_state.cpu   = false;
        pmlib_state.ddr   = ddroff;
        pmlib_state.memc1 = memc1off;
    }
    else if (state == eNexusPowerState_S0) {
        pmlib_state.usb   = true;
        pmlib_state.enet  = true;
        pmlib_state.moca  = true;
        pmlib_state.sata  = true;
        pmlib_state.tp1   = true;
        pmlib_state.tp2   = true;
        pmlib_state.tp3   = true;
        pmlib_state.cpu   = true;
        pmlib_state.ddr   = true;
        pmlib_state.memc1 = true;
    }
    return service->setState(&pmlib_state);
}
    
static int power_set_state(NexusPowerState toState, NexusPowerState fromState)
{
    int rc = 0;
    switch (toState)
    {
        case eNexusPowerState_S0: {
            if (fromState == eNexusPowerState_S1) {
                rc = power_set_pmlibservice_state(toState);
            }
        } break;

        case eNexusPowerState_S1: {
            if (fromState == eNexusPowerState_S0) {
                rc = power_set_pmlibservice_state(toState);
            }
        } break;

        case eNexusPowerState_S5: {
            power_set_shutdown();
        } break;

        default: {
        } break;
    }
    return rc;
}

static void power_set_interactive(struct power_module *module, int on)
{
    int ret;
    NexusPowerState nexusPowerOffState = get_nexus_power_state();

    ALOGV("%s: %s", __FUNCTION__, on ? "ON" : "OFF");

    if (on) {
        ret = power_set_state(eNexusPowerState_S0, nexusPowerOffState);
        if (ret == 0) {
            ret = NexusPower_SetPowerState(eNexusPowerState_S0);
        }
    }
    else {
        ret = NexusPower_SetPowerState(nexusPowerOffState);
        if (ret == NEXUS_SUCCESS) {
            ret = power_set_state(nexusPowerOffState, eNexusPowerState_S0);
        }
    }
    LOGI("%s: %s set Nexus power state %s", __FUNCTION__, !ret ? "Successfully" : "Could not", on ? "S0" : nexus_power_state_string[nexusPowerOffState]);
}

static void power_hint(struct power_module *module, power_hint_t hint, void *data) {
    switch (hint) {
        case POWER_HINT_VSYNC:
            ALOGV("%s: POWER_HINT_VSYNC received", __FUNCTION__);
            break;
        case POWER_HINT_INTERACTION:
            ALOGV("%s: POWER_HINT_INTERACTION received", __FUNCTION__);
            break;
        case POWER_HINT_VIDEO_ENCODE:
            ALOGV("%s: POWER_HINT_VIDEO_ENCODE received", __FUNCTION__);
            break;
        case POWER_HINT_VIDEO_DECODE:
            ALOGV("%s: POWER_HINT_VIDEO_DECODE received", __FUNCTION__);
            break;
        case POWER_HINT_LOW_POWER:
            ALOGV("%s: POWER_HINT_LOW_POWER received", __FUNCTION__);
            break;
        default:
            break;
    }
}

static struct hw_module_methods_t power_module_methods = {
    open: NULL
};

struct power_module HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        module_api_version: POWER_MODULE_API_VERSION_0_2,
        hal_api_version: HARDWARE_HAL_API_VERSION,
        id: POWER_HARDWARE_MODULE_ID,
        name: "Brcmstb Power HAL",
        author: "Broadcom Corp",
        methods: &power_module_methods
    },
    init: power_init,
    setInteractive: power_set_interactive,
    powerHint: power_hint
};
