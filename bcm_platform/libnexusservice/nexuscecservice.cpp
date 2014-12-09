/******************************************************************************
 *    (c)2010-2014 Broadcom Corporation
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
void NexusService::CecServiceManager::CecRxMessageHandler::getPhysicalAddress(unsigned inLength __unused, uint8_t *content, unsigned *outLength )
{
    b_cecStatus status;

    if (mCecServiceManager->getCecStatus(&status) == true) {
        content[0] = status.physicalAddress[0];
        content[1] = status.physicalAddress[1];
        *outLength = 2;
    }
    else {
        ALOGE("%s: Could not get CEC%d physical address!!!", __PRETTY_FUNCTION__, cecId);
    }
}

void NexusService::CecServiceManager::CecRxMessageHandler::getDeviceType(unsigned inLength __unused, uint8_t *content, unsigned *outLength )
{
    b_cecStatus status;

    if (mCecServiceManager->getCecStatus(&status) == true) {

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
            default: ALOGW("Unknown Device Type!!!\n"); break;
        }
        *outLength = 1;
    }
    else {
        ALOGE("%s: Could not get CEC%d device type!!!", __PRETTY_FUNCTION__, cecId);
    }
}

void NexusService::CecServiceManager::CecRxMessageHandler::getCecVersion(unsigned inLength __unused, uint8_t *content, unsigned *outLength )
{
    b_cecStatus status;

    if (mCecServiceManager->getCecStatus(&status) == true) {
        ALOGW("CEC Version Requested -> Version: %#x\n", status.cecVersion);
        content[0] = status.cecVersion;
        *outLength = 1;
    }
    else {
        ALOGE("%s: Could not get CEC%d version!!!", __PRETTY_FUNCTION__, cecId);
    }
}

void NexusService::CecServiceManager::CecRxMessageHandler::getOsdName (unsigned inLength __unused, uint8_t *content, unsigned *outLength )
{
#define OSDNAME_SIZE 7 /* Size no larger than 15 */
    char osdName[]= "BRCMSTB";
    unsigned j;

    for( j = 0; j < OSDNAME_SIZE ; j++ ) {
        content[j] = (int)osdName[j];
    }
    *outLength = OSDNAME_SIZE;
}

void NexusService::CecServiceManager::CecRxMessageHandler::getDeviceVendorID(unsigned inLength __unused, uint8_t *content, unsigned *outLength )
{
#define VendorID_SIZE 4 /* Size no larger than 15 */
    char vendorID[]= "BRCM";
    unsigned j;

    for( j = 0; j < VendorID_SIZE ; j++ ) {
        content[j] = (int)vendorID[j];
    }
    *outLength = VendorID_SIZE;
}

void NexusService::CecServiceManager::CecRxMessageHandler::enterStandby (unsigned inLength __unused, uint8_t *content __unused, unsigned *outLength __unused )
{
    LOGD("%s: TV STANDBY RECEIVED: STB WILL NOW ENTER STANDBY...", __PRETTY_FUNCTION__);
    
    system("/system/bin/input keyevent POWER");
}

