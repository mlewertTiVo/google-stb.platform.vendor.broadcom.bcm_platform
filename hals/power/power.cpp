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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/ethtool.h>
#include <linux/if.h>
#include <linux/socket.h>

#include <cutils/properties.h>
#include <utils/Condition.h>
#include <utils/Mutex.h>
#include <utils/Timers.h>
#include <hardware/hardware.h>
#include <hardware/power.h>
#include <hardware_legacy/power.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "vendor_bcm_props.h"

// Keep the CPU alive wakelock
static const char *WAKE_LOCK_ID = "PowerHAL";
static bool wakeLockAcquired = false;

// Indicates whether we have begun a shut-down sequence
static bool shutdownStarted = false;

// Lid Switch boolean states
static const bool SW_LID_STATE_DOWN = true;
static const bool SW_LID_STATE_UP = false;

// Maximum length of character buffer
static const int BUF_SIZE = 64;

static const char *powerNetInterfaces[] = {
    "gphy",
    "eth0"
};

// Property defaults
static const char * DEFAULT_BCM_PERSIST_POWER_SYS_DOZESTATE     = "S0.5";
static const char * DEFAULT_BCM_PERSIST_POWER_SYS_OFFSTATE      = "S2";
static const int8_t DEFAULT_BCM_RO_POWER_ETH_EN                 = 1;     // Enable Ethernet during standby
static const int8_t DEFAULT_BCM_RO_POWER_MOCA_EN                = 0;     // Disable MOCA during standby
static const int8_t DEFAULT_BCM_RO_POWER_SATA_EN                = 0;     // Disable SATA during standby
static const int8_t DEFAULT_BCM_RO_POWER_TP1_EN                 = 0;     // Disable CPU1 during standby
static const int8_t DEFAULT_BCM_RO_POWER_TP2_EN                 = 0;     // Disable CPU2 during standby
static const int8_t DEFAULT_BCM_RO_POWER_TP3_EN                 = 0;     // Disable CPU3 during standby
static const int8_t DEFAULT_BCM_RO_POWER_DDR_PM_EN              = 1;     // Enabled DDR power management during standby
static const int8_t DEFAULT_BCM_RO_POWER_CPU_FREQ_SCALE_EN      = 1;     // Enable CPU frequency scaling during standby
static const int8_t DEFAULT_BCM_RO_POWER_WOL_EN                 = 1;     // Enable Linux wake up by the WoLAN event
static const int8_t DEFAULT_BCM_RO_POWER_WOL_SCREEN_ON_EN       = 0;     // Disable Android screen on WoLAN event
static const int8_t DEFAULT_BCM_PERSIST_POWER_SCREEN_ON         = 1;     // Turn screen on at boot
static const int8_t DEFAULT_BCM_PERSIST_POWER_KEEP_SCREEN_STATE = 0;     // Don't store screen state for next boot

// Sysfs paths
static const char * SYS_MAP_MEM_TO_S2                     = "/sys/devices/platform/droid_pm/map_mem_to_s2";
static const char * SYS_FULL_WOL_WAKEUP                   = "/sys/devices/platform/droid_pm/full_wol_wakeup";
static const char * SYS_SW_LID                            = "/sys/devices/platform/droid_pm/sw_lid";
static const char * SYS_CLASS_NET                         = "/sys/class/net";

// This is the default WoL monitor timeout in seconds.
static const int DEFAULT_WOL_MONITOR_TIMEOUT = 4;

// This is the default doze timeout in seconds.  The doze time specifies the
// minimum time, in seconds, the Power HAL must wait in the "doze" state after
// a non-interactive event is received, prior to entering the off state. If the
// doze time is a negative value, then the system will remain in the doze state
// and will not transition to the off state.
static const int DEFAULT_DOZE_TIMEOUT = 0;

// The wake timeout specifies the minimum time the system will stay awake in the
// doze state after being woken up by a timer or external event.
// If the wake timeout is a negative value, the system will remain in
// the doze state and will not transition to the off state.
static const int DEFAULT_WAKE_TIMEOUT = 10;

// This is the default timeout for which the event monitor thread will poll the
// PM kernel driver for an indication that the system is about to suspend/resume/shutdown.
static const int DEFAULT_EVENT_MONITOR_THREAD_TIMEOUT_MS = 1000;

// This is the maximum number of event file descriptors that can be epolled.
static const int MAX_NUM_EVENT_FILE_DESCRIPTORS = 2;

// This is the maximum number of events that can be received and queued in
// the event monitor thread.
static const int MAX_NUM_EVENT_MONITOR_THREAD_EVENTS = 2;

// The doze timer will be used only if the platform has be configured to
// enter an intermediate "doze" mode, prior to entering the off state.
static timer_t gDozeTimer = NULL;

// Global instance of the NexusPower class.
static sp<NexusPower> gNexusPower;

// Global power state
static nxwrap_pwr_state gPowerState;

// Power device driver file descriptor.
static int gPowerFd = -1;

// Event file descriptors for reading back events from the power driver and HAL.
static int gPowerEventFd = -1;
static int gPowerEventMonitorFd = -1;

