/*
 * Copyright (C) 2015 Broadcom Canada Ltd.
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

#ifndef __HWC_COMP__INCLUDED__
#define __HWC_COMP__INCLUDED__

#include "nexus_types.h"
#include "nexus_surface.h"
#include "nexus_surface_compositor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NEXUS_HwcComposer *NEXUS_HwcComposerHandle;
typedef struct NEXUS_HwcClient *NEXUS_HwcClientHandle;

NEXUS_HwcComposerHandle NEXUS_HwcComposer_Create(void);
void NEXUS_HwcComposer_Destroy(NEXUS_HwcComposerHandle handle);

NEXUS_HwcClientHandle NEXUS_HwcComposer_CreateClient(NEXUS_HwcComposerHandle handle);
void NEXUS_HwcComposer_DestroyClient(NEXUS_HwcClientHandle client);

NEXUS_Error NEXUS_HwcComposer_SetClientSettings(NEXUS_HwcComposerHandle handle,
                                                NEXUS_HwcClientHandle client,
                                                const NEXUS_SurfaceComposition *pSettings);
NEXUS_Error NEXUS_HwcComposer_GetClientSettings(NEXUS_HwcComposerHandle handle,
                                                NEXUS_HwcClientHandle client,
                                                NEXUS_SurfaceComposition *pSettings);

int NEXUS_HwcComposer_Compose(NEXUS_HwcComposerHandle handle, NEXUS_SurfaceHandle output);

NEXUS_Error NEXUS_HwcClient_SetSurface(NEXUS_HwcClientHandle client, NEXUS_SurfaceHandle surface);

#ifdef __cplusplus
};
#endif

#endif /* __HWC_COMP__INCLUDED__ */
