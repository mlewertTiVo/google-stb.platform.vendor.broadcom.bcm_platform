/******************************************************************************
 *    (c)2011-2014 Broadcom Corporation
 * 
 * This program is the proprietary software of Broadcom Corporation and/or its licensors,
 * and may only be used, duplicated, modified or distributed pursuant to the terms and
 * conditions of a separate, written license agreement executed between you and Broadcom
 * (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 * no license (express or implied), right to use, or waiver of any kind with respect to the
 * Software, and Broadcom expressly reserves all rights in and to the Software and all
 * intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 * HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 * NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.  
 *  
 * Except as expressly set forth in the Authorized License,
 *  
 * 1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 * secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 * and to use this information only in connection with your use of Broadcom integrated circuit products.
 *  
 * 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS" 
 * AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR 
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO 
 * THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES 
 * OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, 
 * LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION 
 * OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF 
 * USE OR PERFORMANCE OF THE SOFTWARE.
 * 
 * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS 
 * LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR 
 * EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR 
 * USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT 
 * ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE 
 * LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF 
 * ANY LIMITED REMEDY.
 *
 * $brcm_Workfile: nexus_ipc_priv.h $
 * 
 * Module Description:
 * This header file defines the command that is used to encapsulate an API call
 * from the client in a parcel and is sent over binder to the server.
 * 
 *****************************************************************************/
#ifndef _NEXUS_IPC_PRIV_H_
#define _NEXUS_IPC_PRIV_H_

// api's supported over ipc (binder)
enum api_name{
    api_clientJoin,
    api_clientUninit,
    api_createClientContext,
    api_destroyClientContext,
    api_setPowerState,
    api_getPowerState,
    api_setCecPowerState,
    api_getCecPowerStatus,
    api_getCecStatus,
    api_sendCecMessage,
    api_setCecLogicalAddress,
    api_getHdmiOutputStatus,
};

// in and out parameters for each of these api's
typedef union api_param
{
    struct {
        struct {
            b_refsw_client_client_name         clientName;
            NEXUS_ClientAuthenticationSettings clientAuthenticationSettings;
        } in;

        struct {
            NEXUS_ClientHandle clientHandle;
        } out;
    } clientJoin;

    struct {
        struct {
            NEXUS_ClientHandle clientHandle;
        } in;

        struct {
            NEXUS_Error status;
        } out;
    } clientUninit;

    struct {
        struct {
            b_refsw_client_client_name clientName;
            unsigned                   clientPid;
        } in;

        struct {
            NexusClientContext *client;
        } out;
    } createClientContext;

    struct {
        struct {
            NexusClientContext *client;
        } in;

        struct {
        } out;
    } destroyClientContext;

    struct {
        struct {
            b_powerState pmState;
        } in;

        struct {
            bool status;
        } out;
    } setPowerState;

    struct {
        struct {
            b_powerState pmState;
        } out;
    } getPowerState;

    struct {
        struct {
            uint32_t     cecId;
            b_powerState pmState;
        } in;

        struct {
            bool status;
        } out;
    } setCecPowerState;

    struct {
        struct {
            uint32_t cecId;
        } in;

        struct {
            uint8_t powerStatus;
            bool status;
        } out;
    } getCecPowerStatus;

    struct {
        struct {
            uint32_t cecId;
        } in;

        struct {
            b_cecStatus cecStatus;
            bool status;
        } out;
    } getCecStatus;

    struct {
        struct {
            uint32_t cecId;
            uint8_t  srcAddr;
            uint8_t  destAddr;
            size_t   length;
            uint8_t  message[NEXUS_CEC_MESSAGE_DATA_SIZE];
        } in;

        struct {
            bool status;
        } out;
    } sendCecMessage;

    struct {
        struct {
            uint32_t cecId;
            uint8_t  addr;
        } in;

        struct {
            bool status;
        } out;
    } setCecLogicalAddress;

    struct {
        struct {
            uint32_t portId;
        } in;

        struct {
            b_hdmiOutputStatus hdmiOutputStatus;
            bool status;
        } out;
    } getHdmiOutputStatus;

} api_param;

// api data that is actually transferred over ipc (binder)
typedef struct api_data {
    api_name api;
    api_param param;
} api_data;

#endif
