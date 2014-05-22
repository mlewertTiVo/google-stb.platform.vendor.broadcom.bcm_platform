/******************************************************************************
 *    (c)2010-2013 Broadcom Corporation
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
 *****************************************************************************/
  
#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "NexusCecService"
//#define LOG_NDEBUG 0

#include "nexuscecservice.h"

/******************************************************************************
  CecRxMessageHandler table of opcode and commands
******************************************************************************/
const struct NexusService::CecServiceManager::CecRxMessageHandler::opCodeCommandList NexusService::CecServiceManager::CecRxMessageHandler::opCodeList[] =
{
    { NEXUS_CEC_OpGivePhysicalAddress, NEXUS_CEC_OpReportPhysicalAddress,
          { &NexusService::CecServiceManager::CecRxMessageHandler::getPhysicalAddress,
            &NexusService::CecServiceManager::CecRxMessageHandler::getDeviceType,
            NULL } },

    { NEXUS_CEC_OpGiveOSDName, NEXUS_CEC_OpSetOSDName,
          { &NexusService::CecServiceManager::CecRxMessageHandler::getOsdName,
            NULL,
            NULL } },

    { NEXUS_CEC_OpGiveDeviceVendorID, NEXUS_CEC_OpDeviceVendorID,
          { &NexusService::CecServiceManager::CecRxMessageHandler::getDeviceVendorID,
            NULL,
            NULL } },

    { NEXUS_CEC_OpRequestActiveSource, NEXUS_CEC_OpActiveSource,
          { &NexusService::CecServiceManager::CecRxMessageHandler::getPhysicalAddress,
            NULL,
            NULL } },

    { NEXUS_CEC_OpSetStreamPath, NEXUS_CEC_OpActiveSource,
          { &NexusService::CecServiceManager::CecRxMessageHandler::getPhysicalAddress,
            NULL,
            NULL } },

    { NEXUS_CEC_OpStandby, 0,
          { &NexusService::CecServiceManager::CecRxMessageHandler::enterStandby,
            NULL,
            NULL } },

    { NEXUS_CEC_OpGetCECVersion, NEXUS_CEC_OpCECVersion,
          { &NexusService::CecServiceManager::CecRxMessageHandler::getCecVersion,
            NULL,
            NULL } },

    { NEXUS_CEC_OpReportPowerStatus, 0,
          { &NexusService::CecServiceManager::CecRxMessageHandler::reportPowerStatus,
            NULL,
            NULL } },

    { 0, 0, {NULL, NULL, NULL} }
};

/******************************************************************************
  CecRxMessageHandler methods
******************************************************************************/
void NexusService::CecServiceManager::CecRxMessageHandler::getPhysicalAddress(unsigned inLength, uint8_t *content, unsigned *outLength )
{
    NEXUS_CecStatus status;

    NEXUS_Cec_GetStatus( mCecServiceManager->cecHandle, &status );

    content[0] = status.physicalAddress[0];
    content[1] = status.physicalAddress[1];
    *outLength = 2;
}

void NexusService::CecServiceManager::CecRxMessageHandler::getDeviceType(unsigned inLength, uint8_t *content, unsigned *outLength )
{
    NEXUS_CecStatus status;

    NEXUS_Cec_GetStatus( mCecServiceManager->cecHandle, &status );

    switch (status.deviceType)
    {
        case NEXUS_CecDeviceType_eTv: content[0] = 0; break;
        case NEXUS_CecDeviceType_eRecordingDevice: content[0] = 1; break;
        case NEXUS_CecDeviceType_eReserved: content[0] = 2; break;
        case NEXUS_CecDeviceType_eTuner: content[0] = 3; break;
        case NEXUS_CecDeviceType_ePlaybackDevice: content[0] = 4; break;
        case NEXUS_CecDeviceType_eAudioSystem: content[0] = 5; break;
        case NEXUS_CecDeviceType_ePureCecSwitch: content[0] = 6; break;
        case NEXUS_CecDeviceType_eVideoProcessor: content[0] = 7; break;
        default: BDBG_WRN(("Unknown Device Type!!!")); break;
    }
    *outLength = 1;
}

