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

#ifndef __HWCCONV__H__INCLUDED__
#define __HWCCONV__H__INCLUDED__

#ifdef __cplusplus
extern "C"
{
#endif

// inline conversion of an input yv12 planar format (android multimedia)
// into a 422 packed format which nsc can understand.
//
// handle is the gralloc private handle, containing the input data and output
// data pointers and size information.  all memory was allocated via gralloc.
//
NEXUS_Error yv12_to_422planar(
   private_handle_t *handle,
   NEXUS_SurfaceHandle out,
   NEXUS_Graphics2DHandle gfx);

#ifdef __cplusplus
};
#endif

#endif /* __HWCCONV__H__INCLUDED__ */


