/*
 * Copyright (C) 2016-2017 The Android Open Source Project
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

#define LOG_TAG "bcm-thermal"

#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <utils/Log.h>
#include <hardware/hardware.h>
#include <hardware/thermal.h>
#include "nxclient.h"

#define MAX_LENGTH              50

#define CPU_USAGE_FILE          "/proc/stat"
#define TEMPERATURE_DIR         "/sys/class/thermal"
#define THERMAL_DIR             "thermal_zone"
#define CPU_ONLINE_FILE_FORMAT  "/sys/devices/system/cpu/cpu%d/online"

extern "C" void* nxwrap_create_client(void **wrap);
extern "C" void nxwrap_destroy_client(void *wrap);
static void *nxWrap = NULL;
static NxClient_ThermalConfiguration nxThermalCfg;
static bool nxThermal = false;

void __attribute__ ((constructor)) thermal_module_load(void);
void __attribute__ ((destructor)) thermal_module_unload(void);

static void nexus_print_thermal_config(NxClient_ThermalConfiguration *config) {
   NxClient_ThermalStatus status;
   unsigned i;

   NxClient_GetThermalStatus(&status);
   ALOGI("ct  : %u", status.temperature);
   if (!status.activeTempThreshold) {
      ALOGI("no active temp threshold defined - no action applied");
      return;
   }
   if (NxClient_GetThermalConfiguration(status.activeTempThreshold, config)) {
      return;
   }
   ALOGI("at  : %u", status.activeTempThreshold);
   ALOGI("ott : %u", config->overTempThreshold);
   ALOGI("tt  : %u", config->overTempThreshold - config->hysteresis);
   ALOGI("otr : %u", config->overTempReset);
   ALOGI("opt : %u", config->overPowerThreshold);
   ALOGI("pi  : %u", config->pollInterval);
   ALOGI("td  : %u", config->tempDelay);
   ALOGI("tj  : %u", config->thetaJC);
   ALOGI("ag ::: use");
   for (i=0; i<sizeof(config->priorityTable)/sizeof(config->priorityTable[0]); i++) {
      if (!config->priorityTable[i].agent) break;
      ALOGI("%u ::: %u",
            config->priorityTable[i].agent,
            status.priorityTable[i].inUse);
   }
}

static void nexus_thermal_callback(
   void *context,
   int param) {

   (void)context;
   (void)param;

   ALOGE("nexus_thermal_callback called.");
}

static void nexus_config_callback(
   void *context,
   int param) {

   (void)context;
   (void)param;

   // TODO: who did that?  we do not allow|expose this currently.
   ALOGW("nexus thermal - configuration changed, really??");

   // change in thermal configuration.
   NxClient_ThermalStatus status;
   NxClient_GetThermalStatus(&status);
   NxClient_GetThermalConfiguration(status.activeTempThreshold, &nxThermalCfg);
   nexus_print_thermal_config(&nxThermalCfg);
}

void thermal_module_load(void) {

   NxClient_ThermalStatus s;
   NxClient_CallbackThreadSettings cts;
   NEXUS_Error rc;

   void *nexus_client = nxwrap_create_client(&nxWrap);
   if (nexus_client == NULL) {
      ALOGE("failed to alloc thermal nexus client, aborting.");
      return;
   }

   NxClient_GetThermalStatus(&s);
   if (s.activeTempThreshold) {
      rc = NxClient_GetThermalConfiguration(s.activeTempThreshold, &nxThermalCfg);
      if (rc == 0) {
         nxThermal = true;
         nexus_print_thermal_config(&nxThermalCfg);
      }
   }

   ALOGI("nexus thermal: %s.",
         nxThermal ? "in use" : "invalid, using default");

   NxClient_GetDefaultCallbackThreadSettings(&cts);
   cts.coolingAgentChanged.callback  = nexus_thermal_callback;
   cts.coolingAgentChanged.context   = (void *)nexus_client;
   cts.thermalConfigChanged.callback = nexus_config_callback;
   cts.thermalConfigChanged.context  = (void *)nexus_client;
   rc = NxClient_StartCallbackThread(&cts);
}

void thermal_module_unload(void) {
   return;
}

// in practice, we only support a single thermal zone at the moment: cpu.
const char *ZONE_LABEL[] = {"CPU", "UNK"};
const size_t ZONE_LABEL_NUM = sizeof(ZONE_LABEL) / sizeof(ZONE_LABEL[0]);

const char *CPU_LABEL[] = {"CPU0", "CPU1", "CPU2", "CPU3"};
const size_t CPU_LABEL_NUM = sizeof(CPU_LABEL) / sizeof(CPU_LABEL[0]);

static ssize_t get_temperatures(thermal_module_t *, temperature_t *list, size_t size) {
    char file_name[MAX_LENGTH];
    FILE *file;
    float temp, tt, st;
    size_t idx = 0;
    DIR *dir;
    struct dirent *de;

    dir = opendir(TEMPERATURE_DIR);
    if (dir == 0) {
        ALOGE("%s: failed to open directory %s: %s", __func__, TEMPERATURE_DIR, strerror(-errno));
        return -errno;
    }

    // if nexus thermal is in place, report the proper threshold.
    tt = UNKNOWN_TEMPERATURE;
    st = UNKNOWN_TEMPERATURE;
    if (nxThermal) {
       tt = (float)nxThermalCfg.overTempThreshold / 1000;
       st = (float)nxThermalCfg.overTempReset / 1000;
    }

    while ((de = readdir(dir))) {
        if (!strncmp(de->d_name, THERMAL_DIR, strlen(THERMAL_DIR))) {
            snprintf(file_name, MAX_LENGTH, "%s/%s/temp", TEMPERATURE_DIR, de->d_name);
            file = fopen(file_name, "r");
            if (file == NULL) {
                continue;
            }
            if (1 != fscanf(file, "%f", &temp)) {
                fclose(file);
                continue;
            }
            fclose(file);

            temp = temp/1000; /* Convert from millicelsius to celsius*/

            if (idx >= ZONE_LABEL_NUM) {
                /* Should never happen but bailing just in case */
                ALOGE("%s: out of zone labels", __func__);
                closedir(dir);
                return -EPERM;
            }

            if (list != NULL && idx < size) {
                list[idx] = (temperature_t) {
                    .name                    = ZONE_LABEL[idx],
                    .type                    = DEVICE_TEMPERATURE_CPU, /* only device supported. */
                    .current_value           = temp,
                    .throttling_threshold    = tt,
                    .shutdown_threshold      = st,
                    .vr_throttling_threshold = UNKNOWN_TEMPERATURE,    /* not applicable. */
                };
            }
            idx++;
        }
    }
    closedir(dir);
    return idx;
}

