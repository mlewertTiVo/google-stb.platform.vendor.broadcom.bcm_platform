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

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "ssd_platform.h"

#include "bdbg.h"
#include "bkni.h"
#include "bkni_multi.h"

#if (NEXUS_SECURITY_API_VERSION == 1)
#include "nexus_otpmsp.h"
#else
#include "nexus_otp_msp.h"
#endif
#include "nexus_security_datatypes.h"
#include "drm_metadata_tl.h"
#include "nexus_base_os.h"

BDBG_MODULE(ssd_tl);

/* helper macro */
#define GET_UINT32_FROM_BUF(pBuf) \
    (((uint32_t)(((uint8_t*)(pBuf))[0]) << 24) | \
     ((uint32_t)(((uint8_t*)(pBuf))[1]) << 16) | \
     ((uint32_t)(((uint8_t*)(pBuf))[2]) << 8)  | \
     ((uint8_t *)(pBuf))[3])

/* definitions */
#define DEFAULT_DRM_BIN_FILESIZE (128*1024)
#define OVERWRITE_BIN_FILE (1)

static uint8_t *drm_bin_file_buff = NULL;

static SRAI_PlatformHandle platformHandle = NULL;

static uint32_t SSD_P_CheckDrmBinFileSize(void);
static BERR_Code SSD_P_GetFileSize(const char * filename, uint32_t *filesize);
BERR_Code SSD_P_TA_Install(char * ta_bin_filename);
ChipType_e SSD_P_GetChipType(void);

#define SSD_TA_NAME_PRODUCTION  "sage_ta_ssd.bin"
#define SSD_TA_NAME_DEVELOPMENT "sage_ta_ssd_dev.bin"

