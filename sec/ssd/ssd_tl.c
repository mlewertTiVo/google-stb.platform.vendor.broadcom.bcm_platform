/******************************************************************************
 *  Copyright (C) 2017 Broadcom. The term "Broadcom" refers to Broadcom Limited and/or its subsidiaries.
 *
 *  This program is the proprietary software of Broadcom and/or its licensors,
 *  and may only be used, duplicated, modified or distributed pursuant to the terms and
 *  conditions of a separate, written license agreement executed between you and Broadcom
 *  (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 *  no license (express or implied), right to use, or waiver of any kind with respect to the
 *  Software, and Broadcom expressly reserves all rights in and to the Software and all
 *  intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 *  HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 *  NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
 *
 *  Except as expressly set forth in the Authorized License,
 *
 *  1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 *  secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 *  and to use this information only in connection with your use of Broadcom integrated circuit products.
 *
 *  2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 *  AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
 *  WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 *  THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES
 *  OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE,
 *  LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION
 *  OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF
 *  USE OR PERFORMANCE OF THE SOFTWARE.
 *
 *  3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
 *  LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR
 *  EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR
 *  USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF
 *  THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT
 *  ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
 *  LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF
 *  ANY LIMITED REMEDY.

 ******************************************************************************/
#include <log/log.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ssd_ids.h"
#include "ssd_platform.h"
#include "nexus_platform.h"
#include "nexus_platform_init.h"
#include "bkni.h"
#include "ssd_vfs.h"
#include "ssd_rpmb.h"
#include "ssd_tl.h"
#include "vendor_bcm_props.h"

#include "nxclient.h"
#include "nexus_memory.h"

#include <log/log.h>

/* infinite timeout waiting for sage to asks us to do something. */
#define SSD_WAIT_TIMEOUT (-1)

#define SSD_DEV_OPS_FAIL_THRESHOLD 10

typedef struct ssd_sage_operation_s {
    bool rpmb_partition;
    uint32_t blockCount;
} ssd_sage_operation_t;

static SRAI_ModuleHandle moduleHandle = NULL;
SRAI_ManagementInterface interface;
static ssdd_Settings settings;
static bool hasInit = false;
static bool TA_Terminate_Event = false;
static bool isRunning = true;
static int failedDevOpsCnt = 0;

static BKNI_EventHandle indication_event = NULL;/* used for indication event */
static void  SSDTl_priv_indication_cb(SRAI_ModuleHandle module, uint32_t arg, uint32_t id, uint32_t value);

void SSDTl_Get_Default_Settings(ssdd_Settings *ssdd_settings)
{
    ssd_rpmb_settings rpmb_settings;
    ssd_vfs_settings vfs_settings;

    if (ssdd_settings != NULL) {
        BKNI_Memset((uint8_t *) ssdd_settings, 0x00, sizeof(ssdd_Settings));

        SSD_RPMB_Get_Default_Settings(&rpmb_settings);
        ssdd_settings->rpmb_path = rpmb_settings.dev_path;

        SSD_VFS_Get_Default_Settings(&vfs_settings);
        ssdd_settings->vfs_path = vfs_settings.fs_path;
    }


    return;
}

static ssd_sage_operation_t *sage_operation;
static ssd_rpmb_frame_t *rpmb_frames;

static BERR_Code SSDTl_Operation(int commandId, int deviceResult, int *SSD_TA_rc)
{
    BERR_Code rc = BERR_SUCCESS;
    BSAGElib_InOutContainer *container = SRAI_Container_Allocate();
    if (container == NULL) {
        return BSAGE_ERR_CONTAINER_REQUIRED;
    }

    container->basicIn[0] = deviceResult;
    container->basicIn[1] = sage_operation->rpmb_partition;
    container->blocks[0].data.ptr = (uint8_t *) rpmb_frames;
    container->blocks[0].len = SSD_VFS_MAX_OPERATION_BLOCKS * sizeof(*rpmb_frames);

    rc = SRAI_Module_ProcessCommand(moduleHandle, commandId, container);

    *SSD_TA_rc = container->basicOut[0];
    sage_operation->rpmb_partition = container->basicOut[1];
    sage_operation->blockCount = container->basicOut[2];

    SRAI_Container_Free(container);

    return rc;
}

static BERR_Code SSDTl_Device_Operation(void)
{
    BERR_Code rc = BERR_SUCCESS;

    if (sage_operation->rpmb_partition) {
        /* Execute RPMB op */
        rc = SSD_RPMB_Operation(rpmb_frames, sage_operation->blockCount);
        if (rc != BERR_SUCCESS) {
            ALOGE("RPMB ioctl failed\n");
            goto errorExit;
        }
    } else {
        /* Execute VFS op */
        rc = SSD_VFS_Operation(rpmb_frames, sage_operation->blockCount);
        if (rc != BERR_SUCCESS) {
            ALOGE("VFS operation failed\n");
            goto errorExit;
        }
    }

errorExit:
    return rc;
}