void NexusService::CecServiceManager::CecRxMessageHandler::getCecVersion(unsigned inLength, uint8_t *content, unsigned *outLength )
{
    NEXUS_CecStatus status;

    NEXUS_Cec_GetStatus( mCecServiceManager->cecHandle, &status );

    BDBG_WRN(("CEC Version Requested -> Version: %#x",status.cecVersion));
    content[0] = status.cecVersion;
    *outLength = 1;
}

void NexusService::CecServiceManager::CecRxMessageHandler::getOsdName (unsigned inLength, uint8_t *content, unsigned *outLength )
{
#define OSDNAME_SIZE 7 /* Size no larger than 15 */
    char osdName[]= "BRCMSTB";
    unsigned j;

    for( j = 0; j < OSDNAME_SIZE ; j++ ) {
        content[j] = (int)osdName[j];
    }
    *outLength = OSDNAME_SIZE;
}

void NexusService::CecServiceManager::CecRxMessageHandler::getDeviceVendorID(unsigned inLength, uint8_t *content, unsigned *outLength )
{
#define VendorID_SIZE 4 /* Size no larger than 15 */
    char vendorID[]= "BRCM";
    unsigned j;

    for( j = 0; j < VendorID_SIZE ; j++ ) {
        content[j] = (int)vendorID[j];
    }
    *outLength = VendorID_SIZE;
}

void NexusService::CecServiceManager::CecRxMessageHandler::enterStandby (unsigned inLength, uint8_t *content, unsigned *outLength )
{
    LOGD("%s: TV STANDBY RECEIVED: STB WILL NOW ENTER S3 STANDBY...", __FUNCTION__);
    
    system("/system/bin/input keyevent POWER");
}

void NexusService::CecServiceManager::CecRxMessageHandler::reportPowerStatus (unsigned inLength, uint8_t *content, unsigned *outLength )
{
    if (inLength > 1) {
        // Signal reception of power status to other threads...
        mCecServiceManager->mCecGetPowerStatusLock.lock();
        mCecServiceManager->cecPowerStatus = *content;
        mCecServiceManager->mCecGetPowerStatusCondition.broadcast();
        mCecServiceManager->mCecGetPowerStatusLock.unlock();
    }
    else {
        LOGE("%s: Could not obtain power status from <Report Power Status> CEC%d message!", __FUNCTION__, cecId);
    }
    *outLength = 0;
}

void NexusService::CecServiceManager::CecRxMessageHandler::responseLookUp( const uint8_t opCode, size_t inLength, uint8_t *pBuffer )
{
    int i, index = 0;
    unsigned j, outLength = 0, param_index = 0;
    uint8_t destAddr = 0;
    unsigned responseLength;
    uint8_t responseBuffer[NEXUS_CEC_MESSAGE_DATA_SIZE];
    uint8_t tmp[NEXUS_CEC_MESSAGE_DATA_SIZE];

    for( i=0; opCodeList[i].opCodeCommand; i++ ) {
        if (opCode == opCodeList[i].opCodeCommand) {
            void (NexusService::CecServiceManager::CecRxMessageHandler::*pFunc)(unsigned inLength, uint8_t *content, unsigned *outLength);

            LOGV("%s: Found support for opcode:0x%02X", __FUNCTION__, opCode);

            /* Assign Designated Response Op Code */
            responseBuffer[0] = opCodeList[i].opCodeResponse;
            responseLength = 1;

            /* If require Parameters, then add to message. Skip getParam function if NULL */
            while ( param_index < numMaxParameters ) {
                pFunc = NexusService::CecServiceManager::CecRxMessageHandler::opCodeList[i].getParamFunc[param_index];
                if (pFunc == NULL) {
                    break;
                }

                // Provide the received parameters to the getParamFunc for parsing if need be...
                if (inLength > 1) {
                    memcpy(tmp, pBuffer, inLength-1);
                }

                (this->*pFunc)(inLength, tmp, &outLength);
                LOGV("%s: Getting parameter %d, length: %d", __FUNCTION__, param_index, outLength);

                if ( responseLength+outLength > NEXUS_CEC_MESSAGE_DATA_SIZE) {
                    LOGW("%s: This parameter has reached over size limit! Last Parameter Length: %d", __FUNCTION__, outLength);
                    return;
                }
                responseLength += outLength;
                for( j = 0 ; j < outLength ; j++ ) {
                    responseBuffer[++index] = tmp[j];
                    LOGV("%s: Message Buffer[%d]: 0x%02X", __FUNCTION__, index, tmp[j]);
                }
                param_index++;
            }

            /* Only response if a response CEC message is required */
            if (opCodeList[i].opCodeResponse) {

                LOGV("%s: Transmitting CEC%d response:0x%02X...", __FUNCTION__, cecId, responseBuffer[0]);

                if (mCecServiceManager->sendCecMessage(destAddr, responseLength, responseBuffer) == OK) {
                    LOGV("%s: Successfully transmitted CEC%d response: 0x%02X", __FUNCTION__, cecId, responseBuffer[0]);
                }
                else {
                    LOGE("%s: ERROR transmitting CEC%d response: 0x%02X!!!", __FUNCTION__, cecId, responseBuffer[0]);
                }
            }
        }
    }
}