// Power HAL monitor thread events.
//
// NOTE: we set the values to be the maximum possible eventfd_t values that can
// be passed in to allow the eventfd_write() call to block if we attempt to
// dispatch multiple events before eventfd_read() is called.  This ensures
// that the interactive state is dispatched and acted upon and that we don't
// miss an event.
static const eventfd_t POWER_MONITOR_EVENT_OVFLOW          = (UINT64_MAX);
static const eventfd_t POWER_MONITOR_EVENT_MAX             = (POWER_MONITOR_EVENT_OVFLOW-1);
static const eventfd_t POWER_MONITOR_EVENT_INTERACTIVE_OFF = (POWER_MONITOR_EVENT_MAX-1);
static const eventfd_t POWER_MONITOR_EVENT_INTERACTIVE_ON  = (POWER_MONITOR_EVENT_MAX-2);

static NxWrap *gNxWrap = NULL;

// Global Power HAL Mutex
Mutex gLock("PowerHAL Lock");

// Prototype declarations...
static status_t power_set_pmlibservice_state(nxwrap_pwr_state state);
static status_t power_create_thread(const char *name, void *(*pthread_function)(void*));
static void *power_event_monitor_thread(void *arg);


static void acquireWakeLock()
{
    Mutex::Autolock autoLock(gLock);

    if (wakeLockAcquired == false) {
        ALOGV("%s: Acquiring \"%s\" wake lock...", __FUNCTION__, WAKE_LOCK_ID);
        gNxWrap->acquireWL();
        wakeLockAcquired = true;
    }
}

static void releaseWakeLock()
{
    Mutex::Autolock autoLock(gLock);

    if (wakeLockAcquired == true && shutdownStarted == false) {
        ALOGV("%s: Releasing \"%s\" wake lock...", __FUNCTION__, WAKE_LOCK_ID);
        gNxWrap->releaseWL();
        wakeLockAcquired = false;
    }
}

static status_t sysfs_get(const char *path, unsigned int *out)
{
    FILE *f;
    unsigned int tmp;
    char buf[BUF_SIZE];
    status_t status = NO_ERROR;

    f = fopen(path, "r");
    if (!f) {
        status = errno;
        strerror_r(status, buf, sizeof(buf));
        ALOGE("%s: Cannot open \"%s\" [%s]!!!", __FUNCTION__, path, buf);
    }
    else {
        if (fgets(buf, BUF_SIZE, f) != buf) {
            status = INVALID_OPERATION;
            ALOGE("%s: Could not read from \"%s\"!!!", __FUNCTION__, path);
        }
        fclose(f);

        if (sscanf(buf, "0x%x", &tmp) != 1 && sscanf(buf, "%u", &tmp) != 1) {
            status = UNKNOWN_ERROR;
            ALOGE("%s: Invalid data in \"%s\"!!!", __FUNCTION__, path);
        }
        else {
            *out = tmp;
        }
    }
    return status;
}

static status_t sysfs_set(const char *path, unsigned int in)
{
    FILE *f;
    char buf[BUF_SIZE];
    status_t status = NO_ERROR;

    f = fopen(path, "w");
    if(!f)
    {
        status = errno;
        strerror_r(status, buf, sizeof(buf));
        ALOGE("%s: Cannot open \"%s\" [%s]!!!", __FUNCTION__, path, buf);
    }
    else {
        sprintf(buf, "%u", in);
        if((fputs(buf, f) < 0) || (fflush(f) < 0))
        {
            status = INVALID_OPERATION;
            ALOGE("%s: Could not write to \"%s\"!!!", __FUNCTION__, path);
        }
        fclose(f);
    }
    return status;
}

