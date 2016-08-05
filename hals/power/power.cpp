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
#include <poll.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/ethtool.h>
#include <linux/if.h>
#include <linux/socket.h>

#include <cutils/properties.h>
#include <utils/Timers.h>
#include <hardware/hardware.h>
#include <hardware/power.h>
#include <hardware_legacy/power.h>
#include "PmLibService.h"
#include <inttypes.h>

using namespace android;

// Keep the CPU alive wakelock
static const char *WAKE_LOCK_ID = "PowerHAL";
static bool wakeLockAcquired = false;

static bool shutdownStarted = false;

// Maximum length of character buffer
static const int BUF_SIZE = 64;

static const char *powerNetInterfaces[] = {
    "gphy",
    "eth0"
};

// Property definitions (max length 32)...
static const char * PROPERTY_SYS_POWER_DOZE_TIMEOUT       = "persist.sys.power.doze.timeout";
static const char * PROPERTY_SYS_POWER_WAKE_TIMEOUT       = "persist.sys.power.wake.timeout";
static const char * PROPERTY_SYS_POWER_DOZESTATE          = "persist.sys.power.dozestate";
static const char * PROPERTY_SYS_POWER_OFFSTATE           = "persist.sys.power.offstate";
static const char * PROPERTY_PM_ETH_EN                    = "ro.pm.eth_en";
static const char * PROPERTY_PM_MOCA_EN                   = "ro.pm.moca_en";
static const char * PROPERTY_PM_SATA_EN                   = "ro.pm.sata_en";
static const char * PROPERTY_PM_TP1_EN                    = "ro.pm.tp1_en";
static const char * PROPERTY_PM_TP2_EN                    = "ro.pm.tp2_en";
static const char * PROPERTY_PM_TP3_EN                    = "ro.pm.tp3_en";
static const char * PROPERTY_PM_DDR_PM_EN                 = "ro.pm.ddr_pm_en";
static const char * PROPERTY_PM_CPU_FREQ_SCALE_EN         = "ro.pm.cpufreq_scale_en";
static const char * PROPERTY_PM_WOL_EN                    = "ro.pm.wol.en";
static const char * PROPERTY_PM_WOL_OPTS                  = "ro.pm.wol.opts";

// Property defaults
static const char * DEFAULT_PROPERTY_SYS_POWER_DOZESTATE  = "S0.5";
static const char * DEFAULT_PROPERTY_SYS_POWER_OFFSTATE   = "S2";
static const int8_t DEFAULT_PROPERTY_PM_ETH_EN            = 1;     // Enable Ethernet during standby
static const int8_t DEFAULT_PROPERTY_PM_MOCA_EN           = 0;     // Disable MOCA during standby
static const int8_t DEFAULT_PROPERTY_PM_SATA_EN           = 0;     // Disable SATA during standby
static const int8_t DEFAULT_PROPERTY_PM_TP1_EN            = 0;     // Disable CPU1 during standby
static const int8_t DEFAULT_PROPERTY_PM_TP2_EN            = 0;     // Disable CPU2 during standby
static const int8_t DEFAULT_PROPERTY_PM_TP3_EN            = 0;     // Disable CPU3 during standby
static const int8_t DEFAULT_PROPERTY_PM_DDR_PM_EN         = 1;     // Enabled DDR power management during standby
static const int8_t DEFAULT_PROPERTY_PM_CPU_FREQ_SCALE_EN = 1;     // Enable CPU frequency scaling during standby
static const int8_t DEFAULT_PROPERTY_PM_WOL_EN            = 0;     // Disable Android wake up by the WoLAN event
static const char * DEFAULT_PROPERTY_PM_WOL_OPTS          = "s";   // Enable WoL for MAGIC SECURE packet

// SecureOn(TM) password file path.
static const char * SOPASS_KEY_FILE_PATH                  = "/data/misc/nexus/sopass.key";

// Sysfs paths
static const char * SYS_MAP_MEM_TO_S2                     = "/sys/devices/platform/droid_pm/map_mem_to_s2";
static const char * SYS_FULL_WOL_WAKEUP                   = "/sys/devices/platform/droid_pm/full_wol_wakeup";
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

