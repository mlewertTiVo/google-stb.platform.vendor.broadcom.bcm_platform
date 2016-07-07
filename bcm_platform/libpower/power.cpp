/*
 * Copyright (C) 2015 The Android Open Source Project
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
static const char *PROPERTY_PM_ETH_EN               = "ro.pm.eth_en";
static const char *PROPERTY_PM_MOCA_EN              = "ro.pm.moca_en";
static const char *PROPERTY_PM_SATA_EN              = "ro.pm.sata_en";
static const char *PROPERTY_PM_TP1_EN               = "ro.pm.tp1_en";
static const char *PROPERTY_PM_TP2_EN               = "ro.pm.tp2_en";
static const char *PROPERTY_PM_TP3_EN               = "ro.pm.tp3_en";
static const char *PROPERTY_PM_DDR_PM_EN            = "ro.pm.ddr_pm_en";
static const char *PROPERTY_PM_CPU_FREQ_SCALE_EN    = "ro.pm.cpufreq_scale_en";

// Property defaults
static const char *DEFAULT_PROPERTY_PM_DOZESTATE          = "S0.5";
static const char *DEFAULT_PROPERTY_PM_OFFSTATE           = "S2";
static const int8_t DEFAULT_PROPERTY_PM_ETH_EN            = 1;     // Enable Ethernet during standby
static const int8_t DEFAULT_PROPERTY_PM_MOCA_EN           = 0;     // Disable MOCA during standby
static const int8_t DEFAULT_PROPERTY_PM_SATA_EN           = 0;     // Disable SATA during standby
static const int8_t DEFAULT_PROPERTY_PM_TP1_EN            = 0;     // Disable CPU1 during standby
static const int8_t DEFAULT_PROPERTY_PM_TP2_EN            = 0;     // Disable CPU2 during standby
static const int8_t DEFAULT_PROPERTY_PM_TP3_EN            = 0;     // Disable CPU3 during standby
static const int8_t DEFAULT_PROPERTY_PM_DDR_PM_EN         = 1;     // Enabled DDR power management during standby
static const int8_t DEFAULT_PROPERTY_PM_CPU_FREQ_SCALE_EN = 1;     // Enable CPU frequency scaling during standby

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

static const char *power_sys_power_state = "/sys/power/state";
static const char *power_standby_state   = "standby";

static b_powerState power_get_state_from_string(char *value)
{
    b_powerState powerOffState = ePowerState_Max;

    if (strcasecmp(value, "0") == 0 || strcasecmp(value, "s0") == 0) {
        powerOffState = ePowerState_S0;
    }
    else if (strcasecmp(value, "0.5") == 0 || strcasecmp(value, "s0.5") == 0) {
        powerOffState = ePowerState_S05;
    }
    else if (strcasecmp(value, "1") == 0 || strcasecmp(value, "s1") == 0) {
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

    property_get(PROPERTY_PM_OFFSTATE, value, DEFAULT_PROPERTY_PM_OFFSTATE);
    return power_get_state_from_string(value);
}

static b_powerState power_get_property_doze_state()
{
    char value[PROPERTY_VALUE_MAX] = "";

    property_get(PROPERTY_PM_DOZESTATE, value, DEFAULT_PROPERTY_PM_DOZESTATE);
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

    ALOGI("%s: Entering power off state %s...", __FUNCTION__, NexusIPCClientBase::getPowerString(powerOffState));
    ret = power_set_state(powerOffState, dozeState);
    ALOGI("%s: %s set power state %s", __FUNCTION__, !ret ? "Successfully" : "Could not", NexusIPCClientBase::getPowerString(powerOffState));
}

static void power_init(struct power_module *module __unused)
{
    gNexusPower = NexusPower::instantiate();

    if (gNexusPower.get() == NULL) {
        ALOGE("%s: failed!!!", __FUNCTION__);
    }
    else {
        struct sigevent se;
        memset(&se, 0, sizeof(se));

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
    return system("/system/bin/svc power shutdown");
}

static int power_set_pmlibservice_state(b_powerState state)
{
    int rc = 0;
    IPmLibService::pmlib_state_t pmlib_state;
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

    if (state == ePowerState_S05 || state == ePowerState_S1) {
        /* eth_en == true means leave ethernet ON during standby */
        bool eth_en = property_get_bool(PROPERTY_PM_ETH_EN, DEFAULT_PROPERTY_PM_ETH_EN);

        /* moca_en == true means leave MOCA ON during standby */
        bool moca_en = property_get_bool(PROPERTY_PM_MOCA_EN, DEFAULT_PROPERTY_PM_MOCA_EN);

        /* sata_en == true means leave SATA ON during standby */
        bool sata_en = property_get_bool(PROPERTY_PM_SATA_EN, DEFAULT_PROPERTY_PM_SATA_EN);

        /* tp1_en == true means leave CPU thread 1 ON during standby */
        bool tp1_en = property_get_bool(PROPERTY_PM_TP1_EN, DEFAULT_PROPERTY_PM_TP1_EN);

        /* tp2_en == true means leave CPU thread 2 ON during standby */
        bool tp2_en = property_get_bool(PROPERTY_PM_TP2_EN, DEFAULT_PROPERTY_PM_TP2_EN);

        /* tp3_en == true means leave CPU thread 3 ON during standby */
        bool tp3_en = property_get_bool(PROPERTY_PM_TP3_EN, DEFAULT_PROPERTY_PM_TP3_EN);

        /* ddr_pm_en == true means enable DDR power management during standby */
        bool ddr_pm_en = property_get_bool(PROPERTY_PM_DDR_PM_EN, DEFAULT_PROPERTY_PM_DDR_PM_EN);

        /* cpufreq_scale_en == true means enable CPU frequency scaling during standby */
        bool cpufreq_scale_en = property_get_bool(PROPERTY_PM_CPU_FREQ_SCALE_EN, DEFAULT_PROPERTY_PM_CPU_FREQ_SCALE_EN);

        pmlib_state.enet_en          = eth_en;
        pmlib_state.moca_en          = moca_en;
        pmlib_state.sata_en          = sata_en;
        pmlib_state.tp1_en           = tp1_en;
        pmlib_state.tp2_en           = tp2_en;
        pmlib_state.tp3_en           = tp3_en;
        pmlib_state.cpufreq_scale_en = cpufreq_scale_en;
        pmlib_state.ddr_pm_en        = ddr_pm_en;
    }
    else if (state == ePowerState_S0) {
        pmlib_state.enet_en          = true;
        pmlib_state.moca_en          = true;
        pmlib_state.sata_en          = true;
        pmlib_state.tp1_en           = true;
        pmlib_state.tp2_en           = true;
        pmlib_state.tp3_en           = true;
        pmlib_state.cpufreq_scale_en = false;
        pmlib_state.ddr_pm_en        = false;
    }
    return service->setState(&pmlib_state);
}
    
