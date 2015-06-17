# Copyright (C) 2008 The Android Open Source Project
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

#-------------
# libbcrypt.so
#-------------
LOCAL_PATH := $(call my-dir)/../../../refsw/BSEAV/lib/security/bcrypt/src

include $(CLEAR_VARS)

ifeq ($(NEXUS_MODE),proxy)
NEXUS_LIB=libnexus
else
ifeq ($(NEXUS_WEBCPU),core1_server)
NEXUS_LIB=libnexus_webcpu
else
NEXUS_LIB=libnexus_client
endif
endif

LOCAL_SRC_FILES := \
    bcrypt.c \
    bcrypt_aes_sw.c \
    bcrypt_aescbc_sw.c \
    bcrypt_aesecb_sw.c \
    bcrypt_asn1_sw.c \
    bcrypt_cmac_sw.c \
    bcrypt_dbg.c \
    bcrypt_descbc_sw.c \
    bcrypt_desecb_sw.c \
    bcrypt_dh_sw.c \
    bcrypt_dsa_sw.c \
    bcrypt_ecc_sw.c \
    bcrypt_ecdsa_sw.c \
    bcrypt_md5_sw.c \
    bcrypt_rc4_sw.c \
    bcrypt_rng_sw.c \
    bcrypt_rsa_sw.c \
    bcrypt_sha1_sw.c \
    bcrypt_sha256_sw.c \
    bcrypt_x509_sw.c

LOCAL_C_INCLUDES := \
    $(TOP)/vendor/broadcom/stb/refsw/BSEAV/lib/security/bcrypt/include \
    $(TOP)/external/openssl/include \
    $(NEXUS_APP_INCLUDE_PATHS)

LOCAL_CFLAGS += -DPIC -fpic -DANDROID
LOCAL_CFLAGS += $(NEXUS_CFLAGS)
LOCAL_CFLAGS += $(addprefix -D,$(NEXUS_APP_DEFINES))
LOCAL_CFLAGS += -DBDBG_NO_MSG -DBDBG_NO_WRN -DBDBG_NO_ERR -DBDBG_NO_LOG

LOCAL_SHARED_LIBRARIES := libssl libcrypto $(NEXUS_LIB)

LOCAL_MODULE := libbcrypt
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