void NexusService::CecServiceManager::CecRxMessageHandler::parseCecMessage(const sp<AMessage> &msg)
{
    int32_t opcode;
    size_t  length;
    uint8_t *pBuffer = NULL;
    sp<ABuffer> params;
    char msgBuffer[3*(NEXUS_CEC_MESSAGE_DATA_SIZE +1)];
    unsigned i, j;

    msgBuffer[0] = '\0';

    msg->findInt32("opcode",  &opcode);
    msg->findSize("length", &length);

    if (length > 1) {
        msg->findBuffer("params", &params);

        if (params != NULL) {
            pBuffer = params->base();

            for (i = 0, j = 0; i < length-1 && j<(sizeof(msgBuffer)-1); i++) {
                j += BKNI_Snprintf(msgBuffer + j, sizeof(msgBuffer)-j, "%02X ", *pBuffer++);
            }           

            pBuffer = params->base();
        }
    }

    LOGD("%s: CEC%d message opcode 0x%02X length %d params \'%s\' received in thread %d", __FUNCTION__, cecId, opcode, length, msgBuffer, gettid());

    responseLookUp(opcode, length, pBuffer);
}

void NexusService::CecServiceManager::CecRxMessageHandler::onMessageReceived(const sp<AMessage> &msg)
{
    switch (msg->what())
    {
        case kWhatParse:
            LOGV("%s: Parsing CEC message...", __FUNCTION__);
            parseCecMessage(msg);
            break;
        default:
            LOGE("%s: Invalid message received - ignoring!", __FUNCTION__);
    }
}

NexusService::CecServiceManager::CecRxMessageHandler::~CecRxMessageHandler()
{
    LOGV("%s: for CEC%d called", __PRETTY_FUNCTION__, cecId); 
}