// The doze timer will be used only if the platform has be configured to
// enter an intermediate "doze" mode, prior to entering the off state.
static timer_t gDozeTimer = NULL;

// Global instance of the NexusPower class.
static sp<NexusPower> gNexusPower;

// Power device driver file descriptor.
static int gPowerFd = -1;

// Event file descriptor for reading back events from the power driver
static int gPowerEventFd = -1;

// Flags to signal when to exit and exited the event monitor thread.
static volatile bool gPowerEventMonitorThreadExit = false;
static volatile bool gPowerEventMonitorThreadExited = true;

// Event monitor thread synchronisation primitives.
static pthread_mutex_t gPowerEventMonitorThreadMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  gPowerEventMonitorThreadCond  = PTHREAD_COND_INITIALIZER;

// Global Power HAL Mutex
Mutex gLock("PowerHAL Lock");

typedef status_t (*powerFinishFunction_t)(void);

// Prototype declarations...
static status_t power_set_state(b_powerState toState);
static status_t power_set_poweroff_state();
static status_t power_finish_set_interactive();
static void power_finish_set_no_interactive();
static void releaseWakeLock();
static void *power_s5_shutdown(void *arg);
static status_t power_create_thread(void *(pthread_function)(void*));
static status_t power_set_doze_state(int timeout);
static void *power_event_monitor_thread(void *arg);


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
    b_powerState offState;
    char value[PROPERTY_VALUE_MAX] = "";

    property_get(PROPERTY_SYS_POWER_OFFSTATE, value, DEFAULT_PROPERTY_SYS_POWER_OFFSTATE);
    offState = power_get_state_from_string(value);

    return offState;
}

static b_powerState power_get_property_doze_state()
{
    b_powerState dozeState;
    char value[PROPERTY_VALUE_MAX] = "";

    property_get(PROPERTY_SYS_POWER_DOZESTATE, value, DEFAULT_PROPERTY_SYS_POWER_DOZESTATE);
    dozeState = power_get_state_from_string(value);

    return dozeState;
}

static bool power_get_property_wol_en()
{
    return property_get_bool(PROPERTY_PM_WOL_EN, DEFAULT_PROPERTY_PM_WOL_EN);
}

static int power_get_property_wol_opts(char *opts)
{
    int len;
    char value[PROPERTY_VALUE_MAX];

    len = property_get(PROPERTY_PM_WOL_OPTS, value, DEFAULT_PROPERTY_PM_WOL_OPTS);
    if (len) {
        strncpy(opts, value, PROPERTY_VALUE_MAX-1);
    }
    return len;
}

static b_powerState power_get_off_state()
{
    b_powerState offState;

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

    if (wakeLockAcquired == true && shutdownStarted == false) {
        ALOGV("%s: Releasing \"%s\" wake lock...", __FUNCTION__, WAKE_LOCK_ID);
        release_wake_lock(WAKE_LOCK_ID);
        wakeLockAcquired = false;
    }
}

static status_t power_parse_wolopts(char *optstr, uint32_t *data)
{
    status_t status = NO_ERROR;

    *data = 0;

    while (*optstr) {
        switch (*optstr) {
            case 'p':
                *data |= WAKE_PHY;
                break;
            case 'u':
                *data |= WAKE_UCAST;
                break;
            case 'm':
                *data |= WAKE_MCAST;
                break;
            case 'b':
                *data |= WAKE_BCAST;
                break;
            case 'a':
                *data |= WAKE_ARP;
                break;
            case 'g':
                *data |= WAKE_MAGIC;
                break;
            case 's':
                *data |= WAKE_MAGICSECURE;
                break;
            case 'd':
                *data = 0;
                break;
            default:
                status = BAD_VALUE;
                break;
        }
        optstr++;
    }
    return status;
}