static nxwrap_pwr_state power_get_state_from_string(char *value)
{
    nxwrap_pwr_state powerOffState = ePowerState_Max;

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

static nxwrap_pwr_state power_get_property_off_state()
{
    nxwrap_pwr_state offState;
    char value[PROPERTY_VALUE_MAX] = "";

    property_get(BCM_PERSIST_POWER_SYS_OFFSTATE, value, DEFAULT_BCM_PERSIST_POWER_SYS_OFFSTATE);
    offState = power_get_state_from_string(value);

    return offState;
}

static nxwrap_pwr_state power_get_property_doze_state()
{
    nxwrap_pwr_state dozeState;
    char value[PROPERTY_VALUE_MAX] = "";

    property_get(BCM_PERSIST_POWER_SYS_DOZESTATE, value, DEFAULT_BCM_PERSIST_POWER_SYS_DOZESTATE);
    dozeState = power_get_state_from_string(value);

    return dozeState;
}

static bool power_get_property_wol_en()
{
    return property_get_bool(BCM_RO_POWER_WOL_EN, DEFAULT_BCM_RO_POWER_WOL_EN);
}

static bool power_get_property_wol_screen_on_en()
{
    return property_get_bool(BCM_RO_POWER_WOL_SCREEN_ON_EN, DEFAULT_BCM_RO_POWER_WOL_SCREEN_ON_EN);
}

static nxwrap_pwr_state power_get_off_state()
{
    nxwrap_pwr_state offState;

    offState = power_get_property_off_state();

    // If the default off state is S3 but the kernel has mapped this to S2 instead,
    // then we need to use S2 rather than S3 for placing Nexus in to standby.
    if (offState == ePowerState_S3) {
        unsigned map_mem_to_s2 = 0;
        if (sysfs_get(SYS_MAP_MEM_TO_S2, &map_mem_to_s2) == NO_ERROR) {
            if (map_mem_to_s2) {
                ALOGW("%s: Kernel cannot support S3, defaulting to S2 instead.", __FUNCTION__);
                offState = ePowerState_S2;
            }
        }
    }
    return offState;
}

static void dozeTimerCallback(union sigval val __unused)
{
    // Ensure we release a wake-lock to allow the system to begin suspending.  We then
    // use this notification to place Nexus in to standby (suspend in S2/S3 modes only).
    releaseWakeLock();
}

static int power_filter_dots(const struct dirent *d)
{
    return (strcmp(d->d_name, "..") && strcmp(d->d_name, "."));
}

static bool power_is_iface_present(const char *ifname)
{
    /* Check /sys/class/net for what devices are available... */
    int i;
    int num_entries;
    bool present = false;
    struct dirent **namelist = NULL;

    num_entries = scandir(SYS_CLASS_NET, &namelist, power_filter_dots, alphasort);
    if (num_entries < 0) {
        ALOGE("%s: Could not scan dir \"%s\" [%s]!!!", __FUNCTION__, SYS_CLASS_NET, strerror(errno));
    }
    else {
        for (i = 0; i < num_entries && !present; i++) {
            ALOGV("%s: Checking d_name=\"%s\" matches \"%s\"...", __FUNCTION__, namelist[i]->d_name, ifname);
            if (strcmp(namelist[i]->d_name, ifname) == 0) {
                present = true;
            }
            free(namelist[i]);
        }
        free(namelist);
    }
    return present;
}

static status_t power_set_enet_wol()
{
    status_t status = NO_ERROR;
    unsigned i;
    int ret;

    status = NAME_NOT_FOUND;
    for (i=0; i<sizeof(powerNetInterfaces)/sizeof(powerNetInterfaces[0]); i++) {
        if (power_is_iface_present(powerNetInterfaces[i])) {
           ret = gNxWrap->setWoL(powerNetInterfaces[i]);
           if (!ret) {
              ALOGD("%s: Successfully set WoL settings for %s", __FUNCTION__, powerNetInterfaces[i]);
              status = NO_ERROR;
           } else {
              ALOGW("%s: Could not set WoL settings for %s!", __FUNCTION__, powerNetInterfaces[i]);
              status = -EAGAIN;
           }
           break; // Stop after the first interface present
        }
    }

    return status;
}

static void * power_wol_monitor_thread(void * arg __unused)
{
    status_t status;

    while (true) {
        status = power_set_enet_wol();
        if (status != -EAGAIN) {
            break;
        }
        else {
            sleep(DEFAULT_WOL_MONITOR_TIMEOUT);
        }
    }
    return NULL;
}

static status_t power_set_wol_mode()
{
    unsigned int full_wol_en;

    // Handle Android WoLAN screen on enable/disable property
    // If enabled, a WoLAN event will wake up Android and turn on the screen
    // Otherwise, Android PM won't be notified and
    // device will return back to low power state
    if (sysfs_get(SYS_FULL_WOL_WAKEUP, &full_wol_en) == NO_ERROR) {
        bool full_wol_en_pr = power_get_property_wol_screen_on_en();

        if (full_wol_en_pr != full_wol_en) {
            if (sysfs_set(SYS_FULL_WOL_WAKEUP, full_wol_en_pr) != NO_ERROR) {
                ALOGE("%s: Could not set WOL entry at %s, leaving it %s!!!", __FUNCTION__,
                      SYS_FULL_WOL_WAKEUP, full_wol_en?"enabled":"disabled");
            }
            else {
                ALOGI("%s: successfully set WOL %s", __FUNCTION__, full_wol_en_pr?"enabled":"disabled");
            }
        }
        else {
            ALOGI("%s: WOL is %s", __FUNCTION__, full_wol_en_pr?"enabled":"disabled");
        }
    }
    else {
        ALOGE("%s: Could not read %s!!!", __FUNCTION__, SYS_FULL_WOL_WAKEUP);
    }

    if (power_get_property_wol_en()) {
        // Create a monitor thread to poll when the network interface is UP...
        return power_create_thread("WoL monitor", power_wol_monitor_thread);
    }
    else {
        return NO_ERROR;
    }
}

static status_t power_get_sw_lid_state(bool *down)
{
    status_t status;
    unsigned state;

    status = sysfs_get(SYS_SW_LID, &state);
    if (status == NO_ERROR) {
        *down = !!state;
        ALOGV("%s: Successfully got lid switch state %s", __FUNCTION__, state ? "down":"up");
    }
    else {
        ALOGE("%s: Could not get lid switch state!!!", __FUNCTION__);
    }
    return status;
}

static status_t power_set_sw_lid_state(bool down)
{
    status_t status;
    bool currentLidState;

    status = power_get_sw_lid_state(&currentLidState);
    if (status == NO_ERROR && currentLidState != down) {
        status = sysfs_set(SYS_SW_LID, down);
        if (status == NO_ERROR) {
            ALOGI("%s: Successfully set lid switch state to %s", __FUNCTION__, down ? "down":"up");
        }
        else {
            ALOGE("%s: Could not set lid switch state to %s !!!", __FUNCTION__, down ? "down":"up");
        }
    }
    return status;
}

static void power_init(struct power_module *module __unused)
{
    char buf[BUF_SIZE];
    const char *devname = getenv("NEXUS_WAKE_DEVICE_NODE");
    struct sigevent se;
    unsigned int wol_en;
    bool gpios_initialised = false;
    nxwrap_pwr_state power;
    nxwrap_wake_status wake;

    if (!devname) devname = "/dev/wake0";
    gPowerFd = open(devname, O_RDONLY);

    if (gPowerFd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("%s: Could not open %s PM driver [%s]!!!", __FUNCTION__, devname, buf);
        goto power_init_fail;
    }

    // Create an eventfd for monitoring events received from our PM driver...
    gPowerEventFd = eventfd(0, 0);
    if (gPowerEventFd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("%s: Could not create an event fd [%s]!!!", __FUNCTION__, buf);
        goto power_init_fail;
    }

    if (ioctl(gPowerFd, BRCM_IOCTL_REGISTER_EVENTS, &gPowerEventFd) != NO_ERROR) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("%s: Could not register PowerHAL with %s PM driver [%s]!!!", __FUNCTION__, devname, buf);
        goto power_init_fail;
    }

    gPowerEventMonitorFd = eventfd(0, 0);
    if (gPowerEventMonitorFd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("%s: Could not create an event monitor fd [%s]!!!", __FUNCTION__, buf);
        goto power_init_fail;
    }

    gNexusPower = NexusPower::instantiate();
    if (gNexusPower.get() == NULL) {
        ALOGE("%s: failed!!!", __FUNCTION__);
        goto power_init_fail;
    }

    gNxWrap = new NxWrap();
    if (gNxWrap == NULL) {
        ALOGE("%s: Could not create NxWrap!", __FUNCTION__);
        goto power_init_fail;
    }

    // Must initialise pmlib state to S0 initially...
    if (power_set_pmlibservice_state(ePowerState_S0) != NO_ERROR) {
        ALOGE("%s: Could not set pmlib state to S0!!!", __FUNCTION__);
        goto power_init_fail;
    }

    // If we have powered up only from an alarm timer, then it means we have woken up from S5 and
    // we need to remain in a standby state (e.g. S0.5 or S2).  Or, if the screen was off when
    // powered down, remain in standby. We do this by using the
    // Android framework's ability to power-up in to standby if a lid switch is set (i.e. down).
    if ((property_get_bool(BCM_PERSIST_POWER_SCREEN_ON, DEFAULT_BCM_PERSIST_POWER_SCREEN_ON) == false) ||
        (gNexusPower->getPowerStatus(&power, &wake) == NO_ERROR && wake.timeout &&
        !(wake.ir || wake.uhf || wake.keypad || wake.gpio || wake.cec || wake.transport))) {

        gPowerState = ePowerState_S05;
        power_set_sw_lid_state(SW_LID_STATE_DOWN);
    }
    else {
        gPowerState = ePowerState_S0;
        power_set_sw_lid_state(SW_LID_STATE_UP);
    }

#ifdef NEXUS_HAS_GPIO
    if (gNexusPower->initialiseGpios(gPowerState) != NO_ERROR) {
        ALOGE("%s: Could not initialise GPIO's!!!", __FUNCTION__);
        goto power_init_fail;
    }
    gpios_initialised = true;
#endif

    // Create the doze timer...
    se.sigev_value.sival_int = 0;
    se.sigev_notify = SIGEV_THREAD;
    se.sigev_notify_function = dozeTimerCallback;
    se.sigev_notify_attributes = NULL;
    if (timer_create(CLOCK_MONOTONIC, &se, &gDozeTimer) != 0) {
        ALOGE("%s: Could not create doze timer!!!", __FUNCTION__);
        goto power_init_fail;
    }

    // Setup WoLAN mode...
    power_set_wol_mode();

    // Start the power event monitor thread...
    if (power_create_thread("power event monitor", power_event_monitor_thread) != NO_ERROR) {
        goto power_init_fail;
    }
    return;

power_init_fail:
    if (gNexusPower.get()) {
        if (gpios_initialised) {
#ifdef NEXUS_HAS_GPIO
            gNexusPower->uninitialiseGpios();
#endif
        }
        gNexusPower = NULL;
    }
    if (gPowerEventMonitorFd >= 0) {
        close(gPowerEventMonitorFd);
        gPowerEventMonitorFd = -1;
    }
    if (gPowerEventFd >= 0) {
        close(gPowerEventFd);
        gPowerEventFd = -1;
    }
    if (gPowerFd >= 0) {
        close(gPowerFd);
        gPowerFd = -1;
    }
    if (gNxWrap != NULL) {
        delete gNxWrap;
        gNxWrap = NULL;
    }
}