/******************************************************************************
  CecTxMessageHandler methods
******************************************************************************/
status_t NexusService::CecServiceManager::CecTxMessageHandler::outputCecMessage(const sp<AMessage> &msg)
{
    int32_t destaddr;
    int32_t opcode;
    size_t  length;
    uint8_t *pBuffer;
    sp<ABuffer> params;
    char msgBuffer[3*(NEXUS_CEC_MESSAGE_DATA_SIZE +1)];
    NEXUS_CecMessage transmitMessage;
    unsigned i, j;

    if (!msg->findInt32("destaddr", &destaddr)) {
        LOGE("%s: Could not find \"destaddr\" in CEC%d message!", __FUNCTION__, cecId);
        return FAILED_TRANSACTION;
    }
    if (!msg->findInt32("opcode", &opcode)) {
        LOGE("%s: Could not find \"opcode\" in CEC%d message!", __FUNCTION__, cecId);
        return FAILED_TRANSACTION;
    }
    if (!msg->findSize("length", &length)) {
        LOGE("%s: Could not find \"length\" in CEC%d message!", __FUNCTION__, cecId);
        return FAILED_TRANSACTION;
    }

    if (length > 0) {
        unsigned loops;
        NEXUS_CecStatus status;

        if (mCecServiceManager->cecHandle == NULL) {
            LOGE("%s: ERROR: cecHandle is NULL!!!", __PRETTY_FUNCTION__);
            return DEAD_OBJECT;
        }

        LOGV("%s: Getting CEC%d status...", __PRETTY_FUNCTION__, cecId);

        NEXUS_Cec_GetStatus(mCecServiceManager->cecHandle, &status);

        NEXUS_Cec_GetDefaultMessageData(&transmitMessage);
        transmitMessage.initiatorAddr = status.logicalAddress;
        transmitMessage.destinationAddr = destaddr;
        transmitMessage.length = length;
        transmitMessage.buffer[0] = opcode;

        msgBuffer[0] = '\0';

        if (length > 1) {
            if (!msg->findBuffer("params", &params)) {
                LOGE("%s: Could not find \"params\" in CEC%d message!", __FUNCTION__, cecId);
                return FAILED_TRANSACTION;
            }

            if (params != NULL) {
                pBuffer = params->base();

                for (i = 0, j = 0; i < length-1 && j<(sizeof(msgBuffer)-1); i++) {
                    transmitMessage.buffer[i+1] = *pBuffer;
                    j += BKNI_Snprintf(msgBuffer + j, sizeof(msgBuffer)-j, "%02X ", *pBuffer++);
                }           
            }
        }

        for (loops = 0; loops < 5; loops++) {
            NEXUS_Error rc;

            LOGD("%s: Outputting CEC%d message: dest addr=0x%02X opcode=0x%02X length=%d params=\'%s\' [thread %d]", __FUNCTION__,
                  cecId, destaddr, opcode, length, msgBuffer, gettid());

            rc = NEXUS_Cec_TransmitMessage(mCecServiceManager->cecHandle, &transmitMessage);
            if (rc == NEXUS_SUCCESS) {
                LOGV("%s: Successfully sent CEC%d message: opcode=0x%02X.", __FUNCTION__, cecId, opcode);
                // Always wait 35ms for (start + header block) and (25ms x length) + 25ms between transmitting messages...
                usleep((60 + 25*length) * 1000);
                break;
            }
            else {
                LOGE("%s: ERROR sending CEC%d message: opcode=0x%02X [rc=%d]%s", __FUNCTION__, cecId, opcode, rc, loops<4 ? " - retrying..." : "!!!");
                usleep(500 * 1000);
            }
        }
        return (loops < 5) ? OK : UNKNOWN_ERROR;
    }
    else {
        LOGE("%s: ERROR cannot output message of length 0!!!", __FUNCTION__);
        return BAD_VALUE;
    }
}

void NexusService::CecServiceManager::CecTxMessageHandler::onMessageReceived(const sp<AMessage> &msg)
{
    switch (msg->what())
    {
        case kWhatSend:
        {
            status_t err;
            uint32_t replyID;

            LOGV("%s: Sending CEC message...", __FUNCTION__);
            err = outputCecMessage(msg);

            if (!msg->senderAwaitsResponse(&replyID)) {
                LOGE("%s: ERROR awaiting response!", __FUNCTION__);
            }
            else {
                sp<AMessage> response = new AMessage;
                response->setInt32("err", err);
                response->postReply(replyID);
            }
            break;
        }
        default:
            LOGE("%s: Invalid message received - ignoring!", __FUNCTION__);
    }
}

NexusService::CecServiceManager::CecTxMessageHandler::~CecTxMessageHandler()
{
    LOGV("%s: for CEC%d called", __PRETTY_FUNCTION__, cecId); 
}

/******************************************************************************
  CecServiceManager methods
******************************************************************************/
void NexusService::CecServiceManager::deviceReady_callback(void *context, int param)
{
    NEXUS_CecStatus status;
    NexusService::CecServiceManager *pCecServiceManager = reinterpret_cast<NexusService::CecServiceManager *>(context);

    if (pCecServiceManager->cecHandle) {
        NEXUS_Cec_GetStatus(pCecServiceManager->cecHandle, &status);

        LOGV("%s: CEC Device %d %sReady, Xmit %sPending", __FUNCTION__, param, 
                status.ready ? "" : "Not ",
                status.messageTransmitPending ? "" : "Not "
                );
        LOGV("%s: Logical Address <%d> Acquired", __FUNCTION__, status.logicalAddress);
        LOGV("%s: Physical Address: %X.%X.%X.%X", __FUNCTION__,
            (status.physicalAddress[0] & 0xF0) >> 4, 
            (status.physicalAddress[0] & 0x0F),
            (status.physicalAddress[1] & 0xF0) >> 4, 
            (status.physicalAddress[1] & 0x0F));

        if ((status.physicalAddress[0] != 0xFF) && (status.physicalAddress[1] != 0xFF)) {
            pCecServiceManager->mCecDeviceReadyLock.lock();
            pCecServiceManager->mCecDeviceReadyCondition.broadcast();
            pCecServiceManager->mCecDeviceReadyLock.unlock();
        }
    }
}