static status_t power_parse_sopass(String8& src, uint8_t *dest)
{
    status_t status = NO_ERROR;
    int count;
    unsigned int buf[ETH_ALEN];

    count = sscanf(src.string(), "%2x:%2x:%2x:%2x:%2x:%2x",
        &buf[0], &buf[1], &buf[2], &buf[3], &buf[4], &buf[5]);

    if (count != ETH_ALEN) {
        status = BAD_VALUE;
    }
    else {
        for (int i = 0; i < count; i++) {
            dest[i] = buf[i];
        }
    }
    return status;
}

String8 power_get_sopass_file_path()
{
    String8 path;

    path.setTo(SOPASS_KEY_FILE_PATH);

    if (!access(path.string(), R_OK)) {
        ALOGV("%s: Found \"%s\".", __FUNCTION__, path.string());
        return path;
    }
    else {
        ALOGV("%s: Could not find \"%s\"!", __FUNCTION__, path.string());
        return String8();
    }
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
    struct ifreq ifr;
    unsigned i;
    int fd = -1;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        ALOGW("%s: Could not open socket for WoL control!", __FUNCTION__);
        status = PERMISSION_DENIED;
    }
    else {
        status = NAME_NOT_FOUND;

        // Search for a valid Ethernet interface...
        for (i=0; i<sizeof(powerNetInterfaces)/sizeof(powerNetInterfaces[0]); i++) {
            if (power_is_iface_present(powerNetInterfaces[i])) {
                memset(&ifr, 0, sizeof(ifr));
                strcpy(ifr.ifr_name, powerNetInterfaces[i]);

                status = ioctl(fd, SIOCGIFFLAGS, &ifr);
                if (status < 0) {
                    ALOGW("%s: Could not get socket flags for %s!", __FUNCTION__, ifr.ifr_name);
                }
                else if (!(ifr.ifr_flags & IFF_UP)) {
                    ALOGW("%s: Interface %s is DOWN!", __FUNCTION__, ifr.ifr_name);
                    status = -EAGAIN;
                }
                else {
                    status = NO_ERROR;
                    break;
                }
            }
        }
    }

    if (status == NO_ERROR) {
        struct ethtool_wolinfo wolinfo;
        char wol_opts[PROPERTY_VALUE_MAX] = "";

        memset(&wolinfo, 0, sizeof(wolinfo));

        if (power_get_property_wol_opts(wol_opts) > 0) {
            uint32_t data;

            status = power_parse_wolopts(wol_opts, &data);
            if (status == NO_ERROR) {
                wolinfo.wolopts = data;
                ALOGV("%s: WoL options=0x%02x", __FUNCTION__, data);

                // SecureOn(TM) password is stored in a password file
                if (data & WAKE_MAGICSECURE) {
                    uint8_t sopass[SOPASS_MAX];
                    String8 path;
                    String8 value;
                    PropertyMap* config;

                    path = power_get_sopass_file_path();
                    if (path.isEmpty()) {
                        ALOGE("%s: Could not find SecureOn(TM) password file \"%s\"!", __FUNCTION__, path.string());
                        status = NAME_NOT_FOUND;
                    }
                    else {
                        status = PropertyMap::load(path, &config);
                        if (status == NO_ERROR) {
                            size_t numEntries = config->getProperties().size();
                            if (numEntries != 1) {
                                ALOGE("%s: Invalid number of entries %zu in SecureOn(TM) password file \"%s\"!", __FUNCTION__, numEntries, path.string());
                                status = BAD_VALUE;
                            }
                            else if (config->tryGetProperty(String8("SOPASS"), value)) {
                                status = power_parse_sopass(value, sopass);
                                if (status == NO_ERROR) {
                                    for (i = 0; i < SOPASS_MAX; i++) {
                                        wolinfo.sopass[i] = sopass[i];
                                    }
                                    ALOGV("%s: WoL sopass=\"%s\"", __FUNCTION__, value.string());
                                }
                                else {
                                    ALOGE("%s: Invalid \"sopass\" key in SecureOn(TM) password file \"%s\"!", __FUNCTION__, path.string());
                                }
                            }
                            else {
                                ALOGE("%s: Could not find \"sopass\" key in SecureOn(TM) password file \"%s\"!", __FUNCTION__, path.string());
                                status = BAD_VALUE;
                            }
                        }
                        else {
                            ALOGE("%s: Could not load SecureOn(TM) password file \"%s\"!", __FUNCTION__, path.string());
                        }
                    }
                }
            }
            else {
                ALOGE("%s: Invalid WoL options \"%s\"!", __FUNCTION__, wol_opts);
            }
        }
        else {
            ALOGE("%s: No WoL arguments specified!", __FUNCTION__);
            status = BAD_VALUE;
        }

        if (status == NO_ERROR) {
            wolinfo.cmd = ETHTOOL_SWOL;
            ifr.ifr_data = (void *)&wolinfo;
            status = ioctl(fd, SIOCETHTOOL, &ifr);
        }

        if (status == NO_ERROR) {
            ALOGD("%s: Successfully set WoL settings for %s", __FUNCTION__, ifr.ifr_name);
        }
        else {
            ALOGW("%s: Could not set WoL settings for %s!", __FUNCTION__, ifr.ifr_name);
        }
    }

    if (fd >= 0) {
        close(fd);
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
    status_t status;
    unsigned int full_wol_en;
    pthread_t thread;
    pthread_attr_t attr;

    // Handle Android WoLAN enable/disable property
    // If enabled, a WoLAN event will wake up Android
    // Otherwise, Android PM won't be notified and
    // device will return back to low power state
    if (sysfs_get(SYS_FULL_WOL_WAKEUP, &full_wol_en) == NO_ERROR) {
        bool full_wol_en_pr = power_get_property_wol_en();

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

    // Create a monitor thread to poll when the network interface is UP...
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&thread, &attr, power_wol_monitor_thread, NULL) == 0) {
        ALOGV("%s: Successfully created a WoL monitor thread!", __FUNCTION__);
        status = NO_ERROR;
    }
    else {
        ALOGE("%s: Could not create a WoL monitor thread!!!", __FUNCTION__);
        status = UNKNOWN_ERROR;
    }
    pthread_attr_destroy(&attr);

    return status;
}