BERR_Code
SSD_ModuleInit(SSD_ModuleId_e module_id,
                   const char * drm_bin_filename,
                   BSAGElib_InOutContainer *container,
                   SRAI_ModuleHandle *moduleHandle,
                   SRAI_IndicationCallback indication_cb)
{
    BERR_Code rc = BERR_SUCCESS;
    FILE * fptr = NULL;
    uint32_t read_size = 0;
    uint32_t filesize_from_header = 0;
    uint32_t filesize = 0;
    uint32_t write_size = 0;
    BERR_Code sage_rc = BERR_SUCCESS;
    SRAI_ModuleHandle tmpModuleHandle = NULL;
    SRAI_Module_InitSettings init_settings;
#if SAGE_VERSION >= SAGE_VERSION_CALC(3,0)
    char * ta_bin_filename;
#endif
    BDBG_ENTER(SSD_ModuleInit);

    if (moduleHandle == NULL)
    {
        rc = BERR_OS_ERROR;
        goto ErrorExit;
    }

    (*moduleHandle) = NULL;


    if(platformHandle == NULL)
    {
#if SAGE_VERSION >= SAGE_VERSION_CALC(3,0)

        if(SSD_P_GetChipType() == ChipType_eZS)
            ta_bin_filename = SSD_TA_NAME_DEVELOPMENT;
        else
            ta_bin_filename = SSD_TA_NAME_PRODUCTION;

        sage_rc = SSD_P_TA_Install(ta_bin_filename);

        if (sage_rc != BERR_SUCCESS)
        {
            BDBG_WRN(("%s - Could not Install TA %s: Make sure you have the TA binary", BSTD_FUNCTION, ta_bin_filename));
            rc = sage_rc;
            goto ErrorExit;
        }
#endif
        BSAGElib_State platform_status;
        sage_rc = SRAI_Platform_Open(BSAGE_PLATFORM_ID_SSD, &platform_status, &platformHandle);
        if (sage_rc != BERR_SUCCESS)
        {
            platformHandle = NULL; /* sanity reset */
            BDBG_ERR(("%s - Error calling platform_open", BSTD_FUNCTION));
            rc = sage_rc;
            goto ErrorExit;
        }

        BDBG_MSG(("%s - SRAI_Platform_Open(%u, %p, %p) returned %p", BSTD_FUNCTION, BSAGE_PLATFORM_ID_SSD, (void *)&platform_status, (void *)&platformHandle, (void *)platformHandle));

        if(platform_status == BSAGElib_State_eUninit)
        {
            BDBG_WRN(("%s - platform_status == BSAGElib_State_eUninit ************************* (platformHandle = %p)", BSTD_FUNCTION, platformHandle));
            sage_rc = SRAI_Platform_Init(platformHandle, NULL);
            if (sage_rc != BERR_SUCCESS)
            {
                BDBG_ERR(("%s - Error calling platform init", BSTD_FUNCTION));
                rc = sage_rc;
                goto ErrorExit;
            }
        }
        else{
            BDBG_WRN(("%s - Platform already initialized *************************", BSTD_FUNCTION));
        }
    }

    /* if module uses a bin file - read it and add it to the container */
    if(drm_bin_filename != NULL)
    {
        if (container == NULL)
        {
            rc = BSAGE_ERR_CONTAINER_REQUIRED;
            BDBG_ERR(("%s - container is required to hold bin file", BSTD_FUNCTION));
            goto ErrorExit;
        }

        /* first verify that it has not been already used by the calling module */
        if(container->blocks[0].data.ptr != NULL)
        {
            BDBG_ERR(("%s - Shared block[0] reserved for all DRM modules with bin file to pass to Sage.", BSTD_FUNCTION));
            rc = BERR_INVALID_PARAMETER;
            goto ErrorExit;
        }

        BDBG_MSG(("%s - DRM bin filename '%s'", BSTD_FUNCTION, drm_bin_filename));
        /*
         * 1) allocate drm_bin_file_buff
         * 2) read bin file
         * 3) check size
         * */
        rc = SSD_P_GetFileSize(drm_bin_filename, &filesize);
        if(rc != BERR_SUCCESS)
        {
            BDBG_ERR(("%s - Error determine file size of bin file", BSTD_FUNCTION));
            goto ErrorExit;
        }

        drm_bin_file_buff = SRAI_Memory_Allocate(filesize, SRAI_MemoryType_Shared);
        if(drm_bin_file_buff == NULL)
        {
            BDBG_ERR(("%s - Error allocating '%u' bytes", BSTD_FUNCTION, filesize));
            rc = BERR_OUT_OF_SYSTEM_MEMORY;
            goto ErrorExit;
        }

        fptr = fopen(drm_bin_filename, "rb");
        if(fptr == NULL)
        {
            BDBG_ERR(("%s - Error opening drm bin file (%s)", BSTD_FUNCTION, drm_bin_filename));
            rc = BERR_OS_ERROR;
            goto ErrorExit;
        }

        read_size = fread(drm_bin_file_buff, 1, filesize, fptr);
        if(read_size != filesize)
        {
            BDBG_ERR(("%s - Error reading drm bin file size (%u != %u)", BSTD_FUNCTION, read_size, filesize));
            rc = BERR_OS_ERROR;
            goto ErrorExit;
        }

        /* close file and set to NULL */
        if(fclose(fptr) != 0)
        {
            BDBG_ERR(("%s - Error closing drm bin file '%s'.  (%s)", BSTD_FUNCTION, drm_bin_filename, strerror(errno)));
            rc = BERR_OS_ERROR;
            goto ErrorExit;
        }

        fptr = NULL;

        /* verify allocated drm_bin_file_buff size with size in header */
        filesize_from_header = SSD_P_CheckDrmBinFileSize();
        if(filesize_from_header != filesize)
        {
            BDBG_ERR(("%s - Error validating file size in header (%u != %u)", BSTD_FUNCTION, filesize_from_header, filesize));
            rc = BERR_OUT_OF_SYSTEM_MEMORY;
            goto ErrorExit;
        }

        BDBG_MSG(("%s - Error validating file size in header (%u ?=? %u)", BSTD_FUNCTION, filesize_from_header, filesize));

        /* All index '0' shared blocks will be reserved for drm bin file data */

        container->blocks[0].len = filesize_from_header;

        container->blocks[0].data.ptr = SRAI_Memory_Allocate(filesize_from_header, SRAI_MemoryType_Shared);
        BDBG_MSG(("%s - Allocating SHARED MEMORY of '%u' bytes for shared block[0] (address %p)", BSTD_FUNCTION, filesize_from_header, container->blocks[0].data.ptr));
        if (container->blocks[0].data.ptr == NULL)
        {
            BDBG_ERR(("%s - Error allocating SRAI memory", BSTD_FUNCTION));
            rc = BERR_OUT_OF_SYSTEM_MEMORY;
            goto ErrorExit;
        }
        BKNI_Memcpy(container->blocks[0].data.ptr, drm_bin_file_buff, filesize_from_header);

        BDBG_MSG(("%s - Copied '%u' bytes into SRAI container (address %p)", BSTD_FUNCTION, filesize_from_header, container->blocks[0].data.ptr));
    }

    /* All modules will call SRAI_Module_Init */
    BDBG_MSG(("%s - ************************* (platformHandle = %p)", BSTD_FUNCTION, platformHandle));
    if (NULL != indication_cb){
        init_settings.indicationCallback =  indication_cb;
        init_settings.indicationCallbackArg = 0;
    }else{
        init_settings.indicationCallback =  NULL;
    }
    sage_rc = SRAI_Module_Init_Ext(platformHandle, module_id, container, &tmpModuleHandle, &init_settings);
    if(sage_rc != BERR_SUCCESS)
    {
        BDBG_ERR(("%s - Error calling SRAI_Module_Init", BSTD_FUNCTION));
        rc = sage_rc;
        goto ErrorExit;
    }
    BDBG_MSG(("%s - SRAI_Module_Init(%p, %u, %p, %p) returned %p",
              BSTD_FUNCTION, (void *)platformHandle, module_id, (void *)container, (void *)&tmpModuleHandle, (void *)moduleHandle));

    /* Extract DRM bin file manager response from basic[0].  Free memory if failed */
    sage_rc = container->basicOut[0];
    if(sage_rc != BERR_SUCCESS)
    {
        BDBG_ERR(("%s - SAGE error processing DRM bin file", BSTD_FUNCTION));
        rc = sage_rc;
        goto ErrorExit;
    }

    /* Overwrite the drm bin file in rootfs, free up the buffer since a copy will exist on the sage side */
    if(drm_bin_filename != NULL)
    {
        if(container->basicOut[1] == OVERWRITE_BIN_FILE)
        {
            BDBG_MSG(("%s - Overwriting file '%s'", BSTD_FUNCTION, drm_bin_filename));

            if(fptr != NULL)
            {
                BDBG_ERR(("%s - File pointer already opened, invalid state.  '%s'", BSTD_FUNCTION, drm_bin_filename));
                rc = BERR_OS_ERROR;
                goto ErrorExit;
            }

            /* Overwrite drm bin file once bounded */
            fptr = fopen(drm_bin_filename, "w+b");
            if(fptr == NULL)
            {
                BDBG_ERR(("%s - Error opening DRM bin file (%s) in 'w+b' mode.  '%s'", BSTD_FUNCTION, drm_bin_filename, strerror(errno)));
                rc = BERR_OS_ERROR;
                goto ErrorExit;
            }

            write_size = fwrite(container->blocks[0].data.ptr, 1, filesize_from_header, fptr);
            if(write_size != filesize)
            {
                BDBG_ERR(("%s - Error writing drm bin file size to rootfs (%u != %u)", BSTD_FUNCTION, write_size, filesize));
                rc = BERR_OS_ERROR;
                goto ErrorExit;
            }
            fclose(fptr);
            fptr = NULL;
        }
        else{
            BDBG_MSG(("%s - No need to overwrite file '%s'", BSTD_FUNCTION, drm_bin_filename));
        }
    }

    /* success */
    *moduleHandle = tmpModuleHandle;

ErrorExit:

    if(container && container->blocks[0].data.ptr != NULL){
        SRAI_Memory_Free(container->blocks[0].data.ptr);
        container->blocks[0].data.ptr = NULL;
    }

    if(drm_bin_file_buff != NULL){
        SRAI_Memory_Free(drm_bin_file_buff);
        drm_bin_file_buff = NULL;
    }

    if(fptr != NULL){
        fclose(fptr);
        fptr = NULL;
    }

    if (rc != BERR_SUCCESS){
        SSD_ModuleUninit(tmpModuleHandle);
    }

    BDBG_LEAVE(SSD_ModuleInit);
    return rc;
}

