# Copyright (C) 2016 Broadcom Limited
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

V3D_ANDROID_DEFINES := -I$(ANDROID_TOP)/${BCM_VENDOR_STB_ROOT}/drivers/nx_ashmem
V3D_ANDROID_DEFINES += -I$(ANDROID_TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/gralloc
V3D_ANDROID_DEFINES += $(NEXUS_APP_CFLAGS)

V3D_ANDROID_LD :=
ifeq ($(B_REFSW_USES_CLANG),y)
V3D_ANDROID_DEFINES += -target arm-linux-androideabi -B${B_REFSW_CROSS_COMPILE_PATH}
V3D_ANDROID_LD := -target arm-linux-androideabi -B${B_REFSW_CROSS_COMPILE_PATH}
endif

include $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/include/nxclient.inc
V3D_ANDROID_DEFINES += $(NXCLIENT_CFLAGS)
V3D_ANDROID_LD += $(NXCLIENT_LDFLAGS)

ifeq ($(B_REFSW_DEBUG), n)
V3D_DEBUG := n
else
V3D_DEBUG := y
endif

export V3D_ANDROID_DEFINES V3D_ANDROID_LD V3D_DEBUG

# vc4 v3d.
ifeq ($(V3D_PREFIX),v3d)

include ${ROCKFORD_TOP}/middleware/v3d/driver/Android.mk
include ${ROCKFORD_TOP}/middleware/v3d/tools/spyhook/Android.mk

# vc5 v3d.
else

include ${ROCKFORD_TOP}/middleware/vc5/driver/Android.mk
include ${ROCKFORD_TOP}/middleware/vc5/tools/gpumon_hook/Android.mk

endif