static void power_init(struct power_module *module __unused)
{
    char buf[BUF_SIZE];
    const char *devname = getenv("NEXUS_WAKE_DEVICE_NODE");
    unsigned int wol_en;

    if (!devname) devname = "/dev/wake0";
    gPowerFd = open(devname, O_RDONLY);

    if (gPowerFd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("%s: Could not open %s PM driver [%s]!!!", __FUNCTION__, devname, buf);
    }
    else {
        // Create an eventfd for monitoring events received from our PM driver...
        gPowerEventFd = eventfd(0, 0);
        if (gPowerEventFd < 0) {
            strerror_r(errno, buf, sizeof(buf));
            ALOGE("%s: Could not create an event fd [%s]!!!", __FUNCTION__, buf);
            close(gPowerFd);
        }
        else if (ioctl(gPowerFd, BRCM_IOCTL_REGISTER_EVENTS, &gPowerEventFd) != NO_ERROR) {
            strerror_r(errno, buf, sizeof(buf));
            ALOGE("%s: Could not register PowerHAL with %s PM driver [%s]!!!", __FUNCTION__, devname, buf);
            close(gPowerEventFd);
            gPowerEventFd = -1;
            close(gPowerFd);
            gPowerFd = -1;
        }
        else {
            b_powerState state = ePowerState_S0;

            gNexusPower = NexusPower::instantiate();

            if (gNexusPower.get() == NULL) {
                ALOGE("%s: failed!!!", __FUNCTION__);
            }
            else if (gNexusPower->initialiseGpios(state) != NO_ERROR) {
                ALOGE("%s: Could not initialise GPIO's!!!", __FUNCTION__);
            }
            else {
                struct sigevent se;

                // Create the doze timer...
                se.sigev_value.sival_int = 0;
                se.sigev_notify = SIGEV_THREAD;
                se.sigev_notify_function = dozeTimerCallback;
                se.sigev_notify_attributes = NULL;
                if (timer_create(CLOCK_MONOTONIC, &se, &gDozeTimer) != 0) {
                    ALOGE("%s: Could not create doze timer!!!", __FUNCTION__);
                }
                // Setup WoLAN mode...
                power_set_wol_mode();
            }
        }
    }
}