BERR_Code
SSD_ModuleUninit(SRAI_ModuleHandle moduleHandle)
{
    BDBG_ENTER(SSD_ModuleUninit);

    if(moduleHandle != NULL){
        BDBG_MSG(("%s - SRAI_Module_Uninit(%p)", BSTD_FUNCTION, (void *)moduleHandle));
        SRAI_Module_Uninit(moduleHandle);
    }

    BDBG_MSG(("%s - Cleaning up SSD TL only parameters ***************************", BSTD_FUNCTION));
    if (platformHandle)
    {
        BDBG_MSG(("%s - SRAI_Platform_Close(%p)", BSTD_FUNCTION, (void *)platformHandle));
        SRAI_Platform_Close(platformHandle);
        platformHandle = NULL;
    }

    SRAI_Platform_UnInstall(BSAGE_PLATFORM_ID_SSD);

    BDBG_LEAVE(SSD_ModuleUninit);

    return BERR_SUCCESS;
}



uint32_t
SSD_P_CheckDrmBinFileSize(void)
{
    uint32_t tmp_file_size = 0;
    drm_bin_header_t *pHeader = (drm_bin_header_t *)drm_bin_file_buff;

    tmp_file_size = GET_UINT32_FROM_BUF(pHeader->bin_file_size);
    if(tmp_file_size > DEFAULT_DRM_BIN_FILESIZE)
    {
        if(drm_bin_file_buff != NULL){
            SRAI_Memory_Free(drm_bin_file_buff);
            drm_bin_file_buff = NULL;
        }
        drm_bin_file_buff = SRAI_Memory_Allocate(tmp_file_size, SRAI_MemoryType_Shared);

        BKNI_Memset(drm_bin_file_buff, 0x00, tmp_file_size);
    }

    BDBG_MSG(("%s - tmp_file_size=%u", BSTD_FUNCTION, tmp_file_size));

    return tmp_file_size;
}



