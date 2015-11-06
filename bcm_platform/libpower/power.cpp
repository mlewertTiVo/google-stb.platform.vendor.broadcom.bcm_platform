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
#include <poll.h>

#include <cutils/properties.h>
#include <utils/Timers.h>
#include <hardware/hardware.h>
#include <hardware/power.h>
#include <hardware_legacy/power.h>
#include "PmLibService.h"
#include "droid_pm.h"

using namespace android;

// Keep the CPU alive wakelock
static const char *WAKE_LOCK_ID = "PowerHAL";
static bool wakeLockAcquired = false;

// Property definitions (max length 32)...
static const char *PROPERTY_SYS_POWER_DOZE_TIMEOUT        = "persist.sys.power.doze.timeout";
static const char *PROPERTY_PM_DOZESTATE                  = "ro.pm.dozestate";
static const char *PROPERTY_PM_OFFSTATE                   = "ro.pm.offstate";
static const char *PROPERTY_PM_ETH_EN                     = "ro.pm.eth_en";
static const char *PROPERTY_PM_MOCA_EN                    = "ro.pm.moca_en";
static const char *PROPERTY_PM_SATA_EN                    = "ro.pm.sata_en";
static const char *PROPERTY_PM_TP1_EN                     = "ro.pm.tp1_en";
static const char *PROPERTY_PM_TP2_EN                     = "ro.pm.tp2_en";
static const char *PROPERTY_PM_TP3_EN                     = "ro.pm.tp3_en";
static const char *PROPERTY_PM_DDR_PM_EN                  = "ro.pm.ddr_pm_en";
static const char *PROPERTY_PM_CPU_FREQ_SCALE_EN          = "ro.pm.cpufreq_scale_en";

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

// This is the default doze timeout in seconds.  The doze time specifies how long
// the Power HAL must wait in the "doze" state prior to entering the off state.
// If the doze timeout is 0, then the system will not enter the doze state but will
// transition to the off state when a no interactivity event is received. If the
// doze time is a negative value, then the system will remain in the doze state
// and will not transition to the off state.
static const int DEFAULT_DOZE_TIMEOUT = 0;

// This is the default timeout for which the suspend monitor thread will poll the
// PM kernel driver for an indication that the system is about to suspend.
static const int DEFAULT_SUSPEND_MONITOR_THREAD_TIMEOUT_MS = 1000;

// The doze timer will be used only if the platform has be configured to
// enter an intermediate "doze" mode, prior to entering the off state.
static timer_t gDozeTimer = NULL;

// Global instance of the NexusPower class.
static sp<NexusPower> gNexusPower;

// Power device driver file descriptor.
static int gPowerFd = -1;

// Flag to signal when to exit the suspend monitor thread.
static volatile bool gPowerSuspendMonitorThreadExit = false;

// Global Power HAL Mutex
Mutex gLock("PowerHAL Lock");

typedef status_t (*powerFinishFunction_t)(void);

// Prototype declarations...
static status_t power_set_poweroff_state();
static status_t power_finish_set_interactive();
static void power_finish_set_no_interactive(b_powerState offState, powerFinishFunction_t powerFunction);


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

// Read back the doze timeout (if it exists) and if it is > 0, then it means we need to enter the
// doze state (e.g. S0.5 or S1) and stay there until either:
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

static b_powerState power_get_sleep_state()
{
    return (power_get_property_doze_timeout() != 0) ? power_get_property_doze_state() : power_get_property_off_state();
}

static void dozeTimerCallback(union sigval val)
{
    b_powerState powerOffState = (b_powerState)val.sival_int;

    power_finish_set_no_interactive(powerOffState, power_set_poweroff_state);
}

static void acquireWakeLock()
{
    Mutex::Autolock autoLock(gLock);

    if (wakeLockAcquired == false) {
        ALOGV("%s: Acquiring \"%s\" wake lock...", __FUNCTION__, WAKE_LOCK_ID);
        acquire_wake_lock(PARTIAL_WAKE_LOCK, WAKE_LOCK_ID);
        wakeLockAcquired = true;
    }
}

