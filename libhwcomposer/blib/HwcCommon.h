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

#ifndef __HWC_COMMON__H__INCLUDED__
#define __HWC_COMMON__H__INCLUDED__

struct hwc_position {
    int x;
    int y;
    int w;
    int h;
};

struct hwc_notification_info {
   int surface_hdl;
   int display_width;
   int display_height;
   /* following is only relevant for HWC_BINDER_NTFY_DISPLAY | HWC_BINDER_NTFY_OVERSCAN */
   int frame_id;
   struct hwc_position frame;
   struct hwc_position clipped;
   int zorder;
};

#endif // __HWC_COMMON__H__INCLUDED__