static ssize_t get_cpu_usages(thermal_module_t *, cpu_usage_t *list) {
    int vals, cpu_num, online;
    ssize_t read;
    uint64_t user, nice, system, idle, active, total;
    char *line = NULL;
    size_t len = 0;
    size_t size = 0;
    char file_name[MAX_LENGTH];
    FILE *cpu_file;
    FILE *file = fopen(CPU_USAGE_FILE, "r");

    if (file == NULL) {
        ALOGE("%s: failed to open: %s", __func__, strerror(errno));
        return -errno;
    }

    while ((read = getline(&line, &len, file)) != -1) {
        // Skip non "cpu[0-9]" lines.
        if (strnlen(line, read) < 4 || strncmp(line, "cpu", 3) != 0 || !isdigit(line[3])) {
            free(line);
            line = NULL;
            len = 0;
            continue;
        }
        vals = sscanf(line, "cpu%d %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64, &cpu_num, &user,
                &nice, &system, &idle);

        free(line);
        line = NULL;
        len = 0;

        if (vals != 5) {
            ALOGE("%s: failed to read CPU information from file: %s", __func__, strerror(errno));
            fclose(file);
            return errno ? -errno : -EIO;
        }

        active = user + nice + system;
        total = active + idle;

        // Read online CPU information.
        snprintf(file_name, MAX_LENGTH, CPU_ONLINE_FILE_FORMAT, cpu_num);
        cpu_file = fopen(file_name, "r");
        online = 0;
        if (cpu_file == NULL) {
            ALOGE("%s: failed to open file: %s (%s)", __func__, file_name, strerror(errno));
            // /sys/devices/system/cpu/cpu0/online is missing on some systems, because cpu0 can't
            // be offline.
            online = cpu_num == 0;
        } else if (1 != fscanf(cpu_file, "%d", &online)) {
            ALOGE("%s: failed to read CPU online information from file: %s (%s)", __func__,
                    file_name, strerror(errno));
            fclose(file);
            fclose(cpu_file);
            return errno ? -errno : -EIO;
        } else {
            fclose(cpu_file);
        }

        if (cpu_num >= (int)CPU_LABEL_NUM) {
            /* Should never happen but bailing just in case */
            ALOGE("%s: out of cpu labels", __func__);
            fclose(file);
            return -EPERM;
        }

        if (list != NULL) {
            list[size] = (cpu_usage_t) {
                .name      = CPU_LABEL[cpu_num],
                .active    = active,
                .total     = total,
                .is_online = static_cast<bool>(online)
            };
        }

        size++;
    }

    fclose(file);
    return size;
}

static ssize_t get_cooling_devices(thermal_module_t *, cooling_device_t *, size_t) {
    return 0;
}

static struct hw_module_methods_t thermal_module_methods = {
    .open = NULL,
};

thermal_module_t HAL_MODULE_INFO_SYM
__attribute__ ((visibility ("default"))) = {
    .common = {
      .tag                = HARDWARE_MODULE_TAG,
      .module_api_version = THERMAL_HARDWARE_MODULE_API_VERSION_0_1,
      .hal_api_version    = HARDWARE_HAL_API_VERSION,
      .id                 = THERMAL_HARDWARE_MODULE_ID,
      .name               = "bcm_platform thermal module",
      .author             = "Broadcom Canada",
      .methods            = &thermal_module_methods,
      .dso                = 0,
      .reserved           = {},
    },
    .getTemperatures      = get_temperatures,
    .getCpuUsages         = get_cpu_usages,
    .getCoolingDevices    = get_cooling_devices,
};
