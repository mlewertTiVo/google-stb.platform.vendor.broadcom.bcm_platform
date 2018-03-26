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

#ifndef __HWCSVC__H__INCLUDED__
#define __HWCSVC__H__INCLUDED__

#include <IHwc.h>

enum {
   HWC_BINDER_NTFY_CONNECTED = 1,
   HWC_BINDER_NTFY_DISCONNECTED,
   HWC_BINDER_NTFY_DISPLAY,
   HWC_BINDER_NTFY_VIDEO_SURFACE_ACQUIRED,
   HWC_BINDER_NTFY_SIDEBAND_SURFACE_ACQUIRED,
   HWC_BINDER_NTFY_VIDEO_SURFACE_GEOMETRY_UPDATE,
   HWC_BINDER_NTFY_SIDEBAND_SURFACE_GEOMETRY_UPDATE,
   HWC_BINDER_NTFY_OVERSCAN,
   HWC_BINDER_NTFY_EVALPLM,
   HWC_BINDER_NTFY_NO_VIDEO_CLIENT,
};

enum {
   HWC_BINDER_UNDEF = 1,
   HWC_BINDER_HWC,
   HWC_BINDER_OMX,
   HWC_BINDER_SDB,
   HWC_BINDER_COM,
};

using namespace android;

const sp<IHwc>& get_hwc(bool retry);

#endif // __HWCSVC__H__INCLUDED__