static void releaseWakeLock()
{
    Mutex::Autolock autoLock(gLock);

    if (wakeLockAcquired == true) {
        ALOGV("%s: Releasing \"%s\" wake lock...", __FUNCTION__, WAKE_LOCK_ID);
        release_wake_lock(WAKE_LOCK_ID);
        wakeLockAcquired = false;
    }
}

static void power_init(struct power_module *module __unused)
{
    gNexusPower = NexusPower::instantiate();

    if (gNexusPower.get() == NULL) {
        ALOGE("%s: failed!!!", __FUNCTION__);
    }
    else {
        struct sigevent se;

        // Create the doze timer...
        se.sigev_value.sival_int = power_get_property_off_state();
        se.sigev_notify = SIGEV_THREAD;
        se.sigev_notify_function = dozeTimerCallback;
        se.sigev_notify_attributes = NULL;
        if (timer_create(CLOCK_MONOTONIC, &se, &gDozeTimer) != 0) {
            ALOGE("%s: Could not create doze timer!!!", __FUNCTION__);
        }

        int fd;
        const char *devname = getenv("NEXUS_WAKE_DEVICE_NODE");

        if (!devname) devname = "/dev/wake0";
        fd = open(devname, O_RDONLY);

        if (fd < 0) {
            ALOGE("%s: Could not open %s PM driver!!!", __FUNCTION__, devname);
        }
        else {
            gPowerFd = fd;
        }
    }
}

static status_t power_set_pmlibservice_state(b_powerState state)
{
    status_t status;
    IPmLibService::pmlib_state_t pmlib_orig_state;
    IPmLibService::pmlib_state_t pmlib_state;
    sp<IBinder> binder = defaultServiceManager()->getService(IPmLibService::descriptor);
    sp<IPmLibService> service = interface_cast<IPmLibService>(binder);

    if (service.get() == NULL) {
        ALOGE("%s: Cannot get \"PmLibService\" service!!!", __FUNCTION__);
        return NO_INIT;
    }

    status = service->getState(&pmlib_state);
    if (status != NO_ERROR) {
        ALOGE("%s: Could not get PmLibService state [status=%d]!!!", __FUNCTION__, status);
        return status;
    }

    pmlib_orig_state = pmlib_state;

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

    if (memcmp(&pmlib_orig_state, &pmlib_state, sizeof(pmlib_state))) {
        status = service->setState(&pmlib_state);
    }
    return status;
}

static status_t power_set_state_s0()
{
    status_t status;

    status = power_set_pmlibservice_state(ePowerState_S0);

    if (gNexusPower.get()) {
        status = gNexusPower->setPowerState(ePowerState_S0);
    }

    // Release the CPU wakelock as the system should be back up now...
    releaseWakeLock();
    return status;
}

static status_t power_set_state_s05()
{
    status_t status = NO_ERROR;
    char buf[80];

    if (status == NO_ERROR && gNexusPower.get()) {
        status = gNexusPower->setPowerState(ePowerState_S05);
    }

    if (status == NO_ERROR) {
        power_set_pmlibservice_state(ePowerState_S05);
    }
    return status;
}

static status_t power_set_state_s1()
{
    status_t status = NO_ERROR;
    char buf[80];

    if (status == NO_ERROR && gNexusPower.get()) {
        status = gNexusPower->setPowerState(ePowerState_S1);
    }

    if (status == NO_ERROR) {
        power_set_pmlibservice_state(ePowerState_S1);
    }
    return status;
}

static status_t power_set_state_s2()
{
    status_t status = NO_ERROR;
    int fd;
    char buf[80];

    acquireWakeLock();  // Prevent the system from suspending to RAM

    if (gNexusPower.get()) {
        status = gNexusPower->setPowerState(ePowerState_S2);
    }

    if (status == NO_ERROR) {
        if (gPowerFd != -1) {
            status = ioctl(gPowerFd, BRCM_IOCTL_STANDBY);
            if (status != NO_ERROR) {
                strerror_r(errno, buf, sizeof(buf));
                ALOGE("%s: Error trying to enter S2 standby [%s]!!!", __FUNCTION__, buf);
                releaseWakeLock();   // Must release to allow a retry by the kernel suspend framework
            }
        }
        else {
            ALOGE("%s: %s driver not opened!!!", __FUNCTION__, DROID_PM_DRV_NAME);
            status = NO_INIT;
        }
    }
    return status;
}

