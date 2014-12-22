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

#include "nexus_power.h"

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cutils/properties.h>
#include <utils/Timers.h>
#include <hardware/hardware.h>
#include <hardware/power.h>
#include <hardware_legacy/power.h>
#include "PmLibService.h"

using namespace android;

// Keep the CPU alive wakelock
static const char *WAKE_LOCK_ID = "PowerHAL";

// Property definitions (max length 32)...
static const char *PROPERTY_SYS_POWER_DOZE_TIMEOUT  = "persist.sys.power.doze.timeout";
static const char *PROPERTY_PM_DOZESTATE            = "ro.pm.dozestate";
static const char *PROPERTY_PM_OFFSTATE             = "ro.pm.offstate";
static const char *PROPERTY_PM_INTERACTIVE_TIMEOUT  = "ro.pm.interactive.timeout";
static const char *PROPERTY_PM_USBOFF               = "ro.pm.usboff";
static const char *PROPERTY_PM_ETHOFF               = "ro.pm.ethoff";
static const char *PROPERTY_PM_MOCAOFF              = "ro.pm.mocaoff";
static const char *PROPERTY_PM_SATAOFF              = "ro.pm.sataoff";
static const char *PROPERTY_PM_TP1OFF               = "ro.pm.tp1off";
static const char *PROPERTY_PM_TP2OFF               = "ro.pm.tp2off";
static const char *PROPERTY_PM_TP3OFF               = "ro.pm.tp3off";
static const char *PROPERTY_PM_DDROFF               = "ro.pm.ddroff";
static const char *PROPERTY_PM_MEMC1OFF             = "ro.pm.memc1off";

// This is the default interactive timeout in seconds, which is the time we have to
// wait for the framework to finish broadcasting its ACTION_SCREEN_OFF intent before
// we actually start to power down the platform.
static const int DEFAULT_INTERACTIVE_TIMEOUT = 5;

// This is the default doze timeout in seconds.  The doze time specifies how long
// the Power HAL must wait in the "doze" state prior to entering the off state.
// If the doze timeout is 0, then the system will not enter the doze state but will
// transition to the off state when a no interactivity event is received. If the
// doze time is a negative value, then the system will remain in the doze state
// and will not transition to the off state.
static const int DEFAULT_DOZE_TIMEOUT = 0;

// The interactive timer will be used to delay bringing down the platform
// when the interactivity has stopped.
static timer_t gInteractiveTimer = NULL;

// The doze timer will be used only if the platform has be configured to
// enter an intermediate "doze" mode, prior to entering the off state.
static timer_t gDozeTimer = NULL;

// Global instance of the NexusPower class.
static sp<NexusPower> gNexusPower;

// Prototype declarations...
static int power_set_state(b_powerState toState, b_powerState fromState);
static void power_finish_set_interactive(int on);

static const char *power_to_string[] = {
    "S0",
    "S1",
    "S2",
    "S3",
    "S4",
    "S5"
};

static const char *power_sys_power_state = "/sys/power/state";
static const char *power_standby_state   = "standby";

static b_powerState power_get_state_from_string(char *value)
{
    b_powerState powerOffState = ePowerState_Max;

    if (strcasecmp(value, "1") == 0 || strcasecmp(value, "s1") == 0) {
        powerOffState = ePowerState_S1;
    }
    else if (strcasecmp(value, "2") == 0 || strcasecmp(value, "s2") == 0) {
        powerOffState = ePowerState_S2;
    }
    else if (strcasecmp(value, "3") == 0 || strcasecmp(value, "s3") == 0) {
        powerOffState = ePowerState_S3;
    }
    else if (strcasecmp(value, "5") == 0 || strcasecmp(value, "s5") == 0) {
        powerOffState = ePowerState_S5;
    }
    return powerOffState;
}

static b_powerState power_get_property_off_state()
{
    char value[PROPERTY_VALUE_MAX] = "";

    property_get(PROPERTY_PM_OFFSTATE, value, "s3");
    return power_get_state_from_string(value);
}

