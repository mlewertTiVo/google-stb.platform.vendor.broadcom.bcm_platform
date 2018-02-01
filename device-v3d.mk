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
V3D_ANDROID_DEFINES += -I$(ANDROID_TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/hals/gralloc/${HAL_GR_VERSION}
V3D_ANDROID_DEFINES += $(NEXUS_APP_CFLAGS)
include $(TOP)/${BCM_VENDOR_STB_ROOT}/refsw/nexus/nxclient/include/nxclient.inc
V3D_ANDROID_DEFINES += $(NXCLIENT_CFLAGS)

V3D_ANDROID_DEFINES_1ST_ARCH := $(V3D_ANDROID_DEFINES)
V3D_ANDROID_DEFINES_2ND_ARCH := $(V3D_ANDROID_DEFINES)

ifeq ($(B_REFSW_USES_CLANG),y)
ifeq ($(TARGET_2ND_ARCH),arm)
V3D_ANDROID_DEFINES_1ST_ARCH += -target aarch64-linux-android -B${B_REFSW_CROSS_COMPILE_PATH_1ST_ARCH}
V3D_ANDROID_LD_1ST_ARCH := -target aarch64-linux-android -B${B_REFSW_CROSS_COMPILE_PATH_1ST_ARCH}
V3D_ANDROID_DEFINES_2ND_ARCH += -target arm-linux-androideabi -B${B_REFSW_CROSS_COMPILE_PATH_2ND_ARCH}
V3D_ANDROID_LD_2ND_ARCH := -target arm-linux-androideabi -B${B_REFSW_CROSS_COMPILE_PATH_2ND_ARCH}
else
V3D_ANDROID_DEFINES_1ST_ARCH += -target arm-linux-androideabi -B${B_REFSW_CROSS_COMPILE_PATH_1ST_ARCH}
V3D_ANDROID_LD_1ST_ARCH := -target arm-linux-androideabi -B${B_REFSW_CROSS_COMPILE_PATH_1ST_ARCH}
endif
endif

# do not strip symbols.
V3D_DEBUG := y

export V3D_ANDROID_DEFINES_1ST_ARCH V3D_ANDROID_LD_1ST_ARCH
export V3D_ANDROID_DEFINES_2ND_ARCH V3D_ANDROID_LD_2ND_ARCH
export V3D_DEBUG

# vc4 v3d.
ifeq ($(V3D_PREFIX),v3d)

# backward compatibility.
V3D_ANDROID_LD := ${V3D_ANDROID_LD_1ST_ARCH}
export V3D_ANDROID_DEFINES V3D_ANDROID_LD

include ${BSEAV_TOP}/lib/gpu/v3d/driver/Android.mk
include ${BSEAV_TOP}/lib/gpu/tools/gpumon_hook/Android.mk

# vc5 v3d.
else

include ${BSEAV_TOP}/lib/gpu/vc5/driver/Android.mk
include ${BSEAV_TOP}/lib/gpu/tools/gpumon_hook/Android.mk

endif