static BERR_Code SSDTl_Perform_Full_Operation_Cycle(int *SSD_TA_rc)
{
    BERR_Code rc = BERR_SUCCESS;

    while (isRunning) {
        ALOGD("SAGE operation received\n");

        rc = SSDTl_Device_Operation();
        if (rc != BERR_SUCCESS) {
           failedDevOpsCnt++;
        } else {
           failedDevOpsCnt = 0;
        }

        rc = SSDTl_Operation(SSD_CommandId_eOperationResult, rc, SSD_TA_rc);
        if ((rc != BERR_SUCCESS) || (*SSD_TA_rc != BERR_SUCCESS)) {
            ALOGE("Error sending result command (%d), TA result (%d)\n", rc, SSD_TA_rc);
            goto errorExit;
        }

        if (sage_operation->blockCount == 0) {
            ALOGD("Operation cycle finished\n");
            break;
        }
    }

errorExit:
    return rc;
}

static void SSDTl_TATerminateCallback (uint32_t platformId) {

    if (platformId != BSAGE_PLATFORM_ID_SSD) {
        ALOGE("Incorrect platform ID reported in terminate callback\n");
        return;
    }

    ALOGD("SSD TA terminate callback event\n");
    TA_Terminate_Event = true;
}

BERR_Code SSDTl_Init(ssdd_Settings *ssdd_settings)
{
    BERR_Code rc = BERR_SUCCESS;
    BERR_Code SSD_TA_rc = BERR_SUCCESS;
    ssd_rpmb_settings rpmb_settings;
    ssd_vfs_settings vfs_settings;
    SRAI_Settings sraiSettings;
    BSAGElib_InOutContainer *container = NULL;
    int rpmb = 1;
    bool rpmb_format = property_get_bool(BCM_PERSIST_SSD_FORMAT_RPMB, 0);
    bool vfs_format = property_get_bool(BCM_PERSIST_SSD_FORMAT_VFS, 0);

    ALOGD("Initialising SSD TA\n");

    if (hasInit) {
        rc = BERR_OS_ERROR;
        ALOGE("SSD TA already initialised\n");
        goto errorExit;
    }

    if (ssdd_settings == NULL) {
        ALOGW("%s - Parameter settings are NULL\n", BSTD_FUNCTION);
        rc = BERR_INVALID_PARAMETER;
        goto errorExit;
    }

    BKNI_Memcpy(&settings, ssdd_settings, sizeof(settings));

    BKNI_Memset(&rpmb_settings, 0, sizeof(rpmb_settings));
    rpmb_settings.dev_path = settings.rpmb_path;
    SSD_RPMB_Init(&rpmb_settings);

    BKNI_Memset(&vfs_settings, 0, sizeof(vfs_settings));
    vfs_settings.fs_path = settings.vfs_path;
    SSD_VFS_Init(&vfs_settings);

    rc = BKNI_CreateEvent(&indication_event);
    if (rc != BERR_SUCCESS) {
        ALOGW("%s - Error Creating BKNI Event handle", BSTD_FUNCTION);
        goto errorExit;
    }

    /* Ensure NEXUS Heaps are correctly setup for NxClient mode */
    SRAI_GetSettings(&sraiSettings);
    sraiSettings.generalHeapIndex = NXCLIENT_FULL_HEAP;
    sraiSettings.videoSecureHeapIndex = NXCLIENT_VIDEO_SECURE_HEAP;
    sraiSettings.exportHeapIndex = NXCLIENT_EXPORT_HEAP;
    SRAI_SetSettings(&sraiSettings);

    container = SRAI_Container_Allocate();
    if (container == NULL) {
        ALOGE("%s - Error allocating container\n", BSTD_FUNCTION);
        rc = BSAGE_ERR_CONTAINER_REQUIRED;
        goto errorExit;
    }

    rc = SSD_ModuleInit(SSD_ModuleId_eSSD_Daemon, NULL, container, &moduleHandle , SSDTl_priv_indication_cb);
    if (rc != BERR_SUCCESS) {
        ALOGE("%s - Error initializing SSD TL module (0x%08x)", BSTD_FUNCTION, container->basicOut[0]);
        goto errorExit;
    }

    BKNI_Memset(&interface, 0, sizeof(interface));
    interface.ta_terminate_callback = SSDTl_TATerminateCallback;
    rc = SRAI_Management_Register(&interface);
    if (rc != BERR_SUCCESS) {
        ALOGE("%s - Error registering terminate callback (0x%08x)", BSTD_FUNCTION, rc);
        goto errorExit;
    }

    sage_operation = (ssd_sage_operation_t *) SRAI_Memory_Allocate(sizeof(*sage_operation), SRAI_MemoryType_Shared);
    if (sage_operation == NULL) {
        ALOGE("%s - Error allocating sage operation structure\n", BSTD_FUNCTION);
        goto errorExit;
    }

    rpmb_frames = (ssd_rpmb_frame_t *) SRAI_Memory_Allocate(SSD_VFS_MAX_OPERATION_BLOCKS * sizeof(*rpmb_frames), SRAI_MemoryType_Shared);
    if (rpmb_frames == NULL) {
        ALOGE("%s - Error allocating frames\n", BSTD_FUNCTION);
        goto errorExit;
    }

    container->blocks[0].data.ptr = (uint8_t *) rpmb_frames;
    container->blocks[0].len = SSD_VFS_MAX_OPERATION_BLOCKS * sizeof(*rpmb_frames);
    container->basicIn[0] = 1; /* Init */

    if (rpmb) {
        /*
         * RPMB
         */
        sage_operation->rpmb_partition = true;

        container->basicIn[1] = sage_operation->rpmb_partition;
        container->basicIn[3] = rpmb_format ? SSD_DaemonInit_eFormatPartition : 0;

        rc = SRAI_Module_ProcessCommand(moduleHandle, SSD_CommandId_eDaemonInit, container);
        if (rc != BERR_SUCCESS) {
            ALOGE("Error sending SSD initialisation command on RPMB partition [%u]\n",
                              container->basicOut[0]);
            goto errorExit;
        }

        rc = container->basicOut[0];
        sage_operation->rpmb_partition = container->basicOut[1];
        sage_operation->blockCount = container->basicOut[2];

        if (rc == BERR_SUCCESS) {
            /* Complete sequence of requested initialisation operations */
            rc = SSDTl_Perform_Full_Operation_Cycle(&SSD_TA_rc);
        } else if (rc == BSAGE_ERR_ALREADY_INITIALIZED) {
            ALOGD("RPMB partition already initialised\n");
            rc = BERR_SUCCESS;
        }

        if ((rc != BERR_SUCCESS) || (SSD_TA_rc != BERR_SUCCESS)) {
            ALOGE("Error initialising RPMB partition\n");
        } else {
           if (rpmb_format) {
              property_set(BCM_PERSIST_SSD_FORMAT_RPMB, "0");
           }
        }
    }

    /*
     * VFS
     */
    sage_operation->rpmb_partition = false;

    container->basicIn[1] = sage_operation->rpmb_partition;
    container->basicIn[3] = vfs_format ? SSD_DaemonInit_eFormatPartition : 0;

    rc = SRAI_Module_ProcessCommand(moduleHandle, SSD_CommandId_eDaemonInit, container);
    if (rc != BERR_SUCCESS) {
        ALOGE("Error sending SSD initialisation command on VFS partition [%u]\n",
                          container->basicOut[0]);
        goto errorExit;
    }

    rc = container->basicOut[0];
    sage_operation->rpmb_partition = container->basicOut[1];
    sage_operation->blockCount = container->basicOut[2];

    if (rc == BERR_SUCCESS) {
        /*  Complete init */
        rc = SSDTl_Perform_Full_Operation_Cycle(&SSD_TA_rc);
        if ((rc != BERR_SUCCESS) || (SSD_TA_rc != BERR_SUCCESS)) {
            ALOGE("Error initialising VFS partition\n");
            rc = BERR_OS_ERROR;
            goto errorExit;
        } else {
           if (vfs_format) {
              property_set(BCM_PERSIST_SSD_FORMAT_VFS, "0");
           }
        }
    } else if (rc == BSAGE_ERR_ALREADY_INITIALIZED) {
        ALOGD("VFS partition already initialised\n");
        rc = BERR_SUCCESS;
    }

    if ((rc != BERR_SUCCESS) || (SSD_TA_rc != BERR_SUCCESS)) {
        ALOGE("Error initialising VFS partition\n");
        goto errorExit;
    }

    ALOGD("Initialised SSD TA\n");
    hasInit = true;

errorExit:
    if (container != NULL) {
        SRAI_Container_Free(container);
    }

    if (!hasInit) {
        // clean up any allocated resources
        ALOGW("%s - Error Exit cleanup\n", BSTD_FUNCTION);
        SSDTl_Uninit();
    }

    return rc;
}