static b_powerState power_get_property_doze_state()
{
    char value[PROPERTY_VALUE_MAX] = "";

    property_get(PROPERTY_PM_DOZESTATE, value, "s1");
    return power_get_state_from_string(value);
}

// The interactive timeout is the time we have to wait for the framework to finish broadcasting
// its ACTION_SCREEN_OFF intent before we actually start to power down the platform.
static int power_get_property_interactive_timeout()
{
    int interactiveTimeout = DEFAULT_INTERACTIVE_TIMEOUT;
    char value[PROPERTY_VALUE_MAX] = "";

    property_get(PROPERTY_PM_INTERACTIVE_TIMEOUT, value, NULL);

    if (value[0]) {
        char *endptr;
        interactiveTimeout = strtol(value, &endptr, 10);
        if (*endptr) {
            ALOGE("%s: invalid value \"%s\" for property \"%s\"!!!", __FUNCTION__, value, PROPERTY_PM_INTERACTIVE_TIMEOUT);
        }
    }
    return interactiveTimeout;
}

// Read back the doze timeout (if it exists) and if it is > 0, then it means we need to enter the
// doze state (S1 or S2) and stay there until either:
//     a) Power button is pressed to exit doze and return to active full power-on state.
//     b) No power button is pressed and the timeout expires, then enter power-off state.
static int power_get_property_doze_timeout()
{
    int dozeTimeout = DEFAULT_DOZE_TIMEOUT;
    char value[PROPERTY_VALUE_MAX] = "";

    property_get(PROPERTY_SYS_POWER_DOZE_TIMEOUT, value, NULL);
    if (value[0]) {
        char *endptr;
        dozeTimeout = strtol(value, &endptr, 10);
        if (*endptr) {
            ALOGE("%s: invalid value \"%s\" for property \"persist.sys.power.doze.timeout\"!!!", __FUNCTION__, value);
        }
    }
    return dozeTimeout;
}

static void interactiveTimerCallback(union sigval val __unused)
{
    ALOGV("%s: enter", __FUNCTION__);
    power_finish_set_interactive(false);
}

static void dozeTimerCallback(union sigval val __unused)
{
    int ret;

    b_powerState powerOffState = power_get_property_off_state();
    b_powerState dozeState = power_get_property_doze_state();

    ALOGV("%s: Entering power off state %s...", __FUNCTION__, power_to_string[powerOffState]);
    ret = power_set_state(powerOffState, dozeState);
    LOGI("%s: %s set power state %s", __FUNCTION__, !ret ? "Successfully" : "Could not", power_to_string[powerOffState]);
}

static void power_init(struct power_module *module __unused)
{
    gNexusPower = NexusPower::instantiate();

    if (gNexusPower == NULL) {
        ALOGE("%s: failed!!!", __FUNCTION__);
    }
    else {
        struct sigevent se;

        // Create the interactive timer...
        se.sigev_notify = SIGEV_THREAD;
        se.sigev_notify_function = interactiveTimerCallback;
        se.sigev_notify_attributes = NULL;
        if (timer_create(CLOCK_MONOTONIC, &se, &gInteractiveTimer) != 0) {
            ALOGE("%s: Could not create interactive timer!!!", __FUNCTION__);
            return;
        }

        // Create the doze timer...
        se.sigev_notify = SIGEV_THREAD;
        se.sigev_notify_function = dozeTimerCallback;
        se.sigev_notify_attributes = NULL;
        if (timer_create(CLOCK_MONOTONIC, &se, &gDozeTimer) != 0) {
            ALOGE("%s: Could not create doze timer!!!", __FUNCTION__);
        }
        else {
            ALOGI("%s: succeeded", __FUNCTION__);
        }
    }
}