static status_t power_set_state_s3()
{
    status_t status = NO_ERROR;
    char buf[80];

    if (gNexusPower.get()) {
        status = gNexusPower->setPowerState(ePowerState_S3);
    }

    if (status == NO_ERROR) {
        if (gPowerFd != -1) {
            status = ioctl(gPowerFd, BRCM_IOCTL_SUSPEND);
            if (status != NO_ERROR) {
                strerror_r(errno, buf, sizeof(buf));
                ALOGE("%s: Error trying to enter S3 deep sleep [%s]!!!", __FUNCTION__, buf);
            }
            else {
                int pm_status;
                status = ioctl(gPowerFd, BRCM_IOCTL_GET_STATUS, &pm_status);
                if (status != NO_ERROR) {
                    strerror_r(errno, buf, sizeof(buf));
                    ALOGE("%s: Error trying to read PM status [%s]!!!", __FUNCTION__, buf);
                }
                else {
                    status = pm_status;
                    if (status != NO_ERROR) {
                        strerror_r(abs(status), buf, sizeof(buf));
                        ALOGE("%s: Error trying to enter S3 deep sleep [%s]!!!", __FUNCTION__, buf);
                    }
                }
            }
        }
        else {
            ALOGE("%s: %s driver not opened!!!", __FUNCTION__, DROID_PM_DRV_NAME);
        }
    }
    return status;
}

static status_t power_set_state_s5()
{
    return system("/system/bin/svc power shutdown");
}

static status_t power_set_state(b_powerState toState)
{
    status_t status = NO_ERROR;

    switch (toState)
    {
        case ePowerState_S0: {
            status = power_set_state_s0();
        } break;

        case ePowerState_S05: {
            status = power_set_state_s05();
        } break;

        case ePowerState_S1: {
            status = power_set_state_s1();
        } break;

        case ePowerState_S2: {
            status = power_set_state_s2();
        } break;

        case ePowerState_S3: {
            status = power_set_state_s3();
        } break;

        case ePowerState_S5: {
            status = power_set_state_s5();
        } break;

        default: {
            ALOGE("%s: Invalid Power State!!!", __FUNCTION__);
            return BAD_VALUE;
        } break;
    }

    if (status == NO_ERROR) {
        ALOGI("%s: Successfully set power state %s", __FUNCTION__, NexusIPCClientBase::getPowerString(toState));
    }
    else {
        ALOGE("%s: Could not set power state %s!!!", __FUNCTION__, NexusIPCClientBase::getPowerString(toState));
    }
    return status;
}

static status_t power_set_poweroff_state()
{
    status_t status;
    b_powerState powerOffState = power_get_property_off_state();

    status = power_set_state(powerOffState);
    return status;
}

static status_t power_set_sleep_state()
{
    status_t status;
    struct itimerspec ts;
    b_powerState sleepState = power_get_sleep_state();
    int dozeTimeout = power_get_property_doze_timeout();

    if (dozeTimeout < 0) {
        ALOGI("%s: Dozing in power state %s indefinitely...", __FUNCTION__, NexusIPCClientBase::getPowerString(sleepState));
    }
    else if (dozeTimeout > 0 && gDozeTimer) {
        timer_gettime(gDozeTimer, &ts);

        // Arm the doze timer...
        ALOGI("%s: Dozing in power state %s for %ds...", __FUNCTION__, NexusIPCClientBase::getPowerString(sleepState), dozeTimeout);
        ts.it_value.tv_sec = dozeTimeout;
        ts.it_value.tv_nsec = 0;
        ts.it_interval.tv_sec = 0;
        ts.it_interval.tv_nsec = 0;
        timer_settime(gDozeTimer, 0, &ts, NULL);
    }
    status = power_set_state(sleepState);
    return status;
}