void SSDTl_Uninit(void)
{
    BERR_Code rc = BERR_SUCCESS;
    BSAGElib_InOutContainer *container = NULL;

    ALOGD("Uninitialising SSD TA\n");

    container = SRAI_Container_Allocate();
    if (container != NULL) {
        container->blocks[0].data.ptr = (uint8_t *) rpmb_frames;
        container->blocks[0].len = SSD_VFS_MAX_OPERATION_BLOCKS * sizeof(*rpmb_frames);

        container->basicIn[0] = 0; /* UnInit */

        // RPMB
        container->basicIn[1] = true;
        rc = SRAI_Module_ProcessCommand(moduleHandle, SSD_CommandId_eDaemonInit, container);
        if (rc != BERR_SUCCESS) {
            ALOGW("%s - Error sending uninit command for RPMB (0x%08x)\n", BSTD_FUNCTION, rc);
        }
        rc = container->basicOut[0];

        // VFS
        container->basicIn[1] = false;
        rc = SRAI_Module_ProcessCommand(moduleHandle, SSD_CommandId_eDaemonInit, container);
        if (rc != BERR_SUCCESS) {
            ALOGW("%s - Error sending uninit command for VFS (0x%08x)\n", BSTD_FUNCTION, rc);
        }

        SRAI_Container_Free(container);
    } else {
        ALOGW("%s - Error allocating container used to uninitialise\n", BSTD_FUNCTION);
    }

    /* After sending UnInit command to SAGE, close the module handle */
    if (moduleHandle != NULL) {
        SSD_ModuleUninit(moduleHandle);
        moduleHandle = NULL;
    }

    if (rpmb_frames) {
        SRAI_Memory_Free((uint8_t *) rpmb_frames);
        rpmb_frames = NULL;
    }

    if (sage_operation) {
        SRAI_Memory_Free((uint8_t *) sage_operation);
        sage_operation = NULL;
    }

    rc = SRAI_Management_Unregister(&interface);
    if (rc != BERR_SUCCESS) {
        ALOGW("%s - Error unregistering terminate callback (0x%08x)", BSTD_FUNCTION, rc);
    }

    if (indication_event != NULL) {
        BKNI_DestroyEvent(indication_event);
        indication_event =  NULL;
    }

    ALOGD("Uninitialised SSD TA\n");
    hasInit = false;
}

