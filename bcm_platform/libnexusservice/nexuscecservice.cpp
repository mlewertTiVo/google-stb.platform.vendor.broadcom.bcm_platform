/******************************************************************************
 *    (c)2010-2015 Broadcom Corporation
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
    uint32_t vendorId;

    vendorId = property_get_int32(PROPERTY_HDMI_CEC_VENDOR_ID, DEFAULT_PROPERTY_HDMI_CEC_VENDOR_ID);

#define VendorID_SIZE 3 /* Size no larger than 3 bytes */
    unsigned j;

    for( j = 0; j < VendorID_SIZE ; j++ ) {
        content[j] = (int)((vendorId >> (VendorID_SIZE-1-j)*8) & 0xFF);
    }
    *outLength = VendorID_SIZE;
}

void NexusService::CecServiceManager::CecRxMessageHandler::enterStandby (unsigned inLength __unused, uint8_t *content __unused, unsigned *outLength __unused )
{
    ALOGD("%s: TV STANDBY RECEIVED: STB WILL NOW ENTER STANDBY...", __PRETTY_FUNCTION__);

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
        ALOGE("%s: Could not obtain power status from <Report Power Status> CEC%d message!", __PRETTY_FUNCTION__, cecId);
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

            ALOGV("%s: Found support for opcode:0x%02X", __PRETTY_FUNCTION__, opCode);

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
                ALOGV("%s: Getting parameter %d, length: %d", __PRETTY_FUNCTION__, param_index, outLength);

                if ( responseLength+outLength > NEXUS_CEC_MESSAGE_DATA_SIZE) {
                    ALOGW("%s: This parameter has reached over size limit! Last Parameter Length: %d", __PRETTY_FUNCTION__, outLength);
                    return;
                }
                responseLength += outLength;
                for( j = 0 ; j < outLength ; j++ ) {
                    responseBuffer[++index] = tmp[j];
                    ALOGV("%s: Message Buffer[%d]: 0x%02X", __PRETTY_FUNCTION__, index, tmp[j]);
                }
                param_index++;
            }

            /* Only response if a response CEC message is required */
            if (opCodeList[i].opCodeResponse) {

                ALOGV("%s: Transmitting CEC%d response:0x%02X...", __PRETTY_FUNCTION__, cecId, responseBuffer[0]);

                if (mCecServiceManager->sendCecMessage(mCecServiceManager->mLogicalAddress, destAddr, responseLength, responseBuffer) == OK) {
                    ALOGV("%s: Successfully transmitted CEC%d response: 0x%02X", __PRETTY_FUNCTION__, cecId, responseBuffer[0]);
                }
                else {
                    ALOGE("%s: ERROR transmitting CEC%d response: 0x%02X!!!", __PRETTY_FUNCTION__, cecId, responseBuffer[0]);
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

    ALOGV("%s: CEC%d message opcode 0x%02X length %d params \'%s\' received in thread %d", __PRETTY_FUNCTION__, cecId, opcode, length, msgBuffer, gettid());

    responseLookUp(opcode, length, pBuffer);
}

void NexusService::CecServiceManager::CecRxMessageHandler::onMessageReceived(const sp<AMessage> &msg)
{
    switch (msg->what())
    {
        case kWhatParse:
            ALOGV("%s: Parsing CEC message...", __PRETTY_FUNCTION__);
            parseCecMessage(msg);
            break;
        default:
            ALOGE("%s: Invalid message received - ignoring!", __PRETTY_FUNCTION__);
    }
}

NexusService::CecServiceManager::CecRxMessageHandler::~CecRxMessageHandler()
{
    ALOGV("%s: for CEC%d called", __PRETTY_FUNCTION__, cecId);
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
    int32_t maxRetries;
    int     retryCount;
    uint8_t *pBuffer;
    sp<ABuffer> params;
    char msgBuffer[3*(NEXUS_CEC_MESSAGE_DATA_SIZE +1)];
    NEXUS_CecMessage transmitMessage;

    if (!msg->findInt32("srcaddr", &srcaddr)) {
        ALOGE("%s: Could not find \"srcaddr\" in CEC%d message!", __PRETTY_FUNCTION__, cecId);
        return FAILED_TRANSACTION;
    }
    if (!msg->findInt32("destaddr", &destaddr)) {
        ALOGE("%s: Could not find \"destaddr\" in CEC%d message!", __PRETTY_FUNCTION__, cecId);
        return FAILED_TRANSACTION;
    }
    if (!msg->findInt32("opcode", &opcode)) {
        ALOGE("%s: Could not find \"opcode\" in CEC%d message!", __PRETTY_FUNCTION__, cecId);
        return FAILED_TRANSACTION;
    }
    if (!msg->findSize("length", &length)) {
        ALOGE("%s: Could not find \"length\" in CEC%d message!", __PRETTY_FUNCTION__, cecId);
        return FAILED_TRANSACTION;
    }
    if (!msg->findInt32("maxRetries", &maxRetries)) {
        ALOGE("%s: Could not find \"maxRetries\" in CEC%d message!", __PRETTY_FUNCTION__, cecId);
        return FAILED_TRANSACTION;
    }

    if (mCecServiceManager->cecHandle == NULL) {
        ALOGE("%s: ERROR: cecHandle is NULL!!!", __PRETTY_FUNCTION__);
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
            ALOGE("%s: Could not find \"params\" in CEC%d message!", __PRETTY_FUNCTION__, cecId);
            return FAILED_TRANSACTION;
        }

        if (params != NULL) {
            unsigned i, j;

            pBuffer = params->base();

            for (i = 0, j = 0; i < length-1 && j<(sizeof(msgBuffer)-1); i++) {
                transmitMessage.buffer[i+1] = *pBuffer;
                j += BKNI_Snprintf(msgBuffer + j, sizeof(msgBuffer)-j, "%02X ", *pBuffer++);
            }
        }
    }

    for (retryCount = 0; retryCount <= maxRetries; retryCount++) {
        NEXUS_Error rc;
        unsigned long timeoutInMs = 0;

        usleep(timeoutInMs * 1000);

        ALOGD("%s: Outputting CEC%d message: src addr=0x%02X dest addr=0x%02X opcode=0x%02X length=%d params=\'%s\' [thread %d]", __PRETTY_FUNCTION__,
              cecId, srcaddr, destaddr, opcode, length, msgBuffer, gettid());

        mCecServiceManager->mCecMessageTransmittedLock.lock();
        rc = NEXUS_Cec_TransmitMessage(mCecServiceManager->cecHandle, &transmitMessage);
        if (rc == NEXUS_SUCCESS) {
            status_t status;

            ALOGV("%s: Waiting for CEC%d message to be transmitted...", __PRETTY_FUNCTION__, cecId);
            // Now wait up to 500ms for message to be transmitted...
            status = mCecServiceManager->mCecMessageTransmittedCondition.waitRelative(mCecServiceManager->mCecMessageTransmittedLock, 500*1000*1000);
            mCecServiceManager->mCecMessageTransmittedLock.unlock();

            if (status == OK) {
                b_cecStatus cecStatus;

                if (mCecServiceManager->getCecStatus(&cecStatus) == true) {
                    if (cecStatus.txMessageAck == true) {
                        ALOGV("%s: Successfully sent CEC%d message: opcode=0x%02X.", __PRETTY_FUNCTION__, cecId, opcode);
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
                ALOGW("%s: Timed out waiting for CEC%d message be transmitted%s", __PRETTY_FUNCTION__, cecId,
                      (retryCount < maxRetries) ? " - retrying..." : "!");
            }
            // Always wait 35ms for (start + header block) and (25ms x length) + 25ms between transmitting messages...
            timeoutInMs = 60 + 25*length;
        }
        else {
            mCecServiceManager->mCecMessageTransmittedLock.unlock();
            ALOGE("%s: ERROR sending CEC%d message: opcode=0x%02X [rc=%d]%s", __PRETTY_FUNCTION__, cecId, opcode, rc,
                  (retryCount < maxRetries) ? " - retrying..." : "!!!");

            timeoutInMs = 500;
        }
    }

    return (retryCount <= maxRetries) ? OK : UNKNOWN_ERROR;
}

void NexusService::CecServiceManager::CecTxMessageHandler::onMessageReceived(const sp<AMessage> &msg)
{
    switch (msg->what())
    {
        case kWhatSend:
        {
            status_t err;
            uint32_t replyID;

            ALOGV("%s: Sending CEC message...", __PRETTY_FUNCTION__);
            err = outputCecMessage(msg);

            if (!msg->senderAwaitsResponse(&replyID)) {
                ALOGE("%s: ERROR awaiting response!", __PRETTY_FUNCTION__);
            }
            else {
                sp<AMessage> response = new AMessage;
                response->setInt32("err", err);
                response->postReply(replyID);
            }
            break;
        }
        default:
            ALOGE("%s: Invalid message received - ignoring!", __PRETTY_FUNCTION__);
    }
}

NexusService::CecServiceManager::CecTxMessageHandler::~CecTxMessageHandler()
{
    ALOGV("%s: for CEC%d called", __PRETTY_FUNCTION__, cecId);
}

NexusService::CecServiceManager::CecServiceManager(NexusService *ns, uint32_t cecId) :  mNexusService(ns), cecId(cecId), cecHandle(NULL),
                                                                                        mLogicalAddress(0xFF), mCecRxMessageHandler(NULL),
                                                                                        mCecTxMessageHandler(NULL)
{
    ALOGV("%s: called for CEC%d", __PRETTY_FUNCTION__, cecId);
    mCecDeviceType = mNexusService->getCecDeviceType(cecId);
}


/******************************************************************************
  EventListener methods
******************************************************************************/
status_t
NexusService::CecServiceManager::EventListener::onHdmiCecMessageReceived(int32_t portId, INexusHdmiCecMessageEventListener::hdmiCecMessage_t *message)
{
    ALOGV("%s: portId=%d, %d, %d, %d, opcode=0x%02x", __PRETTY_FUNCTION__, portId, message->initiator, message->destination, message->length, message->body[0]);

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
    NEXUS_Error rc;
    NEXUS_CecStatus status;
    NexusService::CecServiceManager *pCecServiceManager = reinterpret_cast<NexusService::CecServiceManager *>(context);

    if (pCecServiceManager->cecHandle) {
        rc = NEXUS_Cec_GetStatus(pCecServiceManager->cecHandle, &status);

        if (rc == NEXUS_SUCCESS) {
            ALOGV("%s: CEC Device %d %sReady, Xmit %sPending", __PRETTY_FUNCTION__, param,
                    status.ready ? "" : "Not ",
                    status.messageTransmitPending ? "" : "Not "
                    );
            ALOGD("%s: Logical Address <%d> Acquired", __PRETTY_FUNCTION__, status.logicalAddress);
            pCecServiceManager->mLogicalAddress = status.logicalAddress;

            ALOGD("%s: Physical Address: %X.%X.%X.%X", __PRETTY_FUNCTION__,
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
        else {
            ALOGE("%s: Error obtaining CEC%d status [rc=%d]!!!", __PRETTY_FUNCTION__, param, rc);
        }
    }
}

void NexusService::CecServiceManager::msgReceived_callback(void *context, int param)
{
    NEXUS_Error rc;
    NEXUS_CecStatus status;
    NexusService::CecServiceManager *pCecServiceManager = reinterpret_cast<NexusService::CecServiceManager *>(context);
    NEXUS_CecReceivedMessage receivedMessage;

    if (pCecServiceManager->cecHandle) {
        rc = NEXUS_Cec_GetStatus(pCecServiceManager->cecHandle, &status);

        if (rc == NEXUS_SUCCESS) {
            ALOGV("%s: Device %sReady, Xmit %sPending", __PRETTY_FUNCTION__,
                    status.ready ? "" : "Not ",
                    status.messageTransmitPending ? "" : "Not "
                    );
        }
        else {
            ALOGE("%s: Error obtaining CEC%d status [rc=%d]!!!", __PRETTY_FUNCTION__, param, rc);
            status.messageReceived = false;
        }

        if (status.messageReceived) {
            pCecServiceManager->mLogicalAddress = status.logicalAddress;
            rc = NEXUS_Cec_ReceiveMessage(pCecServiceManager->cecHandle, &receivedMessage);

            if (rc == NEXUS_SUCCESS) {
                ALOGD("%s: CEC%d Msg Recd (opcode=0x%02x, length=%d) for Physical/Logical Addrs: %X.%X.%X.%X/%d", __PRETTY_FUNCTION__, param,
                    receivedMessage.data.buffer[0], receivedMessage.data.length,
                    (status.physicalAddress[0] & 0xF0) >> 4, (status.physicalAddress[0] & 0x0F),
                    (status.physicalAddress[1] & 0xF0) >> 4, (status.physicalAddress[1] & 0x0F),
                    status.logicalAddress) ;

                if (pCecServiceManager->isPlatformInitialised() && receivedMessage.data.length > 0) {
                    status_t ret;
                    INexusHdmiCecMessageEventListener::hdmiCecMessage_t hdmiMessage;

                    memset(&hdmiMessage, 0, sizeof(hdmiMessage));
                    hdmiMessage.initiator = receivedMessage.data.initiatorAddr;
                    hdmiMessage.destination = receivedMessage.data.destinationAddr;
                    hdmiMessage.length = (receivedMessage.data.length <= HDMI_CEC_MESSAGE_BODY_MAX_LENGTH) ?
                                          receivedMessage.data.length : HDMI_CEC_MESSAGE_BODY_MAX_LENGTH;
                    memcpy(hdmiMessage.body, receivedMessage.data.buffer, hdmiMessage.length);

                    ALOGV("%s: portId=%d, %d, %d, %d, opcode=0x%02x", __PRETTY_FUNCTION__, param, hdmiMessage.initiator, hdmiMessage.destination,
                            hdmiMessage.length, hdmiMessage.body[0]);

                    pCecServiceManager->mEventListenerLock.lock();
                    if (pCecServiceManager->mEventListener != NULL) {
                        ret = pCecServiceManager->mEventListener->onHdmiCecMessageReceived(param, &hdmiMessage);
                        if (ret != NO_ERROR) {
                            ALOGE("%s: CEC%d onHdmiCecMessageReceived failed (rc=%d)!!!", __PRETTY_FUNCTION__, param, ret);
                        }
                    }
                    pCecServiceManager->mEventListenerLock.unlock();
                }
            }
            else {
                ALOGE("%s: Error receiving CEC%d message [rc=%d]!!!", __PRETTY_FUNCTION__, param, rc);
            }
        }
    }
}

void NexusService::CecServiceManager::msgTransmitted_callback(void *context, int param)
{
    NEXUS_Error rc;
    NEXUS_CecStatus status;
    NexusService::CecServiceManager *pCecServiceManager = reinterpret_cast<NexusService::CecServiceManager *>(context);

    if (pCecServiceManager->cecHandle) {
        rc = NEXUS_Cec_GetStatus(pCecServiceManager->cecHandle, &status);

        if (rc == NEXUS_SUCCESS) {
            ALOGV("%s: CEC%d Device %sReady, Xmit %sPending", __PRETTY_FUNCTION__, param,
                    status.ready ? "" : "Not ",
                    status.messageTransmitPending ? "" : "Not "
                    );

            ALOGD("%s: CEC%d Msg Xmit Status for Physical/Logical Addrs: %X.%X.%X.%X/%d [ACK=%s, Pending=%s]", __PRETTY_FUNCTION__, param,
                (status.physicalAddress[0] & 0xF0) >> 4, (status.physicalAddress[0] & 0x0F),
                (status.physicalAddress[1] & 0xF0) >> 4, (status.physicalAddress[1] & 0x0F),
                 status.logicalAddress, status.transmitMessageAcknowledged ? "Yes" : "No", status.messageTransmitPending ? "Yes" : "No");

            pCecServiceManager->mCecMessageTransmittedLock.lock();
            pCecServiceManager->mCecMessageTransmittedCondition.broadcast();
            pCecServiceManager->mCecMessageTransmittedLock.unlock();
        }
        else {
            ALOGE("%s: Error obtaining CEC%d status [rc=%d]!!!", __PRETTY_FUNCTION__, param, rc);
        }
    }
}

status_t NexusService::CecServiceManager::sendCecMessage(uint8_t srcAddr, uint8_t destAddr, size_t length, uint8_t *pBuffer, uint8_t maxRetries)
{
    status_t err = NO_ERROR;
    b_cecStatus status;

    // Check to ensure that the device's CEC logical address has been initialised first...
    if (getCecStatus(&status) == false) {
        ALOGE("%s: Could not obtain CEC%d status!!!", __PRETTY_FUNCTION__, cecId);
        err = UNKNOWN_ERROR;
    }
    else {
        sp<AMessage> msg = new AMessage;
        sp<ABuffer> buf = new ABuffer(length);

        msg->setInt32("srcaddr", srcAddr);
        msg->setInt32("destaddr", destAddr);
        msg->setInt32("opcode", *pBuffer);
        msg->setSize("length",  length);

        // If we are sending a polling message, then don't retry if we are using the HDMI Controller...
        if (srcAddr == destAddr) {
            if (mCecDeviceType == eCecDeviceType_eInvalid) {
                maxRetries = 1;
            }
            else {
                maxRetries = 0;
            }
        }
        else if (status.logicalAddress == 0xFF) {
            // If we are not sending a polling message, then check to make sure that the
            // logical address has been set prior to sending the message.
            ALOGW("%s: CEC%d logical address not initialised!", __PRETTY_FUNCTION__, cecId);
            err = NO_INIT;
        }

        if (err == NO_ERROR) {
            msg->setInt32("maxRetries", maxRetries);

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
                ALOGE("%s: ERROR posting message (err=%d)!!!", __PRETTY_FUNCTION__, err);
            }
            else {
                ALOGV("%s: Returned from posting message to looper.", __PRETTY_FUNCTION__);

                if (!response->findInt32("err", &err)) {
                    err = OK;
                }
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
        ALOGE("%s: Could not obtain CEC%d status!!!", __PRETTY_FUNCTION__, cecId);
    }
    else if (status.logicalAddress == 0xFF) {
        ALOGW("%s: CEC%d logical address not initialised!", __PRETTY_FUNCTION__, cecId);
    }
    else {
        if (ePowerState_S0 == pmState) {
            destAddr = 0;
            length = 1;
            buffer[0] = NEXUS_CEC_OpImageViewOn;

            ALOGV("%s: Sending <Image View On> message to turn TV connected to HDMI output %d on...", __PRETTY_FUNCTION__, cecId);
            if (sendCecMessage(mLogicalAddress, destAddr, length, buffer) == OK) {
                ALOGV("%s: Successfully sent <Image View On> CEC%d message.", __PRETTY_FUNCTION__, cecId);
                success = true;
            }
            else {
                ALOGE("%s: ERROR sending <Image View On> CEC%d message!!!", __PRETTY_FUNCTION__, cecId);
            }

            if (success) {
                ALOGV("%s: Broadcasting <Active Source> message to ensure TV is displaying our content from HDMI output %d...", __PRETTY_FUNCTION__, cecId);
                destAddr = 0xF;    // Broadcast address
                length = 3;
                buffer[0] = NEXUS_CEC_OpActiveSource;
                buffer[1] = status.physicalAddress[0];
                buffer[2] = status.physicalAddress[1];

                if (sendCecMessage(mLogicalAddress, destAddr, length, buffer) == OK) {
                    ALOGV("%s: Successfully sent <Active Source> CEC%d message.", __PRETTY_FUNCTION__, cecId);
                }
                else {
                    ALOGE("%s: ERROR sending <Image View On> CEC%d message!!!", __PRETTY_FUNCTION__, cecId);
                    success = false;
                }
            }
        }
        else {
            destAddr = 0;
            length = 1;
            buffer[0] = NEXUS_CEC_OpStandby;

            ALOGV("%s: Sending <Standby> message to place TV connected to HDMI output %d in standby...", __PRETTY_FUNCTION__, cecId);
            if (sendCecMessage(mLogicalAddress, destAddr, length, buffer) == OK) {
                ALOGV("%s: Successfully sent <Standby> CEC%d message.", __PRETTY_FUNCTION__, cecId);
                success = true;
            }
            else {
                ALOGE("%s: ERROR sending <Standby> CEC%d message!!!", __PRETTY_FUNCTION__, cecId);
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
        ALOGE("%s: Could not obtain CEC%d status!!!", __PRETTY_FUNCTION__, cecId);
    }
    else if (status.logicalAddress == 0xFF) {
        ALOGW("%s: CEC%d logical address not initialised!", __PRETTY_FUNCTION__, cecId);
    }
    else {
        destAddr = 0;
        length = 1;
        buffer[0] = NEXUS_CEC_OpGiveDevicePowerStatus;

        ALOGV("%s: Sending <Give Device Power Status> message to HDMI output %d on...", __PRETTY_FUNCTION__, cecId);
        if (sendCecMessage(mLogicalAddress, destAddr, length, buffer) == OK) {
            status_t status;

            ALOGV("%s: Successfully sent <Give Device Power Status> CEC%d message.", __PRETTY_FUNCTION__, cecId);

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
                    ALOGE("%s: Invalid <Power Status> parameter 0x%0x!", __PRETTY_FUNCTION__, mCecPowerStatus);
                }
            }
            else {
                ALOGE("%s: Timed out waiting for <Report Power Status> CEC%d message!!!", __PRETTY_FUNCTION__, cecId);
            }
        }
        else {
            ALOGE("%s: Could not transmit <Give Device Power Status> on CEC%d!", __PRETTY_FUNCTION__, cecId);
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
        ALOGE("%s: ERROR: Cannot obtain Nexus Cec Status!!!", __PRETTY_FUNCTION__);
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
    ALOGV("%s: listener=%p", __PRETTY_FUNCTION__, listener.get());
    mEventListener = listener;
    mEventListenerLock.unlock();
    return NO_ERROR;
}

bool NexusService::CecServiceManager::setLogicalAddress(uint8_t addr)
{
    NEXUS_CecSettings cecSettings;

    NEXUS_Cec_GetSettings(cecHandle, &cecSettings);
    cecSettings.logicalAddress = addr;
    ALOGV("%s: settings CEC%d logical address to 0x%02x", __PRETTY_FUNCTION__, cecId, addr);
    return (NEXUS_Cec_SetSettings(cecHandle, &cecSettings) == NEXUS_SUCCESS);
}

bool NexusService::CecServiceManager::setPhysicalAddress(uint16_t addr)
{
    NEXUS_CecSettings cecSettings;

    NEXUS_Cec_GetSettings(cecHandle, &cecSettings);
    cecSettings.physicalAddress[0] = (addr >> 8) & 0xFF;
    cecSettings.physicalAddress[1] = (addr >> 0) & 0xFF;
    ALOGV("%s: settings CEC%d physical address to %01d.%01d.%01d.%01d", __PRETTY_FUNCTION__, cecId,
            (addr >> 12) & 0x0F,
            (addr >>  8) & 0x0F,
            (addr >>  4) & 0x0F,
            (addr >>  0) & 0x0F);
    return (NEXUS_Cec_SetSettings(cecHandle, &cecSettings) == NEXUS_SUCCESS);
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
        ALOGE("%s: Could not allocate enough memory for the platform configuration!!!", __PRETTY_FUNCTION__);
        return NO_MEMORY;
    }
    NEXUS_Platform_GetConfiguration(pPlatformConfig);
#if NEXUS_HAS_CEC && NEXUS_NUM_CEC
    cecHandle  = pPlatformConfig->outputs.cec[cecId];
#endif
    BKNI_Free(pPlatformConfig);

    if (cecHandle == NULL) {
        ALOGV("%s: CEC%d not opened at platform init time - opening now...", __PRETTY_FUNCTION__, cecId);
        NEXUS_Cec_GetDefaultSettings(&cecSettings);
        cecHandle = NEXUS_Cec_Open(cecId, &cecSettings);
        if (cecHandle == NULL) {
            ALOGE("%s: Could not open CEC%d!!!", __PRETTY_FUNCTION__, cecId);
            status = NO_INIT;
        }
    }

    if (status == OK && mNexusService->getHdmiOutputStatus(cecId, &hdmiOutputStatus)) {
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

        if (mCecDeviceType != eCecDeviceType_eInvalid) {
            cecSettings.disableLogicalAddressPolling = true;
            cecSettings.logicalAddress = 0xff;
            ALOGV("%s: setting CEC%d logical address to 0xFF", __PRETTY_FUNCTION__, cecId);
        }

        rc = NEXUS_Cec_SetSettings(cecHandle, &cecSettings);

        if (rc != NEXUS_SUCCESS) {
            ALOGE("%s: ERROR setting up CEC%d settings [rc=%d]!!!", __PRETTY_FUNCTION__, cecId, rc);
            status = UNKNOWN_ERROR;
        }
        else {
            /* Enable Cec core */
            NEXUS_Cec_GetSettings(cecHandle, &cecSettings);
            cecSettings.enabled = true;
            rc = NEXUS_Cec_SetSettings(cecHandle, &cecSettings);
            if (rc != NEXUS_SUCCESS) {
                ALOGE("%s: ERROR enabling CEC%d [rc=%d]!!!", __PRETTY_FUNCTION__, cecId, rc);
                status = UNKNOWN_ERROR;
            }
            else {
                if (mCecDeviceType == eCecDeviceType_eInvalid) {
                    for (loops = 0; loops < 5; loops++) {
                        ALOGV("%s: Waiting for CEC%d logical address...", __PRETTY_FUNCTION__, cecId);
                        mCecDeviceReadyLock.lock();
                        status = mCecDeviceReadyCondition.waitRelative(mCecDeviceReadyLock, 1 * 1000 * 1000 * 1000);  // Wait up to 1s
                        mCecDeviceReadyLock.unlock();
                        if (status == OK) {
                            break;
                        }
                    }
                }

                if (getCecStatus(&cecStatus) == true) {

                    ALOGV("%s: Device %sReady, Xmit %sPending", __PRETTY_FUNCTION__,
                            cecStatus.ready ? "" : "Not ",
                            cecStatus.messageTxPending ? "" : "Not "
                            );

                    if (mCecDeviceType == eCecDeviceType_eInvalid && cecStatus.logicalAddress == 0xFF) {
                        ALOGE("%s: No Cec capable device found on HDMI output %d!", __PRETTY_FUNCTION__, cecId);
                        status = NO_INIT;
                    }
                    else {
                        char looperName[] = "Cec   x Message Looper";
                        looperName[3] = '0' + cecId;

                        ALOGD("%s: CEC capable device found on HDMI output %d.", __PRETTY_FUNCTION__, cecId);

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
                        if (mCecDeviceType == eCecDeviceType_eInvalid) {
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
        ALOGE("%s: Could not platform initialise CEC%d!!!", __PRETTY_FUNCTION__, cecId);
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

    if (mCecDeviceType == eCecDeviceType_eInvalid && mEventListener != NULL) {
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
        ALOGE("%s: Could not allocate enough memory for the platform configuration!!!", __PRETTY_FUNCTION__);
    }
    else {
        NEXUS_Platform_GetConfiguration(pPlatformConfig);
        if (cecHandle != NULL
#if NEXUS_HAS_CEC && NEXUS_NUM_CEC
             && pPlatformConfig->outputs.cec[cecId] == NULL
#endif
           ) {
            NEXUS_Cec_Close(cecHandle);
            cecHandle = NULL;
        }
        BKNI_Free(pPlatformConfig);
    }
}

NexusService::CecServiceManager::~CecServiceManager()
{
    ALOGV("%s: for CEC%d called", __PRETTY_FUNCTION__, cecId);
}