void NexusService::CecServiceManager::msgReceived_callback(void *context, int param)
{
    NEXUS_CecStatus status;
    NexusService::CecServiceManager *pCecServiceManager = reinterpret_cast<NexusService::CecServiceManager *>(context);
    NEXUS_CecReceivedMessage receivedMessage;

    if (pCecServiceManager->cecHandle) {
        NEXUS_Cec_GetStatus(pCecServiceManager->cecHandle, &status);

        LOGV("%s: Device %sReady, Xmit %sPending", __FUNCTION__,
                status.ready ? "" : "Not ",
                status.messageTransmitPending ? "" : "Not "
                );

        if (status.messageReceived) {
            LOGV("%s: Msg Recd Status from Phys/Logical Addrs: %X.%X.%X.%X / %d", __FUNCTION__,
                (status.physicalAddress[0] & 0xF0) >> 4, (status.physicalAddress[0] & 0x0F),
                (status.physicalAddress[1] & 0xF0) >> 4, (status.physicalAddress[1] & 0x0F),
                status.logicalAddress) ;

            NEXUS_Cec_ReceiveMessage(pCecServiceManager->cecHandle, &receivedMessage);

            LOGV("%s: Cec%d message received length %d.", __FUNCTION__, param, receivedMessage.data.length);

            if (pCecServiceManager->isPlatformInitialised() && receivedMessage.data.length > 0) {
                sp<AMessage> msg = new AMessage;
                sp<ABuffer> buf = new ABuffer(receivedMessage.data.length);

                msg->setInt32("opcode",  receivedMessage.data.buffer[0]);
                msg->setSize("length",  receivedMessage.data.length);

                if (receivedMessage.data.length > 1) {
                    buf->setRange(0, 0);
                    memcpy(buf->data(), &receivedMessage.data.buffer[1], receivedMessage.data.length-1);
                    buf->setRange(0, receivedMessage.data.length-1);
                    msg->setBuffer("params", buf);
                }
                msg->setWhat(CecRxMessageHandler::kWhatParse);
                msg->setTarget(pCecServiceManager->mCecRxMessageHandler->id());
                msg->post();
            }
        }
    }
}

void NexusService::CecServiceManager::msgTransmitted_callback(void *context, int param)
{
    NEXUS_CecStatus status;
    NexusService::CecServiceManager *pCecServiceManager = reinterpret_cast<NexusService::CecServiceManager *>(context);

    if (pCecServiceManager->cecHandle) {
        NEXUS_Cec_GetStatus(pCecServiceManager->cecHandle, &status);
    
        LOGV("%s: Device %sReady, Xmit %sPending", __FUNCTION__,
                status.ready ? "" : "Not ",
                status.messageTransmitPending ? "" : "Not "
                );

        LOGV("%s: Msg Xmit Status for Phys/Logical Addrs: %X.%X.%X.%X / %d", __FUNCTION__, 
            (status.physicalAddress[0] & 0xF0) >> 4, (status.physicalAddress[0] & 0x0F),
            (status.physicalAddress[1] & 0xF0) >> 4, (status.physicalAddress[1] & 0x0F),
             status.logicalAddress);

        LOGV("%s: Xmit Msg Acknowledged: %s", __FUNCTION__, 
            status.transmitMessageAcknowledged ? "Yes" : "No") ;
        LOGV("%s: Xmit Msg Pending: %s", __FUNCTION__, 
            status.messageTransmitPending ? "Yes" : "No") ;
    }
}