static int power_set_state(b_powerState toState, b_powerState fromState)
{
    int rc = 0;
    switch (toState)
    {
        case ePowerState_S0: {
            if (fromState == ePowerState_S05 || fromState == ePowerState_S1) {
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

        case ePowerState_S05: {
            if (gNexusPower.get()) {
                rc = gNexusPower->setPowerState(toState);
            }
            if (rc == 0) {
                power_set_pmlibservice_state(toState);
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
            rc = power_set_state_s5();
        } break;

        default: {
            ALOGE("%s: Invalid Power State %s!!!", __FUNCTION__, NexusIPCClientBase::getPowerString(toState));
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
                ALOGI("%s: Dozing in power state %s indefinitely...", __FUNCTION__, NexusIPCClientBase::getPowerString(toState));
            }
            else if (gDozeTimer) {
                // Arm the doze timer...
                ALOGI("%s: Dozing in power state %s for %ds...", __FUNCTION__, NexusIPCClientBase::getPowerString(toState), dozeTimeout);
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
    ALOGI("%s: %s set power state %s", __FUNCTION__, !ret ? "Successfully" : "Could not", NexusIPCClientBase::getPowerString(toState));
}

static void power_set_interactive(struct power_module *module __unused, int on)
{
    ALOGI("%s: %s", __FUNCTION__, on ? "ON" : "OFF");

    if (on) {
        if (gNexusPower.get()) {
            gNexusPower->preparePowerState(ePowerState_S0);
        }
        power_finish_set_interactive(on);
        if (gNexusPower.get()) {
            gNexusPower->setVideoOutputsState(ePowerState_S0);
        }
    }
    else {
        b_powerState offState = power_get_property_off_state();

        if (gNexusPower.get()) {
            gNexusPower->preparePowerState(offState);
            gNexusPower->setVideoOutputsState(offState);
        }

        if (gInteractiveTimer) {
            // Start interactive timer
            struct itimerspec ts;
            int interactiveTimeout;
            int dozeTimeout = power_get_property_doze_timeout();

            // If we want to enter S5 (aka cold boot), then we can enter immediately as
            // the system will begin its slow shutdown process...
            if (dozeTimeout == 0 && offState == ePowerState_S5) {
                interactiveTimeout = 0;
                ts.it_value.tv_nsec = 1;
            }
            else {
                interactiveTimeout = power_get_property_interactive_timeout();
                ts.it_value.tv_nsec = 0;
            }
            ALOGI("%s: Waiting %ds before actually setting the power state...", __FUNCTION__, interactiveTimeout);
            ts.it_value.tv_sec = interactiveTimeout;
            ts.it_interval.tv_sec = 0;
            ts.it_interval.tv_nsec = 0;
            timer_settime(gInteractiveTimer, 0, &ts, NULL);
        }
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

static void power_set_feature(struct power_module *module __unused, feature_t feature, int state)
{
    ALOGV("%s: feature=%d, state=%d", __FUNCTION__, feature, state);
}

static struct hw_module_methods_t power_module_methods = {
    open: NULL
};

struct power_module HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        module_api_version: POWER_MODULE_API_VERSION_0_3,
        hal_api_version: HARDWARE_HAL_API_VERSION,
        id: POWER_HARDWARE_MODULE_ID,
        name: "Brcmstb Power HAL",
        author: "Broadcom Corp",
        methods: &power_module_methods,
        dso: 0,
        reserved: {0}
    },
    init: power_init,
    setInteractive: power_set_interactive,
    powerHint: power_hint,
    setFeature: power_set_feature
};