void SSDTl_Wait_For_Operations(void)
{
    BERR_Code rc = BERR_SUCCESS;
    BERR_Code SSD_TA_rc = BERR_SUCCESS;
    bool checkAgain = false;
    bool exitOnDeviceOpsFailure = false;
    ALOGD("Start waiting for operations\n");

    while (isRunning) {

        if (exitOnDeviceOpsFailure) {
           ALOGE("too many consecutive device operations failure, exiting...");
           SSDTl_Uninit();
           goto errorExit;
        }

        if (TA_Terminate_Event) {
            SSDTl_Uninit();

            rc = SSDTl_Init(&settings);
            if (rc != BERR_SUCCESS){
                ALOGE("ERROR SSDTl_Init %x\n", rc);
                goto errorExit;
            }

            TA_Terminate_Event = false;
        }

        if (checkAgain) {
            rc = BERR_SUCCESS;
            checkAgain = false;
        } else {
            rc = BKNI_WaitForEvent(indication_event, SSD_WAIT_TIMEOUT);
        }

        if (rc == BERR_SUCCESS) {

            rc = SSDTl_Operation(SSD_CommandId_eOperationGet, rc, &SSD_TA_rc);
            if (rc != BERR_SUCCESS) {
                ALOGE("Error getting operation\n");
                continue;
            }

            if (sage_operation->blockCount) {
                rc = SSDTl_Perform_Full_Operation_Cycle(&SSD_TA_rc);
                if (rc != BERR_SUCCESS) {
                    ALOGE("Error occurred completing operation cycle\n");
                    continue;
                }

                if (SSD_TA_rc != BERR_SUCCESS) {
                    ALOGE("Operation cycle failed\n");
                }

                if (failedDevOpsCnt > SSD_DEV_OPS_FAIL_THRESHOLD) {
                   exitOnDeviceOpsFailure = true;
                } else {
                   /* Set flag to check again if we have an operation ready, in
                    * case we missed an event while processing the previous
                    * operation */
                   checkAgain = true;
                }
            }
        } else {
            ALOGE("BKNI_WaitForEvent failed (rc=%d)\n", rc);
            goto errorExit;
        }
    }

errorExit:
    ALOGD("Stop waiting for operations\n");
}
/*Callback for indication event*/
static void  SSDTl_priv_indication_cb(SRAI_ModuleHandle module, uint32_t arg, uint32_t id, uint32_t value)
{
    BERR_Code rc = BERR_SUCCESS;
    BSTD_UNUSED(arg);
    BSTD_UNUSED(value);

    if ((id == SSD_IndicationType_eOperationSend) && (module == moduleHandle)){
        ALOGI("Indication event received, Id: %d Val: %d \n", id, value);
        BKNI_SetEvent(indication_event);
    }
    return;
}

void SSDTl_Shutdown(void)
{
    ALOGD("Shutting down");
    isRunning = false;
    BKNI_SetEvent(indication_event);
    SSDTl_Uninit();
}
