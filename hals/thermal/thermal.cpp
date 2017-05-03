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

#define LOG_TAG "ThermalHAL"

#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <utils/Log.h>
#include <hardware/hardware.h>
#include <hardware/thermal.h>

#define CPU_LABEL               "CPU"
#define MAX_LENGTH              50

#define CPU_USAGE_FILE          "/proc/stat"
#define TEMPERATURE_DIR         "/sys/class/thermal"
#define THERMAL_DIR             "thermal_zone"
#define CPU_ONLINE_FILE_FORMAT  "/sys/devices/system/cpu/cpu%d/online"
#define SHUTDOWN_TEMPERATURE    125.0
#define THROTTLE_TEMPERATURE    110.0

static ssize_t get_temperatures(thermal_module_t *, temperature_t *list, size_t size) {
    char file_name[MAX_LENGTH];
    FILE *file;
    float temp;
    size_t idx = 0;
    DIR *dir;
    struct dirent *de;

    /** Read all available temperatures from
     * /sys/class/thermal/thermal_zone[0-9]+/temp files.
     * Don't guarantee that all temperatures are in Celsius. */
    dir = opendir(TEMPERATURE_DIR);
    if (dir == 0) {
        ALOGE("%s: failed to open directory %s: %s", __func__, TEMPERATURE_DIR, strerror(-errno));
        return -errno;
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

            temp = temp/1000; /* Convert from millicelsius to celsius*/

            if (list != NULL && idx < size) {
                list[idx] = (temperature_t) {
                    .name = CPU_LABEL,
                    .type = DEVICE_TEMPERATURE_CPU,
                    .current_value = temp,
                    .throttling_threshold = THROTTLE_TEMPERATURE,
                    .shutdown_threshold = SHUTDOWN_TEMPERATURE,
                    .vr_throttling_threshold = UNKNOWN_TEMPERATURE,
                };
            }
            fclose(file);
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
    char cpu_name[MAX_LENGTH];
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
        }
        fclose(cpu_file);

        snprintf(cpu_name, MAX_LENGTH, "%s%d", CPU_LABEL, cpu_num);

        if (list != NULL) {
            list[size] = (cpu_usage_t) {
                .name = cpu_name,
                .active = active,
                .total = total,
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

thermal_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = THERMAL_HARDWARE_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = THERMAL_HARDWARE_MODULE_ID,
        .name = "bcm_platform thermal module",
        .author = "Broadcom Limited",
        .methods = &thermal_module_methods,
    },

    .getTemperatures = get_temperatures,
    .getCpuUsages = get_cpu_usages,
    .getCoolingDevices = get_cooling_devices,
};