void NexusService::CecServiceManager::CecRxMessageHandler::reportPowerStatus (unsigned inLength, uint8_t *content, unsigned *outLength )
{
    if (inLength > 1) {
        // Signal reception of power status to other threads...
        mCecServiceManager->mCecGetPowerStatusLock.lock();
        mCecServiceManager->mCecPowerStatus = *content;
        mCecServiceManager->mCecGetPowerStatusCondition.broadcast();
        mCecServiceManager->mCecGetPowerStatusLock.unlock();
    }
    else {
        LOGE("%s: Could not obtain power status from <Report Power Status> CEC%d message!", __PRETTY_FUNCTION__, cecId);
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

            LOGV("%s: Found support for opcode:0x%02X", __PRETTY_FUNCTION__, opCode);

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
                LOGV("%s: Getting parameter %d, length: %d", __PRETTY_FUNCTION__, param_index, outLength);

                if ( responseLength+outLength > NEXUS_CEC_MESSAGE_DATA_SIZE) {
                    LOGW("%s: This parameter has reached over size limit! Last Parameter Length: %d", __PRETTY_FUNCTION__, outLength);
                    return;
                }
                responseLength += outLength;
                for( j = 0 ; j < outLength ; j++ ) {
                    responseBuffer[++index] = tmp[j];
                    LOGV("%s: Message Buffer[%d]: 0x%02X", __PRETTY_FUNCTION__, index, tmp[j]);
                }
                param_index++;
            }

            /* Only response if a response CEC message is required */
            if (opCodeList[i].opCodeResponse) {

                LOGV("%s: Transmitting CEC%d response:0x%02X...", __PRETTY_FUNCTION__, cecId, responseBuffer[0]);

                if (mCecServiceManager->sendCecMessage(mCecServiceManager->mLogicalAddress, destAddr, responseLength, responseBuffer) == OK) {
                    LOGV("%s: Successfully transmitted CEC%d response: 0x%02X", __PRETTY_FUNCTION__, cecId, responseBuffer[0]);
                }
                else {
                    LOGE("%s: ERROR transmitting CEC%d response: 0x%02X!!!", __PRETTY_FUNCTION__, cecId, responseBuffer[0]);
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

    LOGD("%s: CEC%d message opcode 0x%02X length %d params \'%s\' received in thread %d", __PRETTY_FUNCTION__, cecId, opcode, length, msgBuffer, gettid());

    responseLookUp(opcode, length, pBuffer);
}

void NexusService::CecServiceManager::CecRxMessageHandler::onMessageReceived(const sp<AMessage> &msg)
{
    switch (msg->what())
    {
        case kWhatParse:
            LOGV("%s: Parsing CEC message...", __PRETTY_FUNCTION__);
            parseCecMessage(msg);
            break;
        default:
            LOGE("%s: Invalid message received - ignoring!", __PRETTY_FUNCTION__);
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
    int32_t srcaddr;
    int32_t destaddr;
    int32_t opcode;
    size_t  length;
    uint8_t *pBuffer;
    sp<ABuffer> params;
    char msgBuffer[3*(NEXUS_CEC_MESSAGE_DATA_SIZE +1)];
    NEXUS_CecMessage transmitMessage;
    unsigned i, j;
    unsigned loops;
    unsigned maxLoops;

    if (!msg->findInt32("srcaddr", &srcaddr)) {
        LOGE("%s: Could not find \"srcaddr\" in CEC%d message!", __PRETTY_FUNCTION__, cecId);
        return FAILED_TRANSACTION;
    }
    if (!msg->findInt32("destaddr", &destaddr)) {
        LOGE("%s: Could not find \"destaddr\" in CEC%d message!", __PRETTY_FUNCTION__, cecId);
        return FAILED_TRANSACTION;
    }
    if (!msg->findInt32("opcode", &opcode)) {
        LOGE("%s: Could not find \"opcode\" in CEC%d message!", __PRETTY_FUNCTION__, cecId);
        return FAILED_TRANSACTION;
    }
    if (!msg->findSize("length", &length)) {
        LOGE("%s: Could not find \"length\" in CEC%d message!", __PRETTY_FUNCTION__, cecId);
        return FAILED_TRANSACTION;
    }

    if (mCecServiceManager->cecHandle == NULL) {
        LOGE("%s: ERROR: cecHandle is NULL!!!", __PRETTY_FUNCTION__);
        return DEAD_OBJECT;
    }

    NEXUS_Cec_GetDefaultMessageData(&transmitMessage);
    transmitMessage.initiatorAddr = srcaddr;
    transmitMessage.destinationAddr = destaddr;
    transmitMessage.length = length;
    transmitMessage.buffer[0] = opcode;

    msgBuffer[0] = '\0';

    if (length > 1) {
        if (!msg->findBuffer("params", &params)) {
            LOGE("%s: Could not find \"params\" in CEC%d message!", __PRETTY_FUNCTION__, cecId);
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

    // Is this a polling message?
    if (srcaddr == destaddr) {
        maxLoops = 2;
    } else {
        maxLoops = 5;
    }

    for (loops = 0; loops < maxLoops; loops++) {
        NEXUS_Error rc;

        LOGD("%s: Outputting CEC%d message: src addr=0x%02X dest addr=0x%02X opcode=0x%02X length=%d params=\'%s\' [thread %d]", __PRETTY_FUNCTION__,
              cecId, srcaddr, destaddr, opcode, length, msgBuffer, gettid());

        mCecServiceManager->mCecMessageTransmittedLock.lock();
        rc = NEXUS_Cec_TransmitMessage(mCecServiceManager->cecHandle, &transmitMessage);
        if (rc == NEXUS_SUCCESS) {
            status_t status;

            LOGV("%s: Waiting for CEC%d message to be transmitted...", __PRETTY_FUNCTION__, cecId);
            // Now wait up to 500ms for message to be transmitted...
            status = mCecServiceManager->mCecMessageTransmittedCondition.waitRelative(mCecServiceManager->mCecMessageTransmittedLock, 500*1000*1000);
            mCecServiceManager->mCecMessageTransmittedLock.unlock();

            if (status == OK) {
                b_cecStatus cecStatus;
                
                if (mCecServiceManager->getCecStatus(&cecStatus) == true) {
                    if (cecStatus.txMessageAck == true) {
                        LOGV("%s: Successfully sent CEC%d message: opcode=0x%02X.", __PRETTY_FUNCTION__, cecId, opcode);
                        break;
                    }
                    else {
                        ALOGW("%s: Sent CEC%d message: opcode=0x%02X, but no ACK received!", __PRETTY_FUNCTION__, cecId, opcode);
                    }
                }
                else {
                    ALOGE("%s: Could not get CEC%d status!!!", __PRETTY_FUNCTION__, cecId);
                }
            }
            else {
                LOGW("%s: Timed out waiting for CEC%d message be transmitted!", __PRETTY_FUNCTION__, cecId);
            }
            // Always wait 35ms for (start + header block) and (25ms x length) + 25ms between transmitting messages...
            usleep((60 + 25*length) * 1000);
        }
        else {
            mCecServiceManager->mCecMessageTransmittedLock.unlock();
            LOGE("%s: ERROR sending CEC%d message: opcode=0x%02X [rc=%d]%s", __PRETTY_FUNCTION__, cecId, opcode, rc, loops<(maxLoops-1) ? " - retrying..." : "!!!");
            usleep(500 * 1000);
        }
    }
    return (loops < maxLoops) ? OK : UNKNOWN_ERROR;
}

void NexusService::CecServiceManager::CecTxMessageHandler::onMessageReceived(const sp<AMessage> &msg)
{
    switch (msg->what())
    {
        case kWhatSend:
        {
            status_t err;
            uint32_t replyID;

            LOGV("%s: Sending CEC message...", __PRETTY_FUNCTION__);
            err = outputCecMessage(msg);

            if (!msg->senderAwaitsResponse(&replyID)) {
                LOGE("%s: ERROR awaiting response!", __PRETTY_FUNCTION__);
            }
            else {
                sp<AMessage> response = new AMessage;
                response->setInt32("err", err);
                response->postReply(replyID);
            }
            break;
        }
        default:
            LOGE("%s: Invalid message received - ignoring!", __PRETTY_FUNCTION__);
    }
}

NexusService::CecServiceManager::CecTxMessageHandler::~CecTxMessageHandler()
{
    LOGV("%s: for CEC%d called", __PRETTY_FUNCTION__, cecId); 
}

/******************************************************************************
  EventListener methods
******************************************************************************/
status_t
NexusService::CecServiceManager::EventListener::onHdmiCecMessageReceived(int32_t portId, INexusHdmiCecMessageEventListener::hdmiCecMessage_t *message)
{
    ALOGV("%s: portId=%d, %d, %d, %d", __PRETTY_FUNCTION__, portId, message->initiator, message->destination, message->length);

    sp<AMessage> msg = new AMessage;
    sp<ABuffer> buf = new ABuffer(message->length);

    msg->setInt32("opcode", message->body[0]);
    msg->setSize("length",  message->length);

    if (message->length > 1) {
        buf->setRange(0, 0);
        memcpy(buf->data(), &message->body[1], message->length-1);
        buf->setRange(0, message->length-1);
        msg->setBuffer("params", buf);
    }
    msg->setWhat(NexusService::CecServiceManager::CecRxMessageHandler::kWhatParse);
    msg->setTarget(mCecServiceManager->mCecRxMessageHandler->id());
    msg->post();

    return NO_ERROR;
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

        LOGV("%s: CEC Device %d %sReady, Xmit %sPending", __PRETTY_FUNCTION__, param, 
                status.ready ? "" : "Not ",
                status.messageTransmitPending ? "" : "Not "
                );
        LOGV("%s: Logical Address <%d> Acquired", __PRETTY_FUNCTION__, status.logicalAddress);
        pCecServiceManager->mLogicalAddress = status.logicalAddress;

        LOGV("%s: Physical Address: %X.%X.%X.%X", __PRETTY_FUNCTION__,
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

        LOGV("%s: Device %sReady, Xmit %sPending", __PRETTY_FUNCTION__,
                status.ready ? "" : "Not ",
                status.messageTransmitPending ? "" : "Not "
                );

        if (status.messageReceived) {
            LOGV("%s: Msg Recd Status from Phys/Logical Addrs: %X.%X.%X.%X / %d", __PRETTY_FUNCTION__,
                (status.physicalAddress[0] & 0xF0) >> 4, (status.physicalAddress[0] & 0x0F),
                (status.physicalAddress[1] & 0xF0) >> 4, (status.physicalAddress[1] & 0x0F),
                status.logicalAddress) ;

            pCecServiceManager->mLogicalAddress = status.logicalAddress;
            NEXUS_Cec_ReceiveMessage(pCecServiceManager->cecHandle, &receivedMessage);

            LOGV("%s: Cec%d message received length %d.", __PRETTY_FUNCTION__, param, receivedMessage.data.length);

            if (pCecServiceManager->isPlatformInitialised() && receivedMessage.data.length > 0) {
                status_t ret;
                INexusHdmiCecMessageEventListener::hdmiCecMessage_t hdmiMessage;

                memset(&hdmiMessage, 0, sizeof(hdmiMessage));
                hdmiMessage.initiator = receivedMessage.data.initiatorAddr;
                hdmiMessage.destination = receivedMessage.data.destinationAddr;
                hdmiMessage.length = (receivedMessage.data.length <= HDMI_CEC_MESSAGE_BODY_MAX_LENGTH) ? 
                                      receivedMessage.data.length : HDMI_CEC_MESSAGE_BODY_MAX_LENGTH;
                memcpy(hdmiMessage.body, receivedMessage.data.buffer, hdmiMessage.length);

                pCecServiceManager->mEventListenerLock.lock();
                if (pCecServiceManager->mEventListener != NULL) {
                    ret = pCecServiceManager->mEventListener->onHdmiCecMessageReceived(param, &hdmiMessage);
                    if (ret != NO_ERROR) {
                        LOGE("%s: Cec%d onHdmiCecMessageReceived failed (rc=%d)!!!", __PRETTY_FUNCTION__, param, ret);
                    }
                }
                pCecServiceManager->mEventListenerLock.unlock();
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
    
        LOGV("%s: CEC%d Device %sReady, Xmit %sPending", __PRETTY_FUNCTION__, param,
                status.ready ? "" : "Not ",
                status.messageTransmitPending ? "" : "Not "
                );

        LOGV("%s: Msg Xmit Status for Phys/Logical Addrs: %X.%X.%X.%X / %d", __PRETTY_FUNCTION__, 
            (status.physicalAddress[0] & 0xF0) >> 4, (status.physicalAddress[0] & 0x0F),
            (status.physicalAddress[1] & 0xF0) >> 4, (status.physicalAddress[1] & 0x0F),
             status.logicalAddress);

        LOGV("%s: Xmit Msg Acknowledged: %s", __PRETTY_FUNCTION__, 
            status.transmitMessageAcknowledged ? "Yes" : "No") ;
        LOGV("%s: Xmit Msg Pending: %s", __PRETTY_FUNCTION__, 
            status.messageTransmitPending ? "Yes" : "No") ;

        pCecServiceManager->mCecMessageTransmittedLock.lock();
        pCecServiceManager->mCecMessageTransmittedCondition.broadcast();
        pCecServiceManager->mCecMessageTransmittedLock.unlock();
    }
}

status_t NexusService::CecServiceManager::sendCecMessage(uint8_t srcAddr, uint8_t destAddr, size_t length, uint8_t *pBuffer)
{
    status_t err = BAD_VALUE;
    b_cecStatus status;

    // Check to ensure that the device's CEC logical address has been initialised first...
    if (getCecStatus(&status) == false) {
        LOGE("%s: Could not obtain CEC%d status!!!", __PRETTY_FUNCTION__, cecId);
        err = UNKNOWN_ERROR;
    }
    else {
        sp<AMessage> msg = new AMessage;
        sp<ABuffer> buf = new ABuffer(length);

        msg->setInt32("srcaddr", srcAddr);
        msg->setInt32("destaddr", destAddr);
        msg->setInt32("opcode", *pBuffer);
        msg->setSize("length",  length);

        // If we are not sending a polling message, then check to make sure that the
        // logical address has been set prior to sending the message.
        if (srcAddr != destAddr && status.logicalAddress == 0xFF) {
            LOGW("%s: CEC%d logical address not initialised!", __PRETTY_FUNCTION__, cecId);
            err = NO_INIT;
        }
        else {
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
                LOGE("%s: ERROR posting message (err=%d)!!!", __PRETTY_FUNCTION__, err);
                return err;
            }
            else {
                ALOGV("%s: Returned from posting message to looper.", __PRETTY_FUNCTION__);
            }

            if (!response->findInt32("err", &err)) {
                err = OK;
            }
        }
    }
    return err;
}

bool NexusService::CecServiceManager::setPowerState(b_powerState pmState)
{
    bool success = false;
    uint8_t destAddr;
    size_t length;
    uint8_t buffer[NEXUS_CEC_MESSAGE_DATA_SIZE];
    b_cecStatus status;

    // Check to ensure that the device's CEC logical address has been initialised first...
    if (getCecStatus(&status) == false) {
        LOGE("%s: Could not obtain CEC%d status!!!", __PRETTY_FUNCTION__, cecId);
    }
    else if (status.logicalAddress == 0xFF) {
        LOGW("%s: CEC%d logical address not initialised!", __PRETTY_FUNCTION__, cecId);
    }
    else {
        if (ePowerState_S0 == pmState) {
            destAddr = 0;
            length = 1;
            buffer[0] = NEXUS_CEC_OpImageViewOn;

            LOGV("%s: Sending <Image View On> message to turn TV connected to HDMI output %d on...", __PRETTY_FUNCTION__, cecId);
            if (sendCecMessage(mLogicalAddress, destAddr, length, buffer) == OK) {
                LOGD("%s: Successfully sent <Image View On> CEC%d message.", __PRETTY_FUNCTION__, cecId);
                success = true;
            }
            else {
                LOGE("%s: ERROR sending <Image View On> CEC%d message!!!", __PRETTY_FUNCTION__, cecId);
            }

            if (success) {
                LOGV("%s: Broadcasting <Active Source> message to ensure TV is displaying our content from HDMI output %d...", __PRETTY_FUNCTION__, cecId);
                destAddr = 0xF;    // Broadcast address
                length = 3;
                buffer[0] = NEXUS_CEC_OpActiveSource;
                buffer[1] = status.physicalAddress[0];
                buffer[2] = status.physicalAddress[1];

                if (sendCecMessage(mLogicalAddress, destAddr, length, buffer) == OK) {
                    LOGD("%s: Successfully sent <Active Source> CEC%d message.", __PRETTY_FUNCTION__, cecId);
                }
                else {
                    LOGE("%s: ERROR sending <Image View On> CEC%d message!!!", __PRETTY_FUNCTION__, cecId);
                    success = false;
                }
            }
        }
        else {
            destAddr = 0;
            length = 1;
            buffer[0] = NEXUS_CEC_OpStandby;

            LOGV("%s: Sending <Standby> message to place TV connected to HDMI output %d in standby...", __PRETTY_FUNCTION__, cecId);
            if (sendCecMessage(mLogicalAddress, destAddr, length, buffer) == OK) {
                LOGD("%s: Successfully sent <Standby> Cec%d message.", __PRETTY_FUNCTION__, cecId);
                success = true;
            }
            else {
                LOGE("%s: ERROR sending <Standby> Cec%d message!!!", __PRETTY_FUNCTION__, cecId);
            }
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
    b_cecStatus status;

    // Check to ensure that the device's CEC logical address has been initialised first...
    if (getCecStatus(&status) == false) {
        LOGE("%s: Could not obtain CEC%d status!!!", __PRETTY_FUNCTION__, cecId);
    }
    else if (status.logicalAddress == 0xFF) {
        LOGW("%s: CEC%d logical address not initialised!", __PRETTY_FUNCTION__, cecId);
    }
    else {
        destAddr = 0;
        length = 1;
        buffer[0] = NEXUS_CEC_OpGiveDevicePowerStatus;

        LOGV("%s: Sending <Give Device Power Status> message to HDMI output %d on...", __PRETTY_FUNCTION__, cecId);
        if (sendCecMessage(mLogicalAddress, destAddr, length, buffer) == OK) {
            status_t status;

            LOGD("%s: Successfully sent <Give Device Power Status> Cec%d message.", __PRETTY_FUNCTION__, cecId);
                
            /* Now wait up to 1s for the <Report Power Status> receive message */
            mCecGetPowerStatusLock.lock();
            status = mCecGetPowerStatusCondition.waitRelative(mCecGetPowerStatusLock, 1 * 1000 * 1000 * 1000);  // Wait up to 1s
            mCecGetPowerStatusLock.unlock();

            if (status == OK) {
                if (mCecPowerStatus < 4) {
                    *pPowerStatus = mCecPowerStatus;
                    success = true;
                }
                else {
                    LOGE("%s: Invalid <Power Status> parameter 0x%0x!", __PRETTY_FUNCTION__, mCecPowerStatus);
                }
            }
            else {
                LOGE("%s: Timed out waiting for <Report Power Status> CEC%d message!!!", __PRETTY_FUNCTION__, cecId);
            }
        }
        else {
            LOGE("%s: Could not transmit <Give Device Power Status> on Cec%d!", __PRETTY_FUNCTION__, cecId);
        }
    }
    return success;
}

static const b_cecDeviceType fromNexusCecDeviceType[] =
{
    eCecDeviceType_eTv,
    eCecDeviceType_eRecordingDevice,
    eCecDeviceType_eReserved,
    eCecDeviceType_eTuner,
    eCecDeviceType_ePlaybackDevice,
    eCecDeviceType_eAudioSystem,
    eCecDeviceType_ePureCecSwitch,
    eCecDeviceType_eVideoProcessor
};

bool NexusService::CecServiceManager::getCecStatus(b_cecStatus *pCecStatus)
{
    bool success = true;
    NEXUS_CecStatus status;

    if (NEXUS_Cec_GetStatus(cecHandle, &status) != NEXUS_SUCCESS) {
        LOGE("%s: ERROR: Cannot obtain Nexus Cec Status!!!", __PRETTY_FUNCTION__);
        success = false;
    }
    else {
        memset(pCecStatus, 0, sizeof(*pCecStatus));
        pCecStatus->ready              = status.ready;
        pCecStatus->messageRx          = status.messageReceived;
        pCecStatus->messageTxPending   = status.messageTransmitPending;
        pCecStatus->txMessageAck       = status.transmitMessageAcknowledged;
        pCecStatus->physicalAddress[0] = status.physicalAddress[0];
        pCecStatus->physicalAddress[1] = status.physicalAddress[1];
        pCecStatus->logicalAddress     = status.logicalAddress;
        mLogicalAddress                = status.logicalAddress;
        pCecStatus->deviceType         = fromNexusCecDeviceType[status.deviceType];
        pCecStatus->cecVersion         = status.cecVersion;
    }
    return success;
}

status_t NexusService::CecServiceManager::setEventListener(const sp<INexusHdmiCecMessageEventListener> &listener)
{
    mEventListenerLock.lock();
    LOGV("%s: listener=%p", __PRETTY_FUNCTION__, listener.get());
    mEventListener = listener;
    mEventListenerLock.unlock();
    return NO_ERROR;
}

bool NexusService::CecServiceManager::setLogicalAddress(uint8_t addr)
{
    NEXUS_CecSettings cecSettings;

    NEXUS_Cec_GetSettings(cecHandle, &cecSettings);
    cecSettings.logicalAddress = addr;
    LOGV("%s: settings CEC%d logical address to 0x%02x", __PRETTY_FUNCTION__, cecId, addr);
    return (NEXUS_Cec_SetSettings(cecHandle, &cecSettings) == NEXUS_SUCCESS);
}

/* Android-L sets up a property to define the device type and hence
   the logical address of the device.  */
int NexusService::CecServiceManager::getDeviceType()
{
    char value[PROPERTY_VALUE_MAX];
    int type = -1;

    if (property_get("ro.hdmi.device_type", value, NULL)) {
        type = atoi(value);
    }
    return type;
}

status_t NexusService::CecServiceManager::platformInit()
{
    status_t status = OK;
    NEXUS_Error rc;
    NEXUS_PlatformConfiguration *pPlatformConfig;
    NEXUS_CecSettings cecSettings;
    b_cecStatus cecStatus;
    b_hdmiOutputStatus hdmiOutputStatus;
    unsigned loops;

    pPlatformConfig = reinterpret_cast<NEXUS_PlatformConfiguration *>(BKNI_Malloc(sizeof(*pPlatformConfig)));
    if (pPlatformConfig == NULL) {
        ALOGE("%s: Could not allocate enough memory for the platform configuration!!!", __FUNCTION__);
        return NO_MEMORY;
    }
    NEXUS_Platform_GetConfiguration(pPlatformConfig);
    cecHandle  = pPlatformConfig->outputs.cec[cecId];
    BKNI_Free(pPlatformConfig);

    if (cecHandle == NULL) {
        ALOGV("%s: CEC%d not opened at platform init time - opening now...", __PRETTY_FUNCTION__, cecId);
        NEXUS_Cec_GetDefaultSettings(&cecSettings);
        cecHandle = NEXUS_Cec_Open(cecId, &cecSettings);
        if (cecHandle == NULL) {
            ALOGE("%s: Could not open Cec%d!!!", __PRETTY_FUNCTION__, cecId);
            status = NO_INIT;
        }
    }

    if (status == OK && mNexusService->getHdmiOutputStatus(cecId, &hdmiOutputStatus)) {
        int deviceType = getDeviceType();

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

        cecSettings.physicalAddress[0] = hdmiOutputStatus.physicalAddress[0];
        cecSettings.physicalAddress[1] = hdmiOutputStatus.physicalAddress[1];

        if (deviceType != -1) {
            cecSettings.disableLogicalAddressPolling = true;
            cecSettings.logicalAddress = 0xff;
            LOGD("%s: setting CEC%d logical address to 0xFF", __PRETTY_FUNCTION__, cecId);
        }
        
        rc = NEXUS_Cec_SetSettings(cecHandle, &cecSettings);

        if (rc != NEXUS_SUCCESS) {
            LOGE("%s: ERROR setting up Cec%d settings [rc=%d]!!!", __PRETTY_FUNCTION__, cecId, rc);
            status = UNKNOWN_ERROR;
        }
        else {
            /* Enable Cec core */
            NEXUS_Cec_GetSettings(cecHandle, &cecSettings);
            cecSettings.enabled = true;
            rc = NEXUS_Cec_SetSettings(cecHandle, &cecSettings);
            if (rc != NEXUS_SUCCESS) {
                LOGE("%s: ERROR enabling Cec%d [rc=%d]!!!", __PRETTY_FUNCTION__, cecId, rc);
                status = UNKNOWN_ERROR;
            }
            else {
                if (deviceType == -1) {
                    for (loops = 0; loops < 5; loops++) {
                        LOGV("%s: Waiting for Cec%d logical address...", __PRETTY_FUNCTION__, cecId);
                        mCecDeviceReadyLock.lock();
                        status = mCecDeviceReadyCondition.waitRelative(mCecDeviceReadyLock, 1 * 1000 * 1000 * 1000);  // Wait up to 1s
                        mCecDeviceReadyLock.unlock();
                        if (status == OK) {
                            break;
                        }
                    }
                }

                if (getCecStatus(&cecStatus) == true) {

                    LOGV("%s: Device %sReady, Xmit %sPending", __PRETTY_FUNCTION__,
                            cecStatus.ready ? "" : "Not ",
                            cecStatus.messageTxPending ? "" : "Not "
                            );

                    if (deviceType == -1 && cecStatus.logicalAddress == 0xFF) {
                        LOGE("%s: No Cec capable device found on HDMI output %d!", __PRETTY_FUNCTION__, cecId);
                        status = NO_INIT;
                    }
                    else {
                        char looperName[] = "Cec   x Message Looper";
                        looperName[3] = '0' + cecId;

                        LOGD("%s: CEC capable device found on HDMI output %d.", __PRETTY_FUNCTION__, cecId);

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

                        // Create HDMI CEC Message Event Listener only for non Android-L builds
                        if (deviceType == -1) {
                            sp<INexusHdmiCecMessageEventListener> eventListener = new NexusService::CecServiceManager::EventListener(this);
                            status = setEventListener(eventListener);
                        }
                    }
                }
                else {
                    ALOGE("%s: Could not get CEC%d status!!!", __PRETTY_FUNCTION__, cecId);
                    status = NO_INIT;
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
    NEXUS_PlatformConfiguration *pPlatformConfig;
    int deviceType = getDeviceType();

    if (deviceType == -1 && mEventListener != NULL) {
        setEventListener(NULL);
    }

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

    pPlatformConfig = reinterpret_cast<NEXUS_PlatformConfiguration *>(BKNI_Malloc(sizeof(*pPlatformConfig)));
    if (pPlatformConfig == NULL) {
        ALOGE("%s: Could not allocate enough memory for the platform configuration!!!", __FUNCTION__);
    }
    else {
        NEXUS_Platform_GetConfiguration(pPlatformConfig);
        if (pPlatformConfig->outputs.cec[cecId] == NULL && cecHandle != NULL) {
            NEXUS_Cec_Close(cecHandle);
            cecHandle = NULL;
        }
        BKNI_Free(pPlatformConfig);
    }
}

NexusService::CecServiceManager::~CecServiceManager()
{
    LOGV("%s: for CEC%d called", __PRETTY_FUNCTION__, cecId); 
}

