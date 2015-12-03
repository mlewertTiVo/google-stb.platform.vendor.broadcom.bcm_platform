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

#ifndef __HWCUTILS__H__INCLUDED__
#define __HWCUTILS__H__INCLUDED__

#ifdef __cplusplus
extern "C"
{
#endif

NEXUS_SurfaceHandle hwc_to_nsc_surface(
   int width,
   int height,
   int stride,
   NEXUS_PixelFormat format,
   unsigned handle,
   unsigned offset);

NEXUS_SurfaceHandle hwc_surface_create(
   const NEXUS_SurfaceCreateSettings *pCreateSettings,
   bool dynamic_heap);

#ifdef __cplusplus
};
#endif

#endif /* __HWCUTILS__H__INCLUDED__ */