bool NexusService::CecServiceManager::getHdmiStatus(NEXUS_HdmiOutputStatus *pStatus)
{
    bool success = false;
#if NEXUS_HAS_HDMI_OUTPUT
    NEXUS_Error rc = NEXUS_NOT_SUPPORTED;
    NEXUS_HdmiOutputStatus status;
    NEXUS_PlatformConfiguration platformConfig;
    NEXUS_HdmiOutputHandle hdmiOutput;
    unsigned loops;

    NEXUS_Platform_GetConfiguration(&platformConfig);
    hdmiOutput = platformConfig.outputs.hdmi[cecId];

    if (hdmiOutput != NULL) {
        for (loops = 0; loops < 4; loops++) {
            LOGV("%s: Waiting for HDMI output %d to be connected...", __FUNCTION__, cecId);
            rc = NEXUS_HdmiOutput_GetStatus(hdmiOutput, &status);
            if ((rc == NEXUS_SUCCESS) && status.connected) {
                break;
            }
            usleep(250 * 1000);
        }
    }

    if (rc == NEXUS_SUCCESS && status.connected) {
        LOGV("%s: HDMI output %d is connected.", __FUNCTION__, cecId);
        success = true;
        *pStatus = status;
    }
    else {
        LOGW("%s: HDMI output %d not connected.", __FUNCTION__, cecId);
    }
#endif
    return success;
}

status_t NexusService::CecServiceManager::sendCecMessage(uint8_t destAddr, size_t length, uint8_t *pBuffer)
{
    status_t err = BAD_VALUE;

    if (length > 0) {
        sp<AMessage> msg = new AMessage;
        sp<ABuffer> buf = new ABuffer(length);

        msg->setInt32("destaddr", destAddr);
        msg->setInt32("opcode", *pBuffer);
        msg->setSize("length",  length);

        if (length > 1) {
            buf->setRange(0, 0);
            memcpy(buf->data(), &pBuffer[1], length-1);
            buf->setRange(0, length-1);
            msg->setBuffer("params", buf);
        }
        msg->setWhat(CecTxMessageHandler::kWhatSend);
        msg->setTarget(mCecTxMessageHandler->id());

        ALOGV("%s: Posting message to looper...", __PRETTY_FUNCTION__);
        sp<AMessage> response;
        err = msg->postAndAwaitResponse(&response);

        if (err != OK) {
            LOGE("%s: ERROR posting message (err=%d)!!!", __FUNCTION__, err);
            return err;
        }
        else {
            ALOGV("%s: Returned from posting message to looper.", __PRETTY_FUNCTION__);
        }

        if (!response->findInt32("err", &err)) {
            err = OK;
        }
    }
    else {
        LOGE("%s: Invalid message length of 0!", __FUNCTION__);
    }
    return err;
}

bool NexusService::CecServiceManager::setPowerState(b_powerState pmState)
{
    bool success = false;
    uint8_t destAddr;
    size_t length;
    uint8_t buffer[NEXUS_CEC_MESSAGE_DATA_SIZE];

    if (ePowerState_S0 == pmState)
    {
        destAddr = 0;
        length = 1;
        buffer[0] = NEXUS_CEC_OpImageViewOn;

        LOGV("%s: Sending <Image View On> message to turn TV connected to HDMI output %d on...", __FUNCTION__, cecId);
        if (sendCecMessage(destAddr, length, buffer) == OK) {
            LOGD("%s: Successfully sent <Image View On> CEC%d message.", __FUNCTION__, cecId);
            success = true;
        }
        else {
            LOGE("%s: ERROR sending <Image View On> CEC%d message!!!", __FUNCTION__, cecId);
        }

        if (success) {
            NEXUS_CecStatus status;

            NEXUS_Cec_GetStatus(cecHandle, &status);

            LOGV("%s: Broadcasting <Active Source> message to ensure TV is displaying our content from HDMI output %d...", __FUNCTION__, cecId);
            destAddr = 0xF;    // Broadcast address
            length = 3;
            buffer[0] = NEXUS_CEC_OpActiveSource;
            buffer[1] = status.physicalAddress[0];
            buffer[2] = status.physicalAddress[1];

            if (sendCecMessage(destAddr, length, buffer) == OK) {
                LOGD("%s: Successfully sent <Active Source> CEC%d message.", __FUNCTION__, cecId);
                success = true;
            }
            else {
                LOGE("%s: ERROR sending <Image View On> CEC%d message!!!", __FUNCTION__, cecId);
            }
        }
    }
    else {
        destAddr = 0;
        length = 1;
        buffer[0] = NEXUS_CEC_OpStandby;

        LOGV("%s: Sending <Standby> message to place TV connected to HDMI output %d in standby...", __FUNCTION__, cecId);
        if (sendCecMessage(destAddr, length, buffer) == OK) {
            LOGD("%s: Successfully sent <Standby> Cec%d message.", __FUNCTION__, cecId);
            success = true;
        }
        else {
            LOGE("%s: ERROR sending <Standby> Cec%d message!!!", __FUNCTION__, cecId);
        }
    }
    return success;
}