BERR_Code SSD_P_GetFileSize(const char * filename, uint32_t *filesize)
{
    BERR_Code rc = BERR_SUCCESS;
    FILE *fptr = NULL;
    int pos = 0;

    fptr = fopen(filename, "rb");
    if(fptr == NULL)
    {
        BDBG_ERR(("%s - Error opening file '%s'.  (%s)", BSTD_FUNCTION, filename, strerror(errno)));
        rc = BERR_OS_ERROR;
        goto ErrorExit;
    }

    pos = fseek(fptr, 0, SEEK_END);
    if(pos == -1)
    {
        BDBG_ERR(("%s - Error seeking to end of file '%s'.  (%s)", BSTD_FUNCTION, filename, strerror(errno)));
        rc = BERR_OS_ERROR;
        goto ErrorExit;
    }

    pos = ftell(fptr);
    if(pos == -1)
    {
        BDBG_ERR(("%s - Error determining position of file pointer of file '%s'.  (%s)", BSTD_FUNCTION, filename, strerror(errno)));
        rc = BERR_OS_ERROR;
        goto ErrorExit;
    }

    /* check vs. max SAGE SRAM size */
    if(pos > DEFAULT_DRM_BIN_FILESIZE)
    {
        BDBG_ERR(("%s - Invalid file size detected for of file '%s'.  (%u)", BSTD_FUNCTION, filename, pos));
        rc = BERR_OS_ERROR;
        goto ErrorExit;
    }

    (*filesize) = pos;

ErrorExit:

    if(fptr != NULL)
    {
        /* error closing?!  weird error case not sure how to handle */
        if(fclose(fptr) != 0){
            BDBG_ERR(("%s - Error closing drm bin file '%s'.  (%s)", BSTD_FUNCTION, filename, strerror(errno)));
            rc = BERR_OS_ERROR;
        }
    }

    BDBG_MSG(("%s - Exiting function (%u bytes)", BSTD_FUNCTION, (*filesize)));

    return rc;
}