static status_t power_set_pmlibservice_state(nxwrap_pwr_state state)
{
    status_t status = NO_ERROR;
    static bool init_S0_state = true;
    static struct pmlib_state_t pmlib_S0_state;
    struct pmlib_state_t pmlib_orig_state;
    struct pmlib_state_t pmlib_state;

    ALOGV("%s: Setting power state to %s...", __FUNCTION__, nxwrap_get_power_string(state));

    gNxWrap->getPwr(&pmlib_state);

    pmlib_orig_state = pmlib_state;

    if (state == ePowerState_S05 || state == ePowerState_S1) {
        /* eth_en == true means leave ethernet ON during standby */
        bool eth_en = property_get_bool(BCM_RO_POWER_ETH_EN, DEFAULT_BCM_RO_POWER_ETH_EN);

        /* moca_en == true means leave MOCA ON during standby */
        bool moca_en = property_get_bool(BCM_RO_POWER_MOCA_EN, DEFAULT_BCM_RO_POWER_MOCA_EN);

        /* sata_en == true means leave SATA ON during standby */
        bool sata_en = property_get_bool(BCM_RO_POWER_SATA_EN, DEFAULT_BCM_RO_POWER_SATA_EN);

        /* tp1_en == true means leave CPU thread 1 ON during standby */
        bool tp1_en = property_get_bool(BCM_RO_POWER_TP1_EN, DEFAULT_BCM_RO_POWER_TP1_EN);

        /* tp2_en == true means leave CPU thread 2 ON during standby */
        bool tp2_en = property_get_bool(BCM_RO_POWER_TP2_EN, DEFAULT_BCM_RO_POWER_TP2_EN);

        /* tp3_en == true means leave CPU thread 3 ON during standby */
        bool tp3_en = property_get_bool(BCM_RO_POWER_TP3_EN, DEFAULT_BCM_RO_POWER_TP3_EN);

        /* ddr_pm_en == true means enable DDR power management during true standby */
        bool ddr_pm_en = property_get_bool(BCM_RO_POWER_DDR_PM_EN, DEFAULT_BCM_RO_POWER_DDR_PM_EN) && (state != ePowerState_S05);

        /* cpufreq_scale_en == true means enable CPU frequency scaling during standby */
        bool cpufreq_scale_en = property_get_bool(BCM_RO_POWER_CPU_FREQ_SCALE_EN, DEFAULT_BCM_RO_POWER_CPU_FREQ_SCALE_EN);

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
        if (init_S0_state) {
            pmlib_S0_state = pmlib_state;
            init_S0_state = false;
        }

        pmlib_state.enet_en          = (true && pmlib_S0_state.enet_en);
        pmlib_state.moca_en          = (true && pmlib_S0_state.moca_en);
        pmlib_state.sata_en          = (true && pmlib_S0_state.sata_en);
        pmlib_state.tp1_en           = (true && pmlib_S0_state.tp1_en);
        pmlib_state.tp2_en           = (true && pmlib_S0_state.tp2_en);
        pmlib_state.tp3_en           = (true && pmlib_S0_state.tp3_en);
        pmlib_state.cpufreq_scale_en = false;
        pmlib_state.ddr_pm_en        = false;
    }

    if (memcmp(&pmlib_orig_state, &pmlib_state, sizeof(pmlib_state))) {
        gNxWrap->setPwr(&pmlib_state);
    }
    return status;
}

