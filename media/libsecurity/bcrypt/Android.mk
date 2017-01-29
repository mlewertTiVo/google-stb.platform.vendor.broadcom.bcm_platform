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
LOCAL_PATH := ${REFSW_BASE_DIR}/BSEAV/lib/security/bcrypt/src
LOCAL_PATH := $(subst ${ANDROID}/,,$(LOCAL_PATH))

include $(CLEAR_VARS)

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
    $(TOP)/${BCM_VENDOR_STB_ROOT}/bcm_platform/media/libsecurity/bdbg2alog \
    ${REFSW_BASE_DIR}/BSEAV/lib/security/bcrypt/include \
    $(TOP)/external/boringssl/include
LOCAL_C_INCLUDES := $(subst ${ANDROID}/,,$(LOCAL_C_INCLUDES))

LOCAL_CFLAGS := $(NEXUS_APP_CFLAGS)
ifeq ($(ANDROID_USES_BORINGSSL),y)
LOCAL_CFLAGS += -DUSES_BORINGSSL
endif

# Enable warning and error logs by default
LOCAL_CFLAGS += -DBDBG2ALOG_ENABLE_LOGS=1 -DBDBG_NO_MSG=1 -DBDBG_NO_LOG=1
ifneq ($(TARGET_BUILD_TYPE),debug)
# Enable error logs for non debug build
LOCAL_CFLAGS += -DBDBG_NO_WRN=1
endif

LOCAL_SHARED_LIBRARIES := liblog libssl libcrypto libnexus

LOCAL_MODULE := libbcrypt
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