bool NexusService::CecServiceManager::getPowerStatus(uint8_t *pPowerStatus)
{
    bool success = false;
    uint8_t destAddr;
    size_t length;
    uint8_t buffer[NEXUS_CEC_MESSAGE_DATA_SIZE];

    destAddr = 0;
    length = 1;
    buffer[0] = NEXUS_CEC_OpGiveDevicePowerStatus;

    LOGV("%s: Sending <Give Device Power Status> message to HDMI output %d on...", __FUNCTION__, cecId);
    if (sendCecMessage(destAddr, length, buffer) == OK) {
        status_t status;

        LOGD("%s: Successfully sent <Give Device Power Status> Cec%d message.", __FUNCTION__, cecId);
            
        /* Now wait up to 1s for the <Report Power Status> receive message */
        mCecGetPowerStatusLock.lock();
        status = mCecGetPowerStatusCondition.waitRelative(mCecGetPowerStatusLock, 1 * 1000 * 1000 * 1000);  // Wait up to 1s
        mCecGetPowerStatusLock.unlock();

        if (status == OK) {
            if (cecPowerStatus < 4) {
                *pPowerStatus = cecPowerStatus;
                success = true;
            }
            else {
                LOGE("%s: Invalid <Power Status> parameter 0x%0x!", __FUNCTION__, cecPowerStatus);
            }
        }
        else {
            LOGE("%s: Timed out waiting for <Report Power Status> CEC%d message!!!", __FUNCTION__, cecId);
        }
    }
    else {
        LOGE("%s: Could not transmit <Give Device Power Status> on Cec%d!", __FUNCTION__, cecId);
    }
    return success;
}