static status_t power_ack_suspend_shutdown()
{
    int ret;
    status_t status = NO_ERROR;
    char buf[BUF_SIZE];

    if (gPowerFd == -1) {
        ALOGE("%s: %s driver not opened!!!", __FUNCTION__, DROID_PM_DRV_NAME);
        status = NO_INIT;
    }
    else if (gPowerEventFd == -1) {
        ALOGE("%s: %s event fd not opened!!!", __FUNCTION__, DROID_PM_DRV_NAME);
        status = NO_INIT;
    }
    else {
        ALOGV("%s: Acknowledging suspend with droid_pm driver...", __FUNCTION__);
        ret = ioctl(gPowerFd, BRCM_IOCTL_SET_SUSPEND_ACK);
        if (ret) {
            status = errno;
            strerror_r(status, buf, sizeof(buf));
            ALOGE("%s: Error trying to acknowledge suspend [%s]!!!", __FUNCTION__, buf);
        }
    }
    return status;
}

static status_t power_set_state_s0()
{
    volatile status_t status;

    status = power_set_pmlibservice_state(ePowerState_S0);

    if (gNexusPower.get()) {
        status = gNexusPower->setPowerState(ePowerState_S0);
    }

    return status;
}

static status_t power_set_state_s05()
{
    status_t status = NO_ERROR;

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

    if (status == NO_ERROR && gNexusPower.get()) {
        status = gNexusPower->setPowerState(ePowerState_S1);
    }

    if (status == NO_ERROR) {
        power_set_pmlibservice_state(ePowerState_S1);
    }
    return status;
}

static status_t power_set_state_s2_s3(nxwrap_pwr_state toState)
{
    status_t status = NO_ERROR;

    if (gNexusPower.get()) {
        status = gNexusPower->setPowerState(toState);
    }
    return status;
}

static status_t power_set_state_s5()
{
    status_t status = NO_ERROR;

    if (gNexusPower.get()) {
        status = gNexusPower->setPowerState(ePowerState_S5);
    }
    return status;
}

static status_t power_set_state(nxwrap_pwr_state toState)
{
    volatile status_t status = NO_ERROR;

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

        case ePowerState_S2:
        case ePowerState_S3: {
            status = power_set_state_s2_s3(toState);
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
        ALOGI("%s: Successfully set power state %s", __FUNCTION__, nxwrap_get_power_string(toState));

        /* Mark the system has been suspended so we know whether we need to launch any splash screen when
         * woken up by the wakeup button
         */
        if (toState == ePowerState_S0) {
            property_set(BCM_DYN_POWER_BOOT_WAKEUP, "0");
        }
        else {
            property_set(BCM_DYN_POWER_BOOT_WAKEUP, "1");
        }
        /* If we want to boot up in the same screen on/off state, store the new setting */
        if (property_get_bool(BCM_PERSIST_POWER_KEEP_SCREEN_STATE,
                              DEFAULT_BCM_PERSIST_POWER_KEEP_SCREEN_STATE) == true) {
            property_set(BCM_PERSIST_POWER_SCREEN_ON, (toState == ePowerState_S0) ? "1" : "0");
        }
        gPowerState = toState;
    }
    else {
        ALOGE("%s: Could not set power state %s!!!", __FUNCTION__, nxwrap_get_power_string(toState));
    }
    return status;
}

