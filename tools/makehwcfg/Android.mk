# Copyright (C) 2015 The Android Open Source Project
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

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

$(PRODUCT_OUT_FROM_TOP)/hwcfg:
	mkdir -p $(PRODUCT_OUT_FROM_TOP)/hwcfg

ifneq ($(HW_WIFI_SUPPORT),n)
ifneq ($(wildcard $(BRCM_DHD_NVRAM_DIR)/$(BRCM_DHD_NVRAM_NAME)),)
_hwcfg_dhd_nvram_file := $(PRODUCT_OUT_FROM_TOP)/hwcfg/nvm.txt

ifneq ($(wildcard $(ANDROID_TOP)/hwcfg/wifimac.txt),)
$(_hwcfg_dhd_nvram_file): $(PRODUCT_OUT_FROM_TOP)/hwcfg ${BRCM_DHD_NVRAM_DIR}/${BRCM_DHD_NVRAM_NAME} $(ANDROID_TOP)/hwcfg/wifimac.txt
	sed -e "s/macaddr=\([0-9a-fA-F][0-9a-fA-F]:\)\{5\}[0-9a-fA-F][0-9a-fA-F]/macaddr=$$(cat $(ANDROID_TOP)/hwcfg/wifimac.txt)/" ${BRCM_DHD_NVRAM_DIR}/${BRCM_DHD_NVRAM_NAME} > $@
	@echo "Found wifimac.txt, updated nvm.txt with macaddr=$$(cat $(ANDROID_TOP)/hwcfg/wifimac.txt)"
else
$(_hwcfg_dhd_nvram_file): $(PRODUCT_OUT_FROM_TOP)/hwcfg ${BRCM_DHD_NVRAM_DIR}/${BRCM_DHD_NVRAM_NAME}
	cp ${BRCM_DHD_NVRAM_DIR}/${BRCM_DHD_NVRAM_NAME} $@
	@echo "Could not find $(ANDROID_TOP)/hwcfg/wifimac.txt, nvm.txt will contain default mac address"
endif
endif
endif

ifneq ($(wildcard $(ANDROID_TOP)/hwcfg/drm.bin),)
_hwcfg_drm_file := $(PRODUCT_OUT_FROM_TOP)/hwcfg/drm.bin
$(_hwcfg_drm_file): $(PRODUCT_OUT_FROM_TOP)/hwcfg $(ANDROID_TOP)/hwcfg/drm.bin
	cp $(ANDROID_TOP)/hwcfg/drm.bin $@
	@echo "Found drm.bin, included in hwcfg.img"
endif

ifneq ($(wildcard $(ANDROID_TOP)/hwcfg/drm_hdcp1x.bin),)
_hwcfg_drm_hdcp1x_file := $(PRODUCT_OUT_FROM_TOP)/hwcfg/drm_hdcp1x.bin
$(_hwcfg_drm_hdcp1x_file): $(PRODUCT_OUT_FROM_TOP)/hwcfg $(ANDROID_TOP)/hwcfg/drm_hdcp1x.bin
	cp $(ANDROID_TOP)/hwcfg/drm_hdcp1x.bin $@
	@echo "Found drm_hdcp1x.bin, included in hwcfg.img"
endif

_hwcfg.img := $(PRODUCT_OUT_FROM_TOP)/hwcfg.img
$(_hwcfg.img): $(PRODUCT_OUT_FROM_TOP)/hwcfg $(_hwcfg_dhd_nvram_file) $(_hwcfg_drm_file) $(_hwcfg_drm_hdcp1x_file)
	mkfs.cramfs -n hwcfg $(PRODUCT_OUT_FROM_TOP)/hwcfg $@

LOCAL_MODULE := makehwcfg
LOCAL_MODULE_TAGS := optional
LOCAL_ADDITIONAL_DEPENDENCIES := $(_hwcfg.img)

include $(BUILD_PHONY_PACKAGE)

_hwcfg_dhd_nvram_file :=
_hwcfg_drm_file :=
_hwcfg_drm_hdcp1x_file :=
_hwcfg.img :=