static status_t power_set_pmlibservice_state(b_powerState state)
{
    status_t status;
    static bool init_S0_state = true;
    static IPmLibService::pmlib_state_t pmlib_S0_state;
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
        status = service->setState(&pmlib_state);
    }
    return status;
}

static status_t power_prepare_suspend()
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
        else {
            eventfd_t event;

            // Ready to suspend. Block and wait for a resume indication to exit suspend
            ret = eventfd_read(gPowerEventFd, &event);
            if (ret < 0) {
                status = errno;
                strerror_r(status, buf, sizeof(buf));
                ALOGE("%s: Failed to read event(s) [%s]!!!", __FUNCTION__, buf);
            }
            else {
                ALOGV("%s: Event %" PRIu64 " received", __FUNCTION__, event);

                if (event == DROID_PM_EVENT_RESUMED ||
                    event == DROID_PM_EVENT_RESUMED_PARTIAL ||
                    event == DROID_PM_EVENT_RESUMED_WAKEUP) {
                    if (event == DROID_PM_EVENT_RESUMED_WAKEUP) {
                        ALOGV("%s: Received a valid wakeup event", __FUNCTION__);
                    }
                    else if (event == DROID_PM_EVENT_RESUMED_PARTIAL) {
                        ALOGV("%s: Received a valid partial wakeup event", __FUNCTION__);
                    }
                    else {
                        if (gNexusPower.get()) {
                            b_powerStatus powerStatus;

                            if ((gNexusPower->getPowerStatus(&powerStatus) == NO_ERROR) && powerStatus.wakeupStatus.timeout) {
                                ALOGV("%s: Woke up from timer event", __FUNCTION__);
                            }
                        }
                    }
                }
                else {
                    ALOGW("%s: Invalid state to receive event %" PRIu64 "!", __FUNCTION__, event);
                    status = BAD_VALUE;
                }
            }
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

static status_t power_set_state_s2_s3(b_powerState toState)
{
    status_t status = NO_ERROR;

    if (gNexusPower.get()) {
        status = gNexusPower->setPowerState(toState);
    }
    return status;
}

static status_t power_set_state_s5()
{
    // When offstate is S5, we wait until the kernel starts to suspend before
    // invoking the shutdown. Prevent us from suspending by taking a wakelock.
    acquireWakeLock();

    // Don't kill the current event monitor thread.
    // Spawn a thread to call the blocking shutdown interface
    // We can't block or we will miss the shutdown notification
    return power_create_thread(power_s5_shutdown);
}

static void *power_s5_shutdown(void *arg __unused)
{
    shutdownStarted = true;
    system("/system/bin/svc power shutdown");
    return NULL;
}

static status_t power_set_state(b_powerState toState)
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
        ALOGI("%s: Successfully set power state %s", __FUNCTION__, NexusIPCClientBase::getPowerString(toState));
    }
    else {
        ALOGE("%s: Could not set power state %s!!!", __FUNCTION__, NexusIPCClientBase::getPowerString(toState));
    }
    return status;
}

static status_t power_set_poweroff_state()
{
    b_powerState powerOffState = power_get_off_state();

    return power_set_state(powerOffState);
}

static void *power_doze_monitor_thread(void *arg __unused)
{
    status_t status = NO_ERROR;
    int dozeTimeout = property_get_int32(PROPERTY_SYS_POWER_DOZE_TIMEOUT,
                                         DEFAULT_DOZE_TIMEOUT);

    ALOGV("%s: entered", __FUNCTION__);

    power_set_doze_state(dozeTimeout);

    // Monitor for suspend and shutdown events
    return power_event_monitor_thread(NULL);
}