static status_t power_set_doze_state(int timeout)
{
    status_t status;
    struct itimerspec ts;
    nxwrap_pwr_state dozeState = power_get_property_doze_state();
    nxwrap_pwr_state powerOffState = power_get_off_state();

    if (powerOffState != ePowerState_S2 &&
        powerOffState != ePowerState_S3) {
       // Kernel suspend is not enabled, so doze forever
       timeout = -1;
    }

    if (timeout < 0) {
        ALOGI("%s: Dozing in power state %s indefinitely...",
              __FUNCTION__, nxwrap_get_power_string(dozeState));
    }
    else if (timeout >= 0 && gDozeTimer) {
        timer_gettime(gDozeTimer, &ts);

        // Arm the doze timer...
        ALOGI("%s: Dozing in power state %s for at least %ds...",
              __FUNCTION__, nxwrap_get_power_string(dozeState), timeout);
        ts.it_value.tv_sec = timeout;
        ts.it_value.tv_nsec = 0;
        ts.it_interval.tv_sec = 0;
        ts.it_interval.tv_nsec = 0;
        timer_settime(gDozeTimer, 0, &ts, NULL);
    }

    if (timeout != 0)
        acquireWakeLock();
    else
        releaseWakeLock();

    status = power_set_state(dozeState);
    return status;
}

static status_t power_enter_suspend_state()
{
    nxwrap_pwr_state powerOffState = power_get_off_state();

    return power_set_state(powerOffState);
}

static status_t power_exit_suspend_state()
{
    status_t status = NO_ERROR;
    nxwrap_pwr_state powerOffState = power_get_off_state();

    int wakeTimeout = property_get_int32(BCM_PERSIST_POWER_SYS_WAKE_TIMEOUT,
                                             DEFAULT_WAKE_TIMEOUT);
    // Suspend complete, back to doze
    status = power_set_doze_state(wakeTimeout);

    return status;
}

static status_t power_set_interactivity_state(bool on)
{
    status_t status = NO_ERROR;

    if (gPowerFd >= 0) {
        int onOff = on;
        ALOGV("%s: Set interactive %s with droid_pm driver...", __FUNCTION__, on ? "on" : "off");
        if (ioctl(gPowerFd, BRCM_IOCTL_SET_INTERACTIVE, &onOff)) {
            status = errno;
            ALOGE("%s: Error trying to set interactive state in droid_pm driver [%s] !!!", __FUNCTION__, strerror(status));
        }
    }
    else {
        status = NO_INIT;
    }
    return status;
}

static status_t power_finish_set_no_interactive()
{
    status_t status = NO_ERROR;
    nxwrap_pwr_state dozeState = power_get_property_doze_state();
    int dozeTimeout = property_get_int32(BCM_PERSIST_POWER_SYS_DOZE_TIMEOUT,
                                         DEFAULT_DOZE_TIMEOUT);

    ALOGV("%s: entered", __FUNCTION__);

    // Ensure we acquire a wake-lock to prevent the system from suspending *BEFORE* we have
    // placed Nexus in to standby (we should not suspend in S0.5/S1/S5 modes anyway).
    acquireWakeLock();

    if (gNexusPower.get()) {
        gNexusPower->setVideoOutputsState(dozeState);
    }

    status = power_set_interactivity_state(false);

    power_set_doze_state(dozeTimeout);
    return status;
}