static int power_set_state_s2()
{
    int ret = 0;
    int fd;
    char buf[80];

    fd = open(power_sys_power_state, O_RDWR);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("%s: Error opening %s [err=%s]!!!", __FUNCTION__, power_sys_power_state, buf);
        ret = -1;
    }
    else {
        ret = (write(fd, power_standby_state, strlen(power_standby_state)) > 0) ? 0 : -1;
        if (ret) {
            ALOGE("%s: Error writing \"%s\" to %s!!!", __FUNCTION__, power_standby_state, power_sys_power_state);
        }
        close(fd);
    }
    return ret;
}

static int power_set_state_s5()
{
    return property_set("sys.powerctl", "shutdown");
}

static int power_set_pmlibservice_state(b_powerState state)
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

    if (state == ePowerState_S1) {
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
        property_get(PROPERTY_PM_USBOFF, value, NULL);
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            usboff = false;
        }
        else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            usboff = true;
        }

        /* ethoff == true means leave ethernet ON during standby */
        property_get(PROPERTY_PM_ETHOFF, value, NULL);
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            ethoff = false;
        }
        else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            ethoff = true;
        }

        /* mocaoff == true means leave MOCA ON during standby */
        property_get(PROPERTY_PM_MOCAOFF, value, NULL);
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            mocaoff = false;
        }
        else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            mocaoff = true;
        }

        /* sataoff == true means leave SATA ON during standby */
        property_get(PROPERTY_PM_SATAOFF, value, NULL);
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            sataoff = false;
        }
        else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            sataoff = true;
        }

        /* tp1off == true means leave CPU thread 1 ON during standby */
        property_get(PROPERTY_PM_TP1OFF, value, NULL);
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            tp1off = false;
        }
        else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            tp1off = true;
        }

        /* tp2off == true means leave CPU thread 2 ON during standby */
        property_get(PROPERTY_PM_TP2OFF, value, NULL);
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            tp2off = false;
        }
        else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            tp2off = true;
        }

        /* tp3off == true means leave CPU thread 3 ON during standby */
        property_get(PROPERTY_PM_TP3OFF, value, NULL);
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            tp3off = false;
        }
        else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            tp3off = true;
        }

        /* ddroff == true means leave DDR timeout to default during standby */
        property_get(PROPERTY_PM_DDROFF, value, NULL);
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
            ddroff = false;
        }
        else if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
            ddroff = true;
        }

        /* memc1off == true means enable MEMC1 status during standby */
        property_get(PROPERTY_PM_MEMC1OFF, value, NULL);
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
    else if (state == ePowerState_S0) {
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
    
static int power_set_state(b_powerState toState, b_powerState fromState)
{
    int rc = 0;
    switch (toState)
    {
        case ePowerState_S0: {
            if (fromState == ePowerState_S1) {
                power_set_pmlibservice_state(toState);
            }

            if (rc == 0 && gNexusPower.get()) {
                rc = gNexusPower->setPowerState(toState);
            }

            if ((fromState != ePowerState_S3) && (fromState != ePowerState_S5)) {
                // Release the CPU wakelock as the system should be back up now...
                ALOGV("%s: Releasing \"%s\" wake lock...", __FUNCTION__, WAKE_LOCK_ID);
                release_wake_lock(WAKE_LOCK_ID);
            }
        } break;

        case ePowerState_S1: {
            if (gNexusPower.get()) {
                rc = gNexusPower->setPowerState(toState);
            }
            if (rc == 0) {
                power_set_pmlibservice_state(toState);
            }
        } break;

        case ePowerState_S2: {
            if (gNexusPower.get()) {
                rc = gNexusPower->setPowerState(toState);
            }
            if (rc == 0) {
                rc = power_set_state_s2();
            }
        } break;

        case ePowerState_S3: {
            if (gNexusPower.get()) {
                rc = gNexusPower->setPowerState(toState);
            }

            // Release the CPU wakelock to allow the CPU to suspend...
            ALOGV("%s: Releasing \"%s\" wake lock...", __FUNCTION__, WAKE_LOCK_ID);
            release_wake_lock(WAKE_LOCK_ID);
        } break;

        case ePowerState_S5: {
            if (gNexusPower.get()) {
                rc = gNexusPower->setPowerState(toState);
            }
            if (rc == 0) {
                rc = power_set_state_s5();
            }
        } break;

        default: {
            ALOGE("%s: Invalid Power State %s!!!", __FUNCTION__, power_to_string[toState]);
            rc = BAD_VALUE;
        } break;
    }
    return rc;
}

static void power_finish_set_interactive(int on)
{
    int ret;
    struct itimerspec ts;
    b_powerState fromState;
    b_powerState toState;
    b_powerState powerOffState = power_get_property_off_state();
    b_powerState dozeState = power_get_property_doze_state();
    int dozeTimeout = power_get_property_doze_timeout();

    ALOGV("%s: %s", __FUNCTION__, on ? "ON" : "OFF");

    if (on) {
        if (gInteractiveTimer) {
            // Disarm the interactive timer...
            ts.it_value.tv_sec = 0;
            ts.it_value.tv_nsec = 0;
            ts.it_interval.tv_sec = 0;
            ts.it_interval.tv_nsec = 0;
            timer_settime(gInteractiveTimer, 0, &ts, NULL);
        }

        toState = ePowerState_S0;
        fromState = powerOffState;

        if (dozeTimeout < 0) {
            fromState = dozeState;
        }
        else if (dozeTimeout > 0 && gDozeTimer) {
            timer_gettime(gDozeTimer, &ts);

            if (ts.it_value.tv_sec > 0 || ts.it_value.tv_nsec > 0) {
                // Disarm the doze timer...
                ts.it_value.tv_sec = 0;
                ts.it_value.tv_nsec = 0;
                ts.it_interval.tv_sec = 0;
                ts.it_interval.tv_nsec = 0;
                timer_settime(gDozeTimer, 0, &ts, NULL);
                fromState = dozeState;
            }
        }
    }
    else {
        fromState = ePowerState_S0;
        if (dozeTimeout != 0) {
            toState = dozeState;

            if (dozeTimeout < 0) {
                ALOGV("%s: Dozing in power state %s indefinitely...", __FUNCTION__, power_to_string[toState]);
            }
            else if (gDozeTimer) {
                // Arm the doze timer...
                ALOGV("%s: Dozing in power state %s for %ds...", __FUNCTION__, power_to_string[toState], dozeTimeout);
                ts.it_value.tv_sec = dozeTimeout;
                ts.it_value.tv_nsec = 0;
                ts.it_interval.tv_sec = 0;
                ts.it_interval.tv_nsec = 0;
                timer_settime(gDozeTimer, 0, &ts, NULL);
            }
        }
        else {
            toState = powerOffState;
        }
    }
    ret = power_set_state(toState, fromState);
    LOGI("%s: %s set power state %s", __FUNCTION__, !ret ? "Successfully" : "Could not", power_to_string[toState]);
}

static void power_set_interactive(struct power_module *module __unused, int on)
{
    ALOGV("%s: %s", __FUNCTION__, on ? "ON" : "OFF");

    if (on) {
        power_finish_set_interactive(on);
    }
    else if (gInteractiveTimer) {
        // Start interactive timer
        struct itimerspec ts;
        int interactiveTimeout = power_get_property_interactive_timeout();

        ALOGV("%s: Waiting %ds before actually setting the power state...", __FUNCTION__, interactiveTimeout);
        ts.it_value.tv_sec = interactiveTimeout;
        ts.it_value.tv_nsec = 0;
        ts.it_interval.tv_sec = 0;
        ts.it_interval.tv_nsec = 0;
        timer_settime(gInteractiveTimer, 0, &ts, NULL);

        // Keep the CPU alive until we are ready to suspend it...
        ALOGV("%s: Acquire \"%s\" wake lock...", __FUNCTION__, WAKE_LOCK_ID);
        acquire_wake_lock(PARTIAL_WAKE_LOCK, WAKE_LOCK_ID);
    }
}

static void power_hint(struct power_module *module __unused, power_hint_t hint, void *data __unused) {
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