static void *power_suspend_monitor_thread(void *arg)
{
    int ret;
    struct pollfd pollFd;
    powerFinishFunction_t powerFunction = reinterpret_cast<powerFinishFunction_t>(arg);

    pollFd.fd = gPowerFd;
    pollFd.events = POLLIN | POLLRDNORM;

    while (gPowerFd >= 0) {
        gLock.lock();
        if (gPowerSuspendMonitorThreadExit == true) {
            gLock.unlock();
            break;
        }
        gLock.unlock();
        ret = poll(&pollFd, 1, DEFAULT_SUSPEND_MONITOR_THREAD_TIMEOUT_MS);
        if (ret == 0) {
            ALOGW("%s: Timed out waiting for device to suspend - retrying...", __FUNCTION__);
        }
        else if (ret < 0) {
            ALOGE("%s: Error polling PM device!!!", __FUNCTION__);
            break;
        }
        else if (pollFd.revents & (POLLIN | POLLRDNORM)) {
            if (powerFunction() == NO_ERROR) {
                ALOGD("%s: Successfully finished setting power state.", __FUNCTION__);
                break;
            }
        }
        else {
            ALOGW("%s: Not ready to suspend - retrying...", __FUNCTION__);
        }
    }
    ALOGV("%s: Exiting power_suspend_monitor_thread...", __FUNCTION__);
    return NULL;
}

static void *power_standby_finish_thread(void *arg)
{
    powerFinishFunction_t powerFunction = reinterpret_cast<powerFinishFunction_t>(arg);

    if (powerFunction() == NO_ERROR) {
        ALOGD("%s: Successfully finished setting power state.", __FUNCTION__);
    }
    return NULL;
}

static void power_finish_set_no_interactive(b_powerState toState, powerFinishFunction_t powerFunction)
{
    pthread_t thread;
    pthread_attr_t attr;
    void *(*pthread_function)(void *);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    // Spawn a thread that monitors for kernel suspend callbacks in S2/S3 standby only...
    if (toState == ePowerState_S2 || toState == ePowerState_S3) {
        // Ensure we release a wake-lock to allow the system to begin suspending.  We then
        // use this notification to place Nexus in to standby (suspend in S2/S3 modes only).
        releaseWakeLock();

        gLock.lock();
        gPowerSuspendMonitorThreadExit = false;
        gLock.unlock();
        pthread_function = power_suspend_monitor_thread;
    }
    // Spawn a thread that just finishes the standby sequence...
    else {
        // Ensure we acquire a wake-lock to prevent the system from suspending *BEFORE* we have
        // placed Nexus in to standby (we should not suspend in S0.5/S1/S5 modes anyway).
        acquireWakeLock();

        pthread_function = power_standby_finish_thread;
    }

    ALOGI("%s: Entering power state %s...", __FUNCTION__, NexusIPCClientBase::getPowerString(toState));

    if (pthread_create(&thread, &attr, pthread_function, reinterpret_cast<void *>(powerFunction)) == 0) {
        ALOGV("%s: Successfully created a standby thread!", __FUNCTION__);
    }
    else {
        ALOGE("%s: Could not create a standby thread!!!", __FUNCTION__);
    }
    pthread_attr_destroy(&attr);
}

static status_t power_finish_set_interactive()
{
    status_t status;

    if (power_get_property_doze_timeout() > 0 && gDozeTimer) {
        struct itimerspec ts;
        timer_gettime(gDozeTimer, &ts);

        if (ts.it_value.tv_sec > 0 || ts.it_value.tv_nsec > 0) {
            // Disarm the doze timer...
            ts.it_value.tv_sec = 0;
            ts.it_value.tv_nsec = 0;
            ts.it_interval.tv_sec = 0;
            ts.it_interval.tv_nsec = 0;
            timer_settime(gDozeTimer, 0, &ts, NULL);
        }
    }
    status = power_set_state(ePowerState_S0);
    return status;
}

static void power_set_interactive(struct power_module *module __unused, int on)
{
    ALOGI("%s: %s", __FUNCTION__, on ? "ON" : "OFF");

    if (on) {
        gLock.lock();
        gPowerSuspendMonitorThreadExit = true;
        gLock.unlock();

        if (gNexusPower.get()) {
            gNexusPower->preparePowerState(ePowerState_S0);
        }

        power_finish_set_interactive();

        if (gNexusPower.get()) {
            gNexusPower->setVideoOutputsState(ePowerState_S0);
        }
    }
    else {
        b_powerState sleepState = power_get_sleep_state();

        if (gNexusPower.get()) {
            gNexusPower->preparePowerState(sleepState);
            gNexusPower->setVideoOutputsState(sleepState);
        }

        power_finish_set_no_interactive(sleepState, power_set_sleep_state);
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