static status_t power_finish_set_interactive()
{
    status_t status = NO_ERROR;
    static bool calledDuringBoot = true;

    ALOGV("%s: entered", __FUNCTION__);

    if (!calledDuringBoot) {
        if (gDozeTimer) {
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

        if (status == NO_ERROR) {
            status = power_set_interactivity_state(true);
        }

        if (gNexusPower.get()) {
            gNexusPower->setVideoOutputsState(ePowerState_S0);
        }

        // Release the CPU wakelock as the system should be back up now...
        releaseWakeLock();
    }
    else {
        calledDuringBoot = false;
    }
    return status;
}

static void *power_event_monitor_thread(void *arg __unused)
{
    int ret;
    int epoll_fd;
    char buf[BUF_SIZE];

    ALOGV("%s: Starting power event monitor thread...", __FUNCTION__);

    epoll_fd = epoll_create(MAX_NUM_EVENT_FILE_DESCRIPTORS);
    if (epoll_fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("%s: Error creating polling set [%s]!!!", __FUNCTION__, buf);
    }
    else {
        struct epoll_event ev = epoll_event();
        struct epoll_event ev2 = epoll_event();

        ev.events = EPOLLIN;
        ev.data.fd = gPowerEventFd;
        ev2.events = EPOLLIN;
        ev2.data.fd = gPowerEventMonitorFd;

        ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, gPowerEventFd, &ev);
        if (ret < 0) {
            ALOGE("%s: Error adding power event fd to polling set [%s]!!!", __FUNCTION__, buf);
            goto fail_add_eventfd;
        }
        ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, gPowerEventMonitorFd, &ev2);
        if (ret < 0) {
            ALOGE("%s: Error adding power event monitor fd to polling set [%s]!!!", __FUNCTION__, buf);
            goto fail_add_eventfd2;
        }

        while (gPowerEventFd >= 0 && gPowerEventMonitorFd >= 0) {
            struct epoll_event out_evs[MAX_NUM_EVENT_MONITOR_THREAD_EVENTS];

            ret = epoll_wait(epoll_fd, out_evs, MAX_NUM_EVENT_MONITOR_THREAD_EVENTS, DEFAULT_EVENT_MONITOR_THREAD_TIMEOUT_MS);
            if (ret == 0) {
                ALOGV("%s: Timed out waiting for events - retrying...", __FUNCTION__);
            }
            else if (ret < 0) {
                strerror_r(errno, buf, sizeof(buf));
                ALOGE("%s: Error polling for events [%s]!!!", __FUNCTION__, buf);
                break;
            }
            else {
                int numEvents = ret;
                eventfd_t event;

                for (int i = 0, ret = 0; i < numEvents; i++) {
                    int fd = out_evs[i].data.fd;
                    uint32_t epollEvents = out_evs[i].events;

                    if (fd == gPowerEventMonitorFd) {
                        if (epollEvents & (EPOLLIN | EPOLLERR)) {
                            ret = eventfd_read(gPowerEventMonitorFd, &event);
                            if (ret < 0) {
                                strerror_r(errno, buf, sizeof(buf));
                                ALOGE("%s: Could not read monitor event [%s]!!!", __FUNCTION__, buf);
                            }
                            else {
                                // Event received from Power HAL - now find out which one...
                                ALOGV("%s: Received power monitor event %" PRIu64 "", __FUNCTION__, event);
                                if (event == POWER_MONITOR_EVENT_INTERACTIVE_ON) {
                                    if (power_finish_set_interactive() != NO_ERROR) {
                                        ALOGE("%s: Could not set interactive state!!!", __FUNCTION__);
                                    }
                                }
                                else if (event == POWER_MONITOR_EVENT_INTERACTIVE_OFF) {
                                    if (power_finish_set_no_interactive() != NO_ERROR) {
                                        ALOGE("%s: Could not set non-interactive state!!!", __FUNCTION__);
                                    }
                                }
                                else if (event == POWER_MONITOR_EVENT_OVFLOW) {
                                    ALOGE("%s: Overflow detected reading monitor event!!!", __FUNCTION__);
                                }
                            }
                        }
                    }
                    else if (fd == gPowerEventFd) {
                        if (epollEvents & EPOLLIN) {
                            ret = eventfd_read(gPowerEventFd, &event);
                            if (ret < 0) {
                                strerror_r(errno, buf, sizeof(buf));
                                ALOGE("%s: Could not read event [%s]!!!", __FUNCTION__, buf);
                            }
                            else {
                                // Event received from droid_pm - now find out which one...
                                ALOGV("%s: Received power event %" PRIu64 "", __FUNCTION__, event);
                                if (event == DROID_PM_EVENT_SUSPENDING) {
                                    ret = power_enter_suspend_state();
                                    if (ret == NO_ERROR) {
                                        // Wait for suspend to complete
                                        ret = power_ack_suspend_shutdown();
                                        if (ret != NO_ERROR && ret != NO_INIT) {
                                            ALOGE("%s: Error trying to ack suspend!!!", __FUNCTION__);
                                        }
                                        else {
                                            ALOGV("%s: Successfully ack suspend.", __FUNCTION__);
                                        }
                                    }
                                }
                                else if (event == DROID_PM_EVENT_SHUTDOWN) {
                                    shutdownStarted = true;
                                    ret = power_set_state_s5();
                                    if (ret != NO_ERROR)
                                        ALOGE("%s: Error trying to enter S5!!!", __FUNCTION__);
                                    else
                                        ALOGD("%s: Entered S5", __FUNCTION__);

                                    power_ack_suspend_shutdown();
                                }
                                if (event == DROID_PM_EVENT_RESUMED_WAKEUP) {
                                    ALOGV("%s: Received a valid wakeup event", __FUNCTION__);
                                    ret = power_exit_suspend_state();
                                    if (ret == NO_ERROR) {
                                        ALOGV("%s: Successfully exited suspend state.", __FUNCTION__);
                                    }
                                }
                                else if (event == DROID_PM_EVENT_RESUMED_PARTIAL) {
                                    ALOGV("%s: Received a valid partial wakeup event", __FUNCTION__);
                                    ret = power_exit_suspend_state();
                                    if (ret == NO_ERROR) {
                                        ALOGV("%s: Successfully exited suspend state.", __FUNCTION__);
                                    }
                                }
                                else if (event == DROID_PM_EVENT_RESUMED) {
                                    if (gNexusPower.get()) {
                                        nxwrap_pwr_state state;
                                        nxwrap_wake_status wake;
                                        if ((gNexusPower->getPowerStatus(&state, &wake) == NO_ERROR) && wake.timeout) {
                                            ALOGV("%s: Woke up from timer event", __FUNCTION__);
                                        }
                                        ret = power_exit_suspend_state();
                                        if (ret == NO_ERROR) {
                                            ALOGV("%s: Successfully exited suspend state.", __FUNCTION__);
                                        }
                                    }
                                }
                                else if (event == DROID_PM_EVENT_BT_WAKE_ON) {
                                    ALOGV("%s: Received a BT_WAKE Asserted event", __FUNCTION__);
                                    if (gNexusPower.get()) {
#ifdef NEXUS_HAS_GPIO
                                        gNexusPower->setGpiosInterruptWakeManager(gPowerState, NexusPower::NexusGpio::GpioInterruptWakeManager_eBt, true);
#endif
                                    }
                                }
                                else if (event == DROID_PM_EVENT_BT_WAKE_OFF) {
                                    ALOGV("%s: Received a BT_WAKE Deasserted event", __FUNCTION__);
                                }
                            }
                        }
                        else {
                            ALOGW("%s: Not ready to suspend/shutdown - retrying...", __FUNCTION__);
                        }
                    }
                    else {
                        ALOGE("%s: Unknown event fd received!!!", __FUNCTION__);
                    }
                }
            }
        }
    }