BERR_Code SSD_P_TA_Install(char * ta_bin_filename)
{
    BERR_Code rc = BERR_SUCCESS;
    FILE * fptr = NULL;
    uint32_t file_size = 0;
    uint32_t read_size = 0;
    uint8_t *ta_bin_file_buff = NULL;
    BERR_Code sage_rc = BERR_SUCCESS;
    const char *path = NEXUS_GetEnv("SAGEBIN_PATH");
    char ta_name[512];

    if (path == NULL) {
       sprintf(ta_name, "./%s", ta_bin_filename);
    } else {
       sprintf(ta_name, "%s/%s", path, ta_bin_filename);
    }

    BDBG_MSG(("%s - TA bin filename '%s'", BSTD_FUNCTION, ta_name));

    rc = SSD_P_GetFileSize(ta_name, &file_size);
    if(rc != BERR_SUCCESS)
    {
        BDBG_LOG(("%s - Error determine file size of TA bin file", BSTD_FUNCTION));
        goto ErrorExit;
    }

    ta_bin_file_buff = SRAI_Memory_Allocate(file_size, SRAI_MemoryType_Shared);
    if(ta_bin_file_buff == NULL)
    {
        BDBG_ERR(("%s - Error allocating '%u' bytes for loading TA bin file", BSTD_FUNCTION, file_size));
        rc = BERR_OUT_OF_SYSTEM_MEMORY;
        goto ErrorExit;
    }

    fptr = fopen(ta_name, "rb");
    if(fptr == NULL)
    {
        BDBG_ERR(("%s - Error opening TA bin file (%s)", BSTD_FUNCTION, ta_name));
        rc = BERR_OS_ERROR;
        goto ErrorExit;
    }

    read_size = fread(ta_bin_file_buff, 1, file_size, fptr);
    if(read_size != file_size)
    {
        BDBG_ERR(("%s - Error reading TA bin file size (%u != %u)", BSTD_FUNCTION, read_size, file_size));
        rc = BERR_OS_ERROR;
        goto ErrorExit;
    }

    /* close file and set to NULL */
    if(fclose(fptr) != 0)
    {
        BDBG_ERR(("%s - Error closing TA bin file '%s'.  (%s)", BSTD_FUNCTION, ta_name, strerror(errno)));
        rc = BERR_OS_ERROR;
        goto ErrorExit;
    }
    fptr = NULL;

    BDBG_MSG(("%s - TA 0x%x Install file %s", BSTD_FUNCTION,BSAGE_PLATFORM_ID_SSD, ta_name));

    sage_rc = SRAI_Platform_Install(BSAGE_PLATFORM_ID_SSD, ta_bin_file_buff, file_size);
    if(sage_rc != BERR_SUCCESS)
    {
        BDBG_ERR(("%s - Error calling SRAI_Platform_Install Error 0x%x", BSTD_FUNCTION, sage_rc ));
        rc = sage_rc;
        goto ErrorExit;
    }

ErrorExit:

    if(ta_bin_file_buff != NULL){
        SRAI_Memory_Free(ta_bin_file_buff);
        ta_bin_file_buff = NULL;
    }

    if(fptr != NULL){
        fclose(fptr);
        fptr = NULL;
    }

    return rc;
}

#if (NEXUS_SECURITY_API_VERSION == 1)
ChipType_e SSD_P_GetChipType()
{
    NEXUS_ReadMspParms     readMspParms;
    NEXUS_ReadMspIO        readMsp0;
    NEXUS_ReadMspIO        readMsp1;
    NEXUS_Error rc =  NEXUS_SUCCESS;

    readMspParms.readMspEnum = NEXUS_OtpCmdMsp_eReserved233;
    rc = NEXUS_Security_ReadMSP(&readMspParms,&readMsp0);

    readMspParms.readMspEnum = NEXUS_OtpCmdMsp_eReserved234;
    rc = NEXUS_Security_ReadMSP(&readMspParms,&readMsp1);

    BDBG_MSG(("OTP MSP0 %d %d %d %d OTP MSP0 %d %d %d %d",readMsp0.mspDataBuf[0], readMsp0.mspDataBuf[1], readMsp0.mspDataBuf[2], readMsp0.mspDataBuf[3],
                                                          readMsp1.mspDataBuf[0], readMsp1.mspDataBuf[1], readMsp1.mspDataBuf[2], readMsp1.mspDataBuf[3]));

    if((readMsp0.mspDataBuf[3] == OTP_MSP0_VALUE_ZS) && (readMsp1.mspDataBuf[3] == OTP_MSP1_VALUE_ZS)){
        return ChipType_eZS;
    }
    return ChipType_eZB;
}
#else
ChipType_e SSD_P_GetChipType()
{
   NEXUS_OtpMspRead readMsp0;
   NEXUS_OtpMspRead readMsp1;
   uint32_t Msp0Data;
   uint32_t Msp1Data;
   NEXUS_Error rc = NEXUS_SUCCESS;
#if NEXUS_ZEUS_VERSION < NEXUS_ZEUS_VERSION_CALC(5,0)
   rc = NEXUS_OtpMsp_Read(233, &readMsp0);
   if (rc) BERR_TRACE(rc);
   rc = NEXUS_OtpMsp_Read(234, &readMsp1);
   if (rc) BERR_TRACE(rc);
#else
   rc = NEXUS_OtpMsp_Read(224, &readMsp0);
   if (rc) BERR_TRACE(rc);
   rc = NEXUS_OtpMsp_Read(225, &readMsp1);
   if (rc) BERR_TRACE(rc);
#endif
   Msp0Data = readMsp0.data & readMsp0.valid;
   Msp1Data = readMsp1.data & readMsp1.valid;
   if((Msp0Data == OTP_MSP0_VALUE_ZS) && (Msp1Data == OTP_MSP1_VALUE_ZS)) {
      return ChipType_eZS;
   }
   return ChipType_eZB;
}
#endif