static status_t power_set_doze_state(int timeout)
{
    status_t status;
    struct itimerspec ts;
    b_powerState dozeState = power_get_property_doze_state();
    b_powerState powerOffState = power_get_off_state();

    if (powerOffState != ePowerState_S2 &&
        powerOffState != ePowerState_S3 &&
        powerOffState != ePowerState_S5) {
       // Kernel suspend is not enabled, so doze forever
       timeout = -1;
    }

    if (timeout < 0) {
        ALOGI("%s: Dozing in power state %s indefinitely...",
              __FUNCTION__, NexusIPCClientBase::getPowerString(dozeState));
    }
    else if (timeout >= 0 && gDozeTimer) {
        timer_gettime(gDozeTimer, &ts);

        // Arm the doze timer...
        ALOGI("%s: Dozing in power state %s for at least %ds...",
              __FUNCTION__, NexusIPCClientBase::getPowerString(dozeState), timeout);
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

static void *power_event_monitor_thread(void *arg __unused)
{
    int ret;
    struct pollfd pollFd;
    char buf[BUF_SIZE];
    int wakeTimeout = property_get_int32(PROPERTY_SYS_POWER_WAKE_TIMEOUT,
                                         DEFAULT_WAKE_TIMEOUT);

    pthread_mutex_lock(&gPowerEventMonitorThreadMutex);
    gPowerEventMonitorThreadExit = false;
    gPowerEventMonitorThreadExited = false;
    pthread_mutex_unlock(&gPowerEventMonitorThreadMutex);

    ALOGV("%s: Starting power event monitor thread...", __FUNCTION__);

    pollFd.fd = gPowerEventFd;
    pollFd.events = POLLIN | POLLRDNORM;

    while (gPowerEventFd >= 0) {
        pthread_mutex_lock(&gPowerEventMonitorThreadMutex);
        if (gPowerEventMonitorThreadExit) {
            pthread_mutex_unlock(&gPowerEventMonitorThreadMutex);
            break;
        }
        pthread_mutex_unlock(&gPowerEventMonitorThreadMutex);
        ret = poll(&pollFd, 1, DEFAULT_EVENT_MONITOR_THREAD_TIMEOUT_MS);
        if (ret == 0) {
            ALOGV("%s: Timed out waiting for event - retrying...", __FUNCTION__);
        }
        else if (ret < 0) {
            strerror_r(errno, buf, sizeof(buf));
            ALOGE("%s: Error polling event fd [%s]!!!", __FUNCTION__, buf);
            break;
        }
        else if (pollFd.revents & (POLLIN | POLLRDNORM)) {
            eventfd_t event;

            ret = eventfd_read(pollFd.fd, &event);
            if (ret < 0) {
                strerror_r(errno, buf, sizeof(buf));
                ALOGE("%s: Could not read event [%s]!!!", __FUNCTION__, buf);
            }
            else {
                // Event received from droid_pm - now find out which one...
                ALOGV("%s: Received power event %" PRIu64 "", __FUNCTION__, event);
                if (event == DROID_PM_EVENT_SUSPENDING) {
                    ret = power_set_poweroff_state();
                    if (ret == NO_ERROR) {
                        ALOGD("%s: Successfully finished setting power off state.", __FUNCTION__);
                        // Wait for suspend to complete
                        ret = power_prepare_suspend();
                        if (ret != NO_ERROR && ret != NO_INIT) {
                            ALOGE("%s: Error trying to enter suspend!!!", __FUNCTION__);
                        }
                        // Suspend complete, back to doze
                        power_set_doze_state(wakeTimeout);
                    }
                }

                else if (event == DROID_PM_EVENT_SHUTDOWN) {
                    if (gNexusPower.get()) {
                        ret = gNexusPower->setPowerState(ePowerState_S5);
                    }
                    if (ret != NO_ERROR)
                        ALOGE("%s: Error trying to enter S5!!!", __FUNCTION__);
                    else
                        ALOGD("%s: Entered S5", __FUNCTION__);

                    // Acknowledge shutdown message and wait, shouldn't return
                    power_prepare_suspend();
                }
                else if (event == DROID_PM_EVENT_RESUMED_WAKEUP) {
                    ALOGV("%s: Received an unexpected wakeup event", __FUNCTION__);
                }
            }
        }
        else {
            ALOGW("%s: Not ready to suspend/shutdown - retrying...", __FUNCTION__);
        }
    }
    ALOGV("%s: Exiting power event monitor thread...", __FUNCTION__);
    pthread_mutex_lock(&gPowerEventMonitorThreadMutex);
    gPowerEventMonitorThreadExited = true;
    pthread_cond_signal(&gPowerEventMonitorThreadCond);
    pthread_mutex_unlock(&gPowerEventMonitorThreadMutex);
    return NULL;
}

static void *power_standby_finish_thread(void *arg __unused)
{
    status_t status = NO_ERROR;

    ALOGV("%s: entered", __FUNCTION__);

    status = power_set_state(ePowerState_S0);

    if (gNexusPower.get()) {
        gNexusPower->setVideoOutputsState(ePowerState_S0);
    }

    // Monitor for shutdown events
    return power_event_monitor_thread(NULL);
}

static void power_kill_thread()
{
    // If the event monitor thread is running, then terminate it...
    pthread_mutex_lock(&gPowerEventMonitorThreadMutex);
    if (!gPowerEventMonitorThreadExited) {
        // Signal Event Monitor Thread to exit...
        gPowerEventMonitorThreadExit = true;
        // and wait for it to exit...
        while (!gPowerEventMonitorThreadExited) {
            pthread_cond_wait(&gPowerEventMonitorThreadCond, &gPowerEventMonitorThreadMutex);
        }
    }
    pthread_mutex_unlock(&gPowerEventMonitorThreadMutex);
}

static status_t power_create_thread(void *(pthread_function)(void*))
{
    status_t status;
    pthread_t thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&thread, &attr, pthread_function, NULL) == 0) {
        ALOGV("%s: Successfully created a power thread!", __FUNCTION__);
        status = NO_ERROR;
    }
    else {
        ALOGE("%s: Could not create a power thread!!!", __FUNCTION__);
        status = UNKNOWN_ERROR;
    }
    pthread_attr_destroy(&attr);
    return status;
}

