/*
 * Copyright (C) 2014 Broadcom Canada Ltd.
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

#define LOG_TAG "bcm-hwc-utils"

#include <cutils/log.h>

#include "nexus_surface_client.h"
#include "nxclient.h"

extern "C" NEXUS_SurfaceHandle hwc_to_nsc_surface(int width, int height, int stride, NEXUS_PixelFormat format,
    bool is_mma, unsigned handle, uint8_t *data)
{
    NEXUS_SurfaceHandle shdl = NULL;
    NEXUS_SurfaceCreateSettings createSettings;

    NEXUS_Surface_GetDefaultCreateSettings(&createSettings);
    createSettings.pixelFormat = format;
    createSettings.width       = width;
    createSettings.height      = height;
    createSettings.pitch       = stride;
    if (!is_mma && data) {
        createSettings.pMemory = data;
    } else if (is_mma && handle) {
        createSettings.pixelMemory = (NEXUS_MemoryBlockHandle) handle;
    }

    shdl = NEXUS_Surface_Create(&createSettings);

    ALOGV("%s: (%d,%d), s:%d, fmt:%d, p:%p, h:%p -> %p", __FUNCTION__, width, height, stride, format, data, handle, shdl);
    return shdl;
}