exit_thread:
    ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, gPowerEventMonitorFd, NULL);
    if (ret < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("%s: Error removing epoll events for event monitor fd [%s]!!!", __FUNCTION__, buf);
    }
fail_add_eventfd2:
    ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, gPowerEventFd, NULL);
    if (ret < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("%s: Error removing epoll events for event fd [%s]!!!", __FUNCTION__, buf);
    }
fail_add_eventfd:
    ALOGV("%s: Exiting power event monitor thread...", __FUNCTION__);
    if (epoll_fd >= 0) {
        close(epoll_fd);
    }
    return NULL;
}

static status_t power_create_thread(const char *name, void *(*pthread_function)(void*))
{
    status_t status;
    pthread_t thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&thread, &attr, pthread_function, NULL) == 0) {
        ALOGV("%s: Successfully created %s thread.", __FUNCTION__, name);
        status = NO_ERROR;
    }
    else {
        ALOGE("%s: Could not create %s thread!!!", __FUNCTION__, name);
        status = UNKNOWN_ERROR;
    }
    pthread_attr_destroy(&attr);
    return status;
}

static void power_dispatch_event(int fd, eventfd_t event)
{
    if (fd >= 0) {
        if (eventfd_write(fd, event) < 0) {
            ALOGE("%s: Cannot signal event %" PRIu64 " to monitor thread [%s]!!!", __FUNCTION__, event, strerror(errno));
        }
    }
    else {
        ALOGE("%s: Invalid event fd!!!", __FUNCTION__);
    }
}

static void power_set_interactive(struct power_module *module __unused, int on)
{
    ALOGI("%s: %s", __FUNCTION__, on ? "ON" : "OFF");

    if (shutdownStarted) {
        ALOGI("%s: Ignoring interaction, shutdown in progress.", __FUNCTION__);
    }
    else if (gPowerEventMonitorFd >= 0) {
        power_dispatch_event(gPowerEventMonitorFd, on ? POWER_MONITOR_EVENT_INTERACTIVE_ON : POWER_MONITOR_EVENT_INTERACTIVE_OFF);
    }
    else {
        ALOGE("%s: *** Power HAL not initialised!!! ***", __FUNCTION__);
    }
}

static void power_hint(struct power_module *module __unused, power_hint_t hint, void *data __unused) {
    switch (hint) {
        case POWER_HINT_VSYNC:
            ALOGV("%s: POWER_HINT_VSYNC received", __FUNCTION__);
            break;
        case POWER_HINT_INTERACTION:
            static bool firstInteraction = true;
            ALOGV("%s: POWER_HINT_INTERACTION received", __FUNCTION__);
            if (firstInteraction) {
                firstInteraction = false;
            }
            else if (!shutdownStarted) {
                // Make sure that the Android framework does not try to remain in a suspended state
                // due to the lid switch being activate (i.e. down).
                power_set_sw_lid_state(SW_LID_STATE_UP);
            }
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

static int power_get_stats(struct power_module *module __unused,
                            power_state_platform_sleep_state_t *list)
{
    ALOGD("%s", __FUNCTION__);
    if (list == NULL)
        return EINVAL;

    list[0] = {
                .name = "S0.5",
                .residency_in_msec_since_boot = 0,
                .total_transitions = 0,
                .supported_only_in_suspend = 0,
                .number_of_voters = 0,
    };

    list[1] = {
                .name = "S2",
                .residency_in_msec_since_boot = 0,
                .total_transitions = 0,
                .supported_only_in_suspend = 1,
                .number_of_voters = 0,
    };

    return 0;
}

static ssize_t power_get_num_states(struct power_module *module __unused)
{
    return 2; // S0.5 and S2 are reported
}

static int power_get_voter_list(struct power_module *module __unused, size_t *voter)
{
    if (voter == NULL)
        return EINVAL;

    voter[0] = voter[1] = 0;
    return 0;
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL
};

struct power_module HAL_MODULE_INFO_SYM = {
   .common = {
      .tag                = HARDWARE_MODULE_TAG,
      .module_api_version = POWER_MODULE_API_VERSION_0_5,
      .hal_api_version    = HARDWARE_HAL_API_VERSION,
      .id                 = POWER_HARDWARE_MODULE_ID,
      .name               = "Brcmstb Power HAL",
      .author             = "Broadcom",
      .methods            = &power_module_methods,
      .dso                = 0,
      .reserved           = {0}
    },
    .init                 = power_init,
    .setInteractive       = power_set_interactive,
    .powerHint            = power_hint,
    .setFeature           = power_set_feature,
    .get_platform_low_power_stats
                          = power_get_stats,
    .get_number_of_platform_modes
                          = power_get_num_states,
    .get_voter_list       = power_get_voter_list,
};