static void power_finish_set_no_interactive()
{
    // Ensure we acquire a wake-lock to prevent the system from suspending *BEFORE* we have
    // placed Nexus in to standby (we should not suspend in S0.5/S1/S5 modes anyway).
    acquireWakeLock();

    // Listen for standby and suspend messages
    power_kill_thread();
    power_create_thread(power_doze_monitor_thread);
}

static status_t power_finish_set_interactive()
{
    status_t status = NO_ERROR;

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

    power_kill_thread();
    power_create_thread(power_standby_finish_thread);

    // Release the CPU wakelock as the system should be back up now...
    releaseWakeLock();
    return status;
}

static void power_set_interactive(struct power_module *module __unused, int on)
{
    ALOGI("%s: %s", __FUNCTION__, on ? "ON" : "OFF");

    if (on) {
        if (shutdownStarted) {
            ALOGI("%s: Ignoring interaction, shutdown in progress.", __FUNCTION__);
            return;
        }
        power_finish_set_interactive();
    }
    else {
        b_powerState dozeState = power_get_property_doze_state();

        if (gNexusPower.get()) {
            gNexusPower->setVideoOutputsState(dozeState);
        }

        power_finish_set_no_interactive();
    }

    if (gPowerFd != -1) {
        ALOGV("%s: Set interactive %s with droid_pm driver...", __FUNCTION__, on ? "on" : "off");
        if (ioctl(gPowerFd, BRCM_IOCTL_SET_INTERACTIVE, &on)) {
            ALOGE("%s: Error trying to set interactive state in droid_pm driver !!!", __FUNCTION__);
        }
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
    .open = NULL
};

struct power_module HAL_MODULE_INFO_SYM = {
   .common = {
      .tag                = HARDWARE_MODULE_TAG,
      .module_api_version = POWER_MODULE_API_VERSION_0_3,
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
    .setFeature           = power_set_feature
};
