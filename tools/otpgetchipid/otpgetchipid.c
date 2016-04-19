/******************************************************************************
* Broadcom Proprietary and Confidential. (c)2016 Broadcom. All rights reserved.
*
* This program is the proprietary software of Broadcom and/or its
* licensors, and may only be used, duplicated, modified or distributed pursuant
* to the terms and conditions of a separate, written license agreement executed
* between you and Broadcom (an "Authorized License").  Except as set forth in
* an Authorized License, Broadcom grants no license (express or implied), right
* to use, or waiver of any kind with respect to the Software, and Broadcom
* expressly reserves all rights in and to the Software and all intellectual
* property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
* HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
* NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
*
* Except as expressly set forth in the Authorized License,
*
* 1. This program, including its structure, sequence and organization,
*    constitutes the valuable trade secrets of Broadcom, and you shall use all
*    reasonable efforts to protect the confidentiality thereof, and to use
*    this information only in connection with your use of Broadcom integrated
*    circuit products.
*
* 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
*    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
*    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT
*    TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED
*    WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A
*    PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET
*    ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME
*    THE ENTIRE RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
*
* 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
*    LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT,
*    OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO
*    YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN
*    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS
*    OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. , WHICHEVER
*    IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF
*    ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
******************************************************************************/

/* Nexus example app: OTP/MSP Access */

#ifndef BSTD_CPU_ENDIAN
#define BSTD_CPU_ENDIAN BSTD_ENDIAN_LITTLE
#endif

#include "nexus_platform.h"
#include "nexus_display.h"
#include "nexus_message.h"
#include "nexus_memory.h"
#include "bstd.h"
#include "bkni.h"
#include "nexus_otpmsp.h"
#include "nexus_read_otp_id.h"
#ifdef NEXUS_HAS_PROGRAM_OTP_DATASECTION
 #include "crc/bhsm_crc.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nxclient.h"

BDBG_MODULE(OTPGETCHIPID);


#define DEBUG_PRINT_ARRAY(description_txt,in_size,in_ptr) { int x_offset;       \
            printf("[%s]", description_txt );                                   \
            for( x_offset = 0; x_offset < (int)(in_size); x_offset++ )          \
            {                                                                   \
                printf("%02X", in_ptr[x_offset] );                              \
            }                                                                   \
            printf("\n");                                                       \
}




int main(void)
{
    NEXUS_Error rc;
    NEXUS_SecurityCapabilities securityCapabilities;
    NEXUS_ReadMspParms readMspParms;
    NEXUS_ReadMspIO readMspIO;
    NEXUS_OtpIdOutput otpRead;
    NEXUS_PlatformStatus platStatus;
    uint32_t outValue233 = 0;
    uint32_t outValue234 = 0;
    uint8_t outValueSecureBootEnable = 0;
    unsigned i = 0;
    NxClient_JoinSettings joinSettings;

    NxClient_GetDefaultJoinSettings(&joinSettings);
    joinSettings.ignoreStandbyRequest = true;
    joinSettings.timeout = 60;
    rc = NxClient_Join(&joinSettings);
    if (rc) {
       fprintf( stderr, "\nfailed to join nexus.  aborting.\n" );
       goto BHSM_P_DONE_LABEL;
    }

    rc = NEXUS_Security_ReadOtpId( NEXUS_OtpIdType_eA, &otpRead );
    if( rc != NEXUS_SUCCESS )
    {
        fprintf( stderr, "\nNEXUS_Security_ReadOtpId() failed. Error code: %x\n", rc );
        goto BHSM_P_DONE_LABEL;
    }

    memset( &readMspParms, 0, sizeof(readMspParms) );

    readMspParms.readMspEnum = NEXUS_OtpCmdMsp_eReserved233;
    rc = NEXUS_Security_ReadMSP( &readMspParms, &readMspIO );
    if( rc != NEXUS_SUCCESS )
    {
        printf("NEXUS_Security_ReadMSP() errCode: %x\n", rc );
        goto BHSM_P_DONE_LABEL;
    }
    for( i = 0; i < readMspIO.mspDataSize; i++ )
    {
        outValue233 = (outValue233 << 8) | (readMspIO.mspDataBuf[i] & readMspIO.lockMspDataBuf[i] );
    }

    DEBUG_PRINT_ARRAY( "MSP Data 233", readMspIO.mspDataSize, readMspIO.mspDataBuf );
    DEBUG_PRINT_ARRAY( "MSP Mask 233", readMspIO.mspDataSize, readMspIO.lockMspDataBuf );


    readMspParms.readMspEnum = NEXUS_OtpCmdMsp_eReserved234;
    rc = NEXUS_Security_ReadMSP( &readMspParms, &readMspIO );
    if( rc != NEXUS_SUCCESS )
    {
        printf("NEXUS_Security_ReadMSP() errCode: %x\n", rc );
        goto BHSM_P_DONE_LABEL;
    }
    for( i = 0; i < readMspIO.mspDataSize; i++ )
    {
        outValue234 = (outValue234 << 8) | (readMspIO.mspDataBuf[i] & readMspIO.lockMspDataBuf[i] );
    }

    DEBUG_PRINT_ARRAY( "MSP Data 234", readMspIO.mspDataSize, readMspIO.mspDataBuf );
    DEBUG_PRINT_ARRAY( "MSP Mask 234", readMspIO.mspDataSize, readMspIO.lockMspDataBuf );

    readMspParms.readMspEnum = NEXUS_OtpCmdMsp_eSecureBootEnable;
    rc = NEXUS_Security_ReadMSP( &readMspParms, &readMspIO );
    if( rc != NEXUS_SUCCESS )
    {
        printf("NEXUS_Security_ReadMSP() errCode: %x\n", rc );
        goto BHSM_P_DONE_LABEL;
    }
    outValueSecureBootEnable = readMspIO.mspDataBuf[3];

    NEXUS_GetSecurityCapabilities( &securityCapabilities );
    NEXUS_Platform_GetStatus( &platStatus );
    printf("Chip Family........[%X]\n", platStatus.familyId );
    printf("Chip Version.......[%X]\n", platStatus.chipRevision );
    printf("Chip Id............[%X]\n", platStatus.chipId );
    printf("Zeus Version.......[%d.%d]\n", NEXUS_ZEUS_VERSION_MAJOR( securityCapabilities.version.zeus )
                                       , NEXUS_ZEUS_VERSION_MINOR( securityCapabilities.version.zeus ) );
    printf("Firmware Version...[%d.%d]\n", ( securityCapabilities.version.firmware >> 16)
                                       , ( securityCapabilities.version.firmware &  0x0000FFFF) );
    printf("OTP signature 233..[0x%X]\n", outValue233 );
    printf("OTP signature 234..[0x%X]\n", outValue234 );
    printf("OTP secure boot....[0x%X]\n", outValueSecureBootEnable );
    DEBUG_PRINT_ARRAY( "The OTP ID for A", otpRead.size, otpRead.otpId );

BHSM_P_DONE_LABEL:

    NxClient_Uninit();
    return rc;
}