status_t NexusService::CecServiceManager::platformInit()
{
    status_t status = OK;
    NEXUS_Error rc;
    NEXUS_PlatformConfiguration platformConfig;
    NEXUS_HdmiOutputStatus hdmiOutputStatus;
    NEXUS_CecSettings cecSettings;
    NEXUS_CecStatus cecStatus;
    unsigned loops;

    NEXUS_Platform_GetConfiguration(&platformConfig);
    cecHandle  = platformConfig.outputs.cec[cecId];

    if (cecHandle == NULL) {
        ALOGV("%s: CEC%d not opened at platform init time - opening now...", __FUNCTION__, cecId);
        NEXUS_Cec_GetDefaultSettings(&cecSettings);
        cecHandle = NEXUS_Cec_Open(cecId, &cecSettings);
        if (cecHandle == NULL) {
            ALOGE("%s: Could not open Cec%d!!!", __FUNCTION__, cecId);
            status = NO_INIT;
        }
    }

    if (status == OK && getHdmiStatus(&hdmiOutputStatus)) {
        NEXUS_Cec_GetSettings(cecHandle, &cecSettings);
            cecSettings.enabled = false;
            cecSettings.messageReceivedCallback.callback = msgReceived_callback;
            cecSettings.messageReceivedCallback.context = this;
            cecSettings.messageReceivedCallback.param = cecId;

            cecSettings.messageTransmittedCallback.callback = msgTransmitted_callback;
            cecSettings.messageTransmittedCallback.context  = this;
            cecSettings.messageTransmittedCallback.param    = cecId;

            cecSettings.logicalAddressAcquiredCallback.callback = deviceReady_callback;
            cecSettings.logicalAddressAcquiredCallback.context = this;
            cecSettings.logicalAddressAcquiredCallback.param = cecId;

            cecSettings.physicalAddress[0]= (hdmiOutputStatus.physicalAddressA << 4) 
                | hdmiOutputStatus.physicalAddressB;
            cecSettings.physicalAddress[1]= (hdmiOutputStatus.physicalAddressC << 4) 
                | hdmiOutputStatus.physicalAddressD;
        
        rc = NEXUS_Cec_SetSettings(cecHandle, &cecSettings);

        if (rc != NEXUS_SUCCESS) {
            LOGE("%s: ERROR setting up Cec%d settings [rc=%d]!!!", __FUNCTION__, cecId, rc);
            status = UNKNOWN_ERROR;
        }
        else {
            /* Enable Cec core */
            NEXUS_Cec_GetSettings(cecHandle, &cecSettings);
            cecSettings.enabled = true;
            rc = NEXUS_Cec_SetSettings(cecHandle, &cecSettings);
            if (rc != NEXUS_SUCCESS) {
                LOGE("%s: ERROR enabling Cec%d [rc=%d]!!!", __FUNCTION__, cecId, rc);
                status = UNKNOWN_ERROR;
            }
            else {
                for (loops = 0; loops < 5; loops++) {
                    LOGV("%s: Waiting for Cec%d logical address...", __FUNCTION__, cecId);
                    mCecDeviceReadyLock.lock();
                    status = mCecDeviceReadyCondition.waitRelative(mCecDeviceReadyLock, 1 * 1000 * 1000 * 1000);  // Wait up to 1s
                    mCecDeviceReadyLock.unlock();
                    if (status == OK) {
                        break;
                    }
                }

                NEXUS_Cec_GetStatus(cecHandle, &cecStatus);

                LOGV("%s: Device %sReady, Xmit %sPending", __FUNCTION__,
                        cecStatus.ready ? "" : "Not ",
                        cecStatus.messageTransmitPending ? "" : "Not "
                        );

                if (cecStatus.logicalAddress == 0xFF) {
                    LOGE("%s: No Cec capable device found on HDMI output %d!", __FUNCTION__, cecId);
                    status = NO_INIT;
                }
                else {
                    char looperName[] = "Cec   x Message Looper";
                    looperName[3] = '0' + cecId;

                    LOGD("%s: CEC capable device found on HDMI output %d.", __FUNCTION__, cecId);

                    // Create looper to receive CEC messages...
                    looperName[5] = 'R';
                    mCecRxMessageLooper = new ALooper;
                    mCecRxMessageLooper->setName(looperName);
                    mCecRxMessageLooper->start();

                    // Create Handler to handle reception of CEC messages...
                    mCecRxMessageHandler = NexusService::CecServiceManager::CecRxMessageHandler::instantiate(this, cecId);
                    mCecRxMessageLooper->registerHandler(mCecRxMessageHandler);

                    // Create looper to send CEC messages...
                    looperName[5] = 'T';
                    mCecTxMessageLooper = new ALooper;
                    mCecTxMessageLooper->setName(looperName);
                    mCecTxMessageLooper->start();

                    // Create Handler to handle transmission of CEC messages...
                    mCecTxMessageHandler = NexusService::CecServiceManager::CecTxMessageHandler::instantiate(this, cecId);
                    mCecTxMessageLooper->registerHandler(mCecTxMessageHandler);

                    status = OK;
                }
            }
        }
    }
    else {
        LOGE("%s: Could not platform initialise CEC%d!!!", __PRETTY_FUNCTION__, cecId);
        status = NO_INIT;
    }
    return status;
}

bool NexusService::CecServiceManager::isPlatformInitialised()
{
    return (mCecTxMessageHandler != NULL && mCecRxMessageHandler != NULL);
}

void NexusService::CecServiceManager::platformUninit()
{
    NEXUS_PlatformConfiguration platformConfig;

    if (mCecTxMessageLooper != NULL) {
        mCecTxMessageLooper->unregisterHandler(mCecTxMessageHandler->id());
        mCecTxMessageLooper->stop();
        mCecTxMessageHandler = NULL;
        mCecTxMessageLooper = NULL;
    }
    if (mCecRxMessageLooper != NULL) {
        mCecRxMessageLooper->unregisterHandler(mCecRxMessageHandler->id());
        mCecRxMessageLooper->stop();
        mCecRxMessageHandler = NULL;
        mCecRxMessageLooper = NULL;
    }

    NEXUS_Platform_GetConfiguration(&platformConfig);
    if (platformConfig.outputs.cec[cecId] == NULL && cecHandle != NULL) {
        NEXUS_Cec_Close(cecHandle);
        cecHandle = NULL;
    }
}

NexusService::CecServiceManager::~CecServiceManager()
{
    LOGV("%s: for CEC%d called", __PRETTY_FUNCTION__, cecId); 
}

