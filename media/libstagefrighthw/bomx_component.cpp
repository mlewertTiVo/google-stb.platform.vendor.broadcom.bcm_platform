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
 * $brcm_Workfile: $
 * $brcm_Revision: $
 * $brcm_Date: $
 *
 * Module Description:
 *
 * Revision History:
 *
 * $brcm_Log: $
 *
 *****************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "bomx_component"

#include "bomx_component.h"
#include "bomx_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <utils/threads.h>
#include <cutils/sched_policy.h>
#include <cutils/properties.h>

#define B_THREAD_EXIT_TIMEOUT           5000

static BLST_Q_HEAD(ComponentResourceList,BOMX_ComponentResources) g_resourceList;
static B_MutexHandle g_resourceMutex;

static void BOMX_Component_CommandEvent(void *pParam);

static OMX_ERRORTYPE BOMX_Component_GetComponentVersion(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_OUT OMX_STRING pComponentName,
    OMX_OUT OMX_VERSIONTYPE* pComponentVersion,
    OMX_OUT OMX_VERSIONTYPE* pSpecVersion,
    OMX_OUT OMX_UUIDTYPE* pComponentUUID);

static OMX_ERRORTYPE BOMX_Component_SendCommand(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_COMMANDTYPE Cmd,
    OMX_IN  OMX_U32 nParam1,
    OMX_IN  OMX_PTR pCmdData);

static OMX_ERRORTYPE BOMX_Component_GetParameter(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nParamIndex,
    OMX_INOUT OMX_PTR pComponentParameterStructure);

static OMX_ERRORTYPE BOMX_Component_SetParameter(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nIndex,
    OMX_IN  OMX_PTR pComponentParameterStructure);

static OMX_ERRORTYPE BOMX_Component_GetConfig(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nIndex,
    OMX_INOUT OMX_PTR pComponentConfigStructure);

static OMX_ERRORTYPE BOMX_Component_SetConfig(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nIndex,
    OMX_IN  OMX_PTR pComponentConfigStructure);

static OMX_ERRORTYPE BOMX_Component_GetExtensionIndex(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_STRING cParameterName,
    OMX_OUT OMX_INDEXTYPE* pIndexType);

static OMX_ERRORTYPE BOMX_Component_GetState(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_OUT OMX_STATETYPE* pState);

static OMX_ERRORTYPE BOMX_Component_ComponentTunnelRequest(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_U32 nPort,
    OMX_IN  OMX_HANDLETYPE hTunneledComp,
    OMX_IN  OMX_U32 nTunneledPort,
    OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup);

static OMX_ERRORTYPE BOMX_Component_UseBuffer(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes,
    OMX_IN OMX_U8* pBuffer);

static OMX_ERRORTYPE BOMX_Component_AllocateBuffer(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes);

static OMX_ERRORTYPE BOMX_Component_FreeBuffer(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_U32 nPortIndex,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE BOMX_Component_EmptyThisBuffer(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE BOMX_Component_FillThisBuffer(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE BOMX_Component_SetCallbacks(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
    OMX_IN  OMX_PTR pAppData);

static OMX_ERRORTYPE BOMX_Component_ComponentDeInit(
    OMX_IN  OMX_HANDLETYPE hComponent);

static OMX_ERRORTYPE BOMX_Component_UseEGLImage(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN void* eglImage);

static OMX_ERRORTYPE BOMX_Component_ComponentRoleEnum(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_OUT OMX_U8 *cRole,
    OMX_IN OMX_U32 nIndex);

OMX_ERRORTYPE BOMX_InitComponentResourceList()
{
    BLST_Q_INIT(&g_resourceList);
    ALOG_ASSERT(NULL == g_resourceMutex);
    g_resourceMutex = B_Mutex_Create(NULL);
    if ( NULL == g_resourceMutex )
    {
        ALOGW("Unable to create resource mutex");
        return BOMX_ERR_TRACE(OMX_ErrorUndefined);
    }
    return OMX_ErrorNone;
}

void BOMX_UninitComponentResourceList()
{
    BOMX_ComponentResources *pRes;

    ALOG_ASSERT(NULL != g_resourceMutex);

    while ( NULL != (pRes=BLST_Q_FIRST(&g_resourceList)) )
    {
        ALOGW("Freeing active resource entry %p", pRes);
        BLST_Q_REMOVE_HEAD(&g_resourceList, node);
        delete pRes;
    }

    B_Mutex_Destroy(g_resourceMutex);
    g_resourceMutex = NULL;
}

static void BOMX_ComponentThread(void *pParam)
{
    BOMX_Component *pComponent = static_cast <BOMX_Component *> (pParam);
    int priority;
    if (pComponent->getSchedPriority(priority)) {
        setpriority(PRIO_PROCESS, 0, priority);
        set_sched_policy(0, SP_FOREGROUND);
    }
    ALOGV("%s: Scheduler Thread Start", pComponent->GetName());
    pComponent->RunScheduler();
    ALOGV("%s: Scheduler Exited", pComponent->GetName());
}

static void BOMX_Component_EosEvent(void *pParam)
{
    BOMX_Component *pComponent = (BOMX_Component *)pParam;

    pComponent->Eos();
}

void BOMX_Component::Eos()
{
    unsigned i;
    BOMX_Component *pTunnelComp;

    // Send callback to application
    if ( m_callbacks.EventHandler )
    {
        (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate, OMX_EventBufferFlag, 0, 0, NULL);
    }
    // Notify downstream tunneled components of EOS
    for ( i = 0; i < m_numVideoPorts; i++ )
    {
        if ( m_pVideoPorts[i]->GetDir() == OMX_DirOutput )
        {
            pTunnelComp = m_pVideoPorts[i]->GetTunnelComponent();
            if ( pTunnelComp )
            {
                pTunnelComp->HandleEOS();
            }
        }
    }
    for ( i = 0; i < m_numAudioPorts; i++ )
    {
        if ( m_pAudioPorts[i]->GetDir() == OMX_DirOutput )
        {
            pTunnelComp = m_pAudioPorts[i]->GetTunnelComponent();
            if ( pTunnelComp )
            {
                pTunnelComp->HandleEOS();
            }
        }
    }
    for ( i = 0; i < m_numImagePorts; i++ )
    {
        if ( m_pImagePorts[i]->GetDir() == OMX_DirOutput )
        {
            pTunnelComp = m_pImagePorts[i]->GetTunnelComponent();
            if ( pTunnelComp )
            {
                pTunnelComp->HandleEOS();
            }
        }
    }
    // Don't handle other ports - EOS doesn't propagate to clocks
}

BOMX_Component::BOMX_Component(
        OMX_COMPONENTTYPE *pComponentType,
        const OMX_STRING pName,
        const OMX_PTR pAppData,
        const OMX_CALLBACKTYPE *pCallbacks,
        const char *(*pGetRole)(unsigned roleIndex),
        bool overwriteThreadPriority,
        uint32_t threadPriority) :
    m_pComponentType(NULL),
    m_schedulerStopped(false),
    m_version(0),
    m_numAudioPorts(0),
    m_audioPortBase(0),
    m_numImagePorts(0),
    m_imagePortBase(0),
    m_numVideoPorts(0),
    m_videoPortBase(0),
    m_numOtherPorts(0),
    m_otherPortBase(0),
    m_pResources(NULL),
    m_currentState(OMX_StateLoaded),
    m_targetState(OMX_StateLoaded),
    m_hScheduler(NULL),
    m_hMutex(NULL),
    m_hCommandEvent(NULL),
    m_hPortEvent(NULL),
    m_hEosEvent(NULL),
    m_hExitEvent(NULL),
    m_hThread(NULL),
    m_commandEventId(NULL),
    m_eosEventId(NULL),
    m_hCommandQueue(NULL),
    m_portWaitTimeout(0),
    m_overwriteThreadPriority(overwriteThreadPriority),
    m_threadPriority(threadPriority)
{
    ALOG_ASSERT(NULL != pComponentType);
    m_pComponentType = pComponentType;
    memset(m_uuid, 0, sizeof(m_uuid));
    FILE *pUuid = fopen("/proc/sys/kernel/random/uuid", "r");
    if ( pUuid )
    {
        fread(m_uuid, 1, 35, pUuid);
        fclose(pUuid);
    }
    if ( pCallbacks )
    {
        m_callbacks = *pCallbacks;
    }
    else
    {
        memset(&m_callbacks, 0, sizeof(m_callbacks));
    }
    m_pGetRole = pGetRole;
    m_hScheduler = B_Scheduler_Create(NULL);
    ALOG_ASSERT(NULL != m_hScheduler);
    m_hMutex = B_Mutex_Create(NULL);
    ALOG_ASSERT(NULL != m_hMutex);
    m_hCommandEvent = B_Event_Create(NULL);
    ALOG_ASSERT(NULL != m_hCommandEvent);
    m_commandEventId = B_Scheduler_RegisterEvent(m_hScheduler, m_hMutex, m_hCommandEvent, BOMX_Component_CommandEvent, this);
    ALOG_ASSERT(NULL != m_commandEventId);
    m_hEosEvent = B_Event_Create(NULL);
    ALOG_ASSERT(NULL != m_hEosEvent);
    m_eosEventId = B_Scheduler_RegisterEvent(m_hScheduler, m_hMutex, m_hEosEvent, BOMX_Component_EosEvent, this);
    ALOG_ASSERT(NULL != m_eosEventId);
    m_hPortEvent = B_Event_Create(NULL);
    ALOG_ASSERT(NULL != m_hPortEvent);
    m_hExitEvent = B_Event_Create(NULL);
    ALOG_ASSERT(NULL != m_hExitEvent);

    B_MessageQueueSettings messageQueueSettings;
    B_MessageQueue_GetDefaultSettings(&messageQueueSettings);
    messageQueueSettings.maxMessages = BOMX_COMPONENT_MAX_MSGS;
    messageQueueSettings.maxMessageSize = sizeof(BOMX_CommandMsg);
    m_hCommandQueue = B_MessageQueue_Create(&messageQueueSettings);
    ALOG_ASSERT(NULL != m_hCommandQueue);
    m_numAudioPorts = m_numImagePorts = m_numVideoPorts = m_numOtherPorts = 0;
    memset(m_pAudioPorts, 0, sizeof(m_pAudioPorts));
    memset(m_pImagePorts, 0, sizeof(m_pAudioPorts));
    memset(m_pVideoPorts, 0, sizeof(m_pAudioPorts));
    memset(m_pOtherPorts, 0, sizeof(m_pAudioPorts));

    strncpy(m_componentName, (const char *)pName, OMX_MAX_STRINGNAME_SIZE);
    m_componentName[OMX_MAX_STRINGNAME_SIZE-1] = '\0';

    // Populate callbacks for OMX core - these will automatically handle synchronization for the client
    pComponentType->pComponentPrivate = static_cast <void *> (this);
    pComponentType->pApplicationPrivate = pAppData;
    pComponentType->GetComponentVersion = BOMX_Component_GetComponentVersion;
    pComponentType->SendCommand = BOMX_Component_SendCommand;
    pComponentType->GetParameter = BOMX_Component_GetParameter;
    pComponentType->SetParameter = BOMX_Component_SetParameter;
    pComponentType->GetConfig = BOMX_Component_GetConfig;
    pComponentType->SetConfig = BOMX_Component_SetConfig;
    pComponentType->GetExtensionIndex = BOMX_Component_GetExtensionIndex;
    pComponentType->GetState = BOMX_Component_GetState;
    pComponentType->ComponentTunnelRequest = BOMX_Component_ComponentTunnelRequest;
    pComponentType->UseBuffer = BOMX_Component_UseBuffer;
    pComponentType->AllocateBuffer = BOMX_Component_AllocateBuffer;
    pComponentType->FreeBuffer = BOMX_Component_FreeBuffer;
    pComponentType->EmptyThisBuffer = BOMX_Component_EmptyThisBuffer;
    pComponentType->FillThisBuffer = BOMX_Component_FillThisBuffer;
    pComponentType->SetCallbacks = BOMX_Component_SetCallbacks;
    pComponentType->ComponentDeInit = BOMX_Component_ComponentDeInit;
    pComponentType->UseEGLImage = BOMX_Component_UseEGLImage;
    pComponentType->ComponentRoleEnum = BOMX_Component_ComponentRoleEnum;

    SetRole("unknown");

    m_logMask = property_get_int32(BCM_DYN_MEDIA_LOG_MASK, B_LOG_MASK_DEFAULT);

    // Currently, each component requires its own thread - it's by far the easiest way to
    // Implement the command queue/response handling
    m_hThread = B_Thread_Create(pName, BOMX_ComponentThread, static_cast <void *> (this), NULL);
    ALOG_ASSERT(NULL != m_hThread);
}

BOMX_Component::~BOMX_Component()
{
    ALOGV("Destroying component %s", GetName());

    if ( m_hThread )
    {
        ShutdownScheduler();
        B_Thread_Destroy(m_hThread);
    }

    if ( m_hCommandQueue )
    {
        B_MessageQueue_Destroy(m_hCommandQueue);
    }
    if ( m_eosEventId )
    {
        B_Scheduler_UnregisterEvent(m_hScheduler, m_eosEventId);
    }
    if ( m_commandEventId )
    {
        B_Scheduler_UnregisterEvent(m_hScheduler, m_commandEventId);
    }
    else
    {
        ALOGW("NOT Unregistering event");
    }
    if ( m_hScheduler )
    {
        BKNI_Sleep(10);
        B_Scheduler_Destroy(m_hScheduler);
    }
    if ( m_hEosEvent )
    {
        B_Event_Destroy(m_hEosEvent);
    }
    if ( m_hCommandEvent )
    {
        B_Event_Destroy(m_hCommandEvent);
    }
    if ( m_hPortEvent )
    {
        B_Event_Destroy(m_hPortEvent);
    }
    if ( m_hExitEvent )
    {
        B_Event_Destroy(m_hExitEvent);
    }
    if ( m_hMutex )
    {
        B_Mutex_Destroy(m_hMutex);
    }
}

OMX_ERRORTYPE BOMX_Component::StateChangeStart(OMX_STATETYPE newState)
{
    // Check if state transition is already in progress...
    if ( m_currentState != m_targetState )
    {
        return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateTransition);
    }
    // You can't transition to a state you're already in
    if ( newState == m_currentState )
    {
        return BOMX_ERR_TRACE(OMX_ErrorSameState);
    }
    // Validate this is a valid state change
    if ( newState != OMX_StateInvalid )
    {
        // Invalid can be reached from any state - check other State Changes (See Figure 2-3 of Spec 1.1.2)
        switch ( m_currentState )
        {
        case OMX_StateInvalid:
            // No transitions out of invalid are legal
            return BOMX_ERR_TRACE(OMX_ErrorInvalidState);
        case OMX_StateLoaded:
            switch ( newState )
            {
            case OMX_StateWaitForResources:
            case OMX_StateIdle:
                break;
            default:
                return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateTransition);
            }
            break;
        case OMX_StateIdle:
            switch ( newState )
            {
            case OMX_StateLoaded:
            case OMX_StateExecuting:
            case OMX_StatePause:
                break;
            default:
                return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateTransition);
            }
            break;
        case OMX_StateExecuting:
            switch ( newState )
            {
            case OMX_StateIdle:
            case OMX_StatePause:
                break;
            default:
                return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateTransition);
            }
            break;
        case OMX_StatePause:
            switch ( newState )
            {
            case OMX_StateIdle:
            case OMX_StateExecuting:
                break;
            default:
                return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateTransition);
            }
            break;
        case OMX_StateWaitForResources:
            switch ( newState )
            {
            case OMX_StateIdle:
            case OMX_StateLoaded:
                break;
            default:
                return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateTransition);
            }
            break;
        default:
            // Unknown State Transition
            return BOMX_ERR_TRACE(OMX_ErrorIncorrectStateTransition);
        }
    }

    m_targetState = newState;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_Component::GetComponentVersion(
        OMX_OUT OMX_STRING pComponentName,
        OMX_OUT OMX_VERSIONTYPE* pComponentVersion,
        OMX_OUT OMX_VERSIONTYPE* pSpecVersion,
        OMX_OUT OMX_UUIDTYPE* pComponentUUID)
{
    ALOG_ASSERT(NULL != pComponentName);
    ALOG_ASSERT(NULL != pComponentVersion);
    ALOG_ASSERT(NULL != pSpecVersion);
    ALOG_ASSERT(NULL != pComponentUUID);

    strncpy(pComponentName, m_componentName, OMX_MAX_STRINGNAME_SIZE);
    pComponentVersion->nVersion = m_version;
    pSpecVersion->s.nVersionMajor = 1;
    pSpecVersion->s.nVersionMinor = 0;
    pSpecVersion->s.nRevision = 0;
    pSpecVersion->s.nStep = 0;
    strncpy((char *)*pComponentUUID, m_uuid, sizeof(m_uuid));
    return OMX_ErrorNone;
}

BOMX_Port *BOMX_Component::FindPortByIndex(unsigned index)
{
    if ( index >= m_audioPortBase && index < (m_audioPortBase+m_numAudioPorts) )
    {
        return static_cast <BOMX_Port *> (m_pAudioPorts[index-m_audioPortBase]);
    }
    else if ( index >= m_videoPortBase && index < (m_videoPortBase+m_numVideoPorts) )
    {
        return static_cast <BOMX_Port *> (m_pVideoPorts[index-m_videoPortBase]);
    }
    else if ( index >= m_imagePortBase && index < (m_imagePortBase+m_numImagePorts) )
    {
        return static_cast <BOMX_Port *> (m_pImagePorts[index-m_imagePortBase]);
    }
    else if ( index >= m_otherPortBase && index < (m_otherPortBase+m_numOtherPorts) )
    {
        return static_cast <BOMX_Port *> (m_pOtherPorts[index-m_otherPortBase]);
    }
    else
    {
        return NULL;
    }
}

OMX_ERRORTYPE BOMX_Component::GetParameter(
        OMX_IN  OMX_INDEXTYPE nParamIndex,
        OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    OMX_PORT_PARAM_TYPE *pPortParam;

    // All of these parameters are mandatory for every component
    switch ( nParamIndex )
    {
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE *pDefinition = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pDefinition);
        BOMX_Port *pPort = FindPortByIndex(pDefinition->nPortIndex);
        if ( NULL == pPort )
        {
            ALOGW("Invalid port number %u", pDefinition->nPortIndex);
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        pPort->GetDefinition(pDefinition);
        return OMX_ErrorNone;
    }
    case OMX_IndexParamCompBufferSupplier:
    {
        OMX_PARAM_BUFFERSUPPLIERTYPE *pSupplier = (OMX_PARAM_BUFFERSUPPLIERTYPE *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pSupplier);
        BOMX_Port *pPort = FindPortByIndex(pSupplier->nPortIndex);
        if ( NULL == pPort )
        {
            ALOGW("Invalid port number %u", pSupplier->nPortIndex);
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        pSupplier->eBufferSupplier = pPort->GetSupplier();
        return OMX_ErrorNone;
    }
    case OMX_IndexParamAudioInit:
        pPortParam = (OMX_PORT_PARAM_TYPE *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pPortParam);
        pPortParam->nPorts = m_numAudioPorts;
        pPortParam->nStartPortNumber = m_audioPortBase;
        return OMX_ErrorNone;
    case OMX_IndexParamImageInit:
        pPortParam = (OMX_PORT_PARAM_TYPE *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pPortParam);
        pPortParam->nPorts = m_numImagePorts;
        pPortParam->nStartPortNumber = m_imagePortBase;
        return OMX_ErrorNone;
    case OMX_IndexParamVideoInit:
        pPortParam = (OMX_PORT_PARAM_TYPE *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pPortParam);
        pPortParam->nPorts = m_numVideoPorts;
        pPortParam->nStartPortNumber = m_videoPortBase;
        return OMX_ErrorNone;
    case OMX_IndexParamOtherInit:
        pPortParam = (OMX_PORT_PARAM_TYPE *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pPortParam);
        pPortParam->nPorts = m_numOtherPorts;
        pPortParam->nStartPortNumber = m_otherPortBase;
        return OMX_ErrorNone;
    case OMX_IndexParamAudioPortFormat:
    {
            OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
            BOMX_STRUCT_VALIDATE(pAudioPortFormat);
            if ( pAudioPortFormat->nPortIndex < m_audioPortBase ||
                 pAudioPortFormat->nPortIndex >= (m_audioPortBase+m_numAudioPorts) )
            {
                ALOGW("Invalid audio port number %u", pAudioPortFormat->nPortIndex);
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            BOMX_AudioPort *pAudioPort = m_pAudioPorts[pAudioPortFormat->nPortIndex-m_audioPortBase];
            ALOG_ASSERT(NULL != pAudioPort);
            return pAudioPort->GetPortFormat(pAudioPortFormat->nIndex, pAudioPortFormat);
    }
    case OMX_IndexParamVideoPortFormat:
    {
            OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
            BOMX_STRUCT_VALIDATE(pVideoPortFormat);
            if ( pVideoPortFormat->nPortIndex < m_videoPortBase ||
                 pVideoPortFormat->nPortIndex >= (m_videoPortBase+m_numVideoPorts) )
            {
                ALOGW("Invalid video port number %u", pVideoPortFormat->nPortIndex);
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            BOMX_VideoPort *pVideoPort = m_pVideoPorts[pVideoPortFormat->nPortIndex-m_videoPortBase];
            ALOG_ASSERT(NULL != pVideoPort);
            return pVideoPort->GetPortFormat(pVideoPortFormat->nIndex, pVideoPortFormat);
    }
    case OMX_IndexParamImagePortFormat:
    {
            OMX_IMAGE_PARAM_PORTFORMATTYPE *pImagePortFormat = (OMX_IMAGE_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
            BOMX_STRUCT_VALIDATE(pImagePortFormat);
            if ( pImagePortFormat->nPortIndex < m_imagePortBase ||
                 pImagePortFormat->nPortIndex >= (m_imagePortBase+m_numImagePorts) )
            {
                ALOGW("Invalid image port number %u", pImagePortFormat->nPortIndex);
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            BOMX_ImagePort *pImagePort = m_pImagePorts[pImagePortFormat->nPortIndex-m_imagePortBase];
            ALOG_ASSERT(NULL != pImagePort);
            return pImagePort->GetPortFormat(pImagePortFormat->nIndex, pImagePortFormat);
    }
    case OMX_IndexParamOtherPortFormat:
    {
            OMX_OTHER_PARAM_PORTFORMATTYPE *pOtherPortFormat = (OMX_OTHER_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
            BOMX_STRUCT_VALIDATE(pOtherPortFormat);
            if ( pOtherPortFormat->nPortIndex < m_otherPortBase ||
                 pOtherPortFormat->nPortIndex >= (m_otherPortBase+m_numOtherPorts) )
            {
                ALOGW("Invalid other port number %u", pOtherPortFormat->nPortIndex);
                return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
            }
            BOMX_OtherPort *pOtherPort = m_pOtherPorts[pOtherPortFormat->nPortIndex-m_otherPortBase];
            ALOG_ASSERT(NULL != pOtherPort);
            return pOtherPort->GetPortFormat(pOtherPortFormat->nIndex, pOtherPortFormat);
    }
    case OMX_IndexParamStandardComponentRole:
    {
        OMX_PARAM_COMPONENTROLETYPE *pCompRole = (OMX_PARAM_COMPONENTROLETYPE *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pCompRole);
        GetRole((char *)pCompRole->cRole);
        return OMX_ErrorNone;
    }
    default:
        ALOGI("Unsupported get-parameter index %u (%#x)", nParamIndex, nParamIndex);
        return OMX_ErrorUnsupportedIndex;
    }
}

OMX_ERRORTYPE BOMX_Component::SetParameter(
        OMX_IN  OMX_INDEXTYPE nIndex,
        OMX_IN  OMX_PTR pComponentParameterStructure)
{
    // All of these parameters are mandatory for every component
    switch ( nIndex )
    {
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE *pDefinition = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pDefinition);
        BOMX_Port *pPort = FindPortByIndex(pDefinition->nPortIndex);
        if ( NULL == pPort )
        {
            ALOGW("Invalid port number %u", pDefinition->nPortIndex);
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        return BOMX_ERR_TRACE(pPort->SetDefinition(pDefinition));
    }
    case OMX_IndexParamCompBufferSupplier:
    {
        OMX_PARAM_BUFFERSUPPLIERTYPE *pSupplier = (OMX_PARAM_BUFFERSUPPLIERTYPE *)pComponentParameterStructure;
        BOMX_STRUCT_VALIDATE(pSupplier);
        BOMX_Port *pPort = FindPortByIndex(pSupplier->nPortIndex);
        if ( NULL == pPort )
        {
            ALOGW("Invalid port number %u", pSupplier->nPortIndex);
            return BOMX_ERR_TRACE(OMX_ErrorBadPortIndex);
        }
        switch ( pSupplier->eBufferSupplier )
        {
        case OMX_BufferSupplyUnspecified:
        case OMX_BufferSupplyInput:
        case OMX_BufferSupplyOutput:
            break;
        default:
            ALOGW("Invalid buffer supplier type %d", (int)pSupplier->eBufferSupplier);
        }
        pPort->SetSupplier(pSupplier->eBufferSupplier);
        return OMX_ErrorNone;
    }
    // Handle port format changes in the sub-classes.  The default port config isn't known in this layer.
    #if 0
    case OMX_IndexParamAudioPortFormat:
    case OMX_IndexParamVideoPortFormat:
    case OMX_IndexParamImagePortFormat:
    case OMX_IndexParamOtherPortFormat:
    #endif
    default:
        ALOGI("Unsupported set-parameter index %u (%#x)", nIndex, nIndex);
        return OMX_ErrorUnsupportedIndex;
    }
}

OMX_ERRORTYPE BOMX_Component::GetConfig(
        OMX_IN  OMX_INDEXTYPE nIndex,
        OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    BSTD_UNUSED(pComponentConfigStructure);
    ALOGW("Unsupported config index %u (%#x)", nIndex, nIndex);
    return BOMX_ERR_TRACE(OMX_ErrorUnsupportedIndex);
}

OMX_ERRORTYPE BOMX_Component::SetConfig(
        OMX_IN  OMX_INDEXTYPE nIndex,
        OMX_IN  OMX_PTR pComponentConfigStructure)
{
    BSTD_UNUSED(pComponentConfigStructure);
    ALOGW("Unsupported config index %u (%#x)", nIndex, nIndex);
    return BOMX_ERR_TRACE(OMX_ErrorUnsupportedIndex);
}

OMX_ERRORTYPE BOMX_Component::GetExtensionIndex(
        OMX_IN  OMX_STRING cParameterName,
        OMX_OUT OMX_INDEXTYPE* pIndexType)
{
    BSTD_UNUSED(cParameterName);
    BSTD_UNUSED(pIndexType);
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

OMX_ERRORTYPE BOMX_Component::GetState(
        OMX_OUT OMX_STATETYPE* pState)
{
    ALOG_ASSERT(NULL != pState);
    *pState = m_currentState;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_Component::ComponentTunnelRequest(
    OMX_IN  OMX_U32 nPort,
    OMX_IN  OMX_HANDLETYPE hTunneledComp,
    OMX_IN  OMX_U32 nTunneledPort,
    OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
    BSTD_UNUSED(nPort);
    BSTD_UNUSED(hTunneledComp);
    BSTD_UNUSED(nTunneledPort);
    BSTD_UNUSED(pTunnelSetup);
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

OMX_ERRORTYPE BOMX_Component::UseBuffer(
        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_PTR pAppPrivate,
        OMX_IN OMX_U32 nSizeBytes,
        OMX_IN OMX_U8* pBuffer)
{
    BSTD_UNUSED(ppBufferHdr);
    BSTD_UNUSED(nPortIndex);
    BSTD_UNUSED(pAppPrivate);
    BSTD_UNUSED(nSizeBytes);
    BSTD_UNUSED(pBuffer);
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

OMX_ERRORTYPE BOMX_Component::AllocateBuffer(
        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_PTR pAppPrivate,
        OMX_IN OMX_U32 nSizeBytes)
{
    BSTD_UNUSED(ppBuffer);
    BSTD_UNUSED(nPortIndex);
    BSTD_UNUSED(pAppPrivate);
    BSTD_UNUSED(nSizeBytes);
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

OMX_ERRORTYPE BOMX_Component::FreeBuffer(
        OMX_IN  OMX_U32 nPortIndex,
        OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
    BSTD_UNUSED(nPortIndex);
    BSTD_UNUSED(pBuffer);
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

OMX_ERRORTYPE BOMX_Component::EmptyThisBuffer(
        OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
    BSTD_UNUSED(pBuffer);
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

OMX_ERRORTYPE BOMX_Component::FillThisBuffer(
        OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
    BSTD_UNUSED(pBuffer);
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

OMX_ERRORTYPE BOMX_Component::SetCallbacks(
        OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
        OMX_IN  OMX_PTR pAppData)
{
    m_callbacks = *pCallbacks;
    m_pComponentType->pApplicationPrivate = pAppData;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_Component::UseEGLImage(
        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_PTR pAppPrivate,
        OMX_IN void* eglImage)
{
    BSTD_UNUSED(ppBufferHdr);
    BSTD_UNUSED(nPortIndex);
    BSTD_UNUSED(pAppPrivate);
    BSTD_UNUSED(eglImage);
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

OMX_ERRORTYPE BOMX_Component::ComponentRoleEnum(
    OMX_OUT OMX_U8 *cRole,
    OMX_IN OMX_U32 nIndex)
{
    if ( m_pGetRole )
    {
        const char *pRole = m_pGetRole(nIndex);
        if ( NULL == pRole )
        {
            return OMX_ErrorNoMore;
        }
        strncpy((char *)cRole, pRole, OMX_MAX_STRINGNAME_SIZE);
        return OMX_ErrorNone;
    }
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

static void BOMX_Component_CommandEvent(void *pParam)
{
    BOMX_Component *pComponent = (BOMX_Component *)pParam;

//    ALOGV("%s: CommandEvent Awake", pComponent->GetName());
    pComponent->CommandEventHandler();
//    ALOGV("%s: CommandEvent Yield", pComponent->GetName());
}

void BOMX_Component::PortFormatChanged(BOMX_Port *pPort)
{
    if ( m_callbacks.EventHandler )
    {
        unsigned portIndex = pPort->GetIndex();
        ALOGV("%s: OMX_EventPortSettingsChanged Begin", GetName());
        (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate, OMX_EventPortSettingsChanged, portIndex, OMX_IndexParamPortDefinition, NULL);
        ALOGV("%s: OMX_EventPortSettingsChanged End", GetName());
    }
}


void BOMX_Component::CommandEventHandler()
{
    BOMX_CommandMsg msg;
    size_t msgSize;

    while ( B_ERROR_SUCCESS == B_MessageQueue_Wait(m_hCommandQueue, &msg, sizeof(msg), &msgSize, B_WAIT_NONE) )
    {
        OMX_ERRORTYPE err=OMX_ErrorNone;
        bool reply=true;
        bool error=false;
        unsigned i;

        switch ( msg.command )
        {
        case OMX_CommandStateSet:
            {
                OMX_STATETYPE newState = (OMX_STATETYPE)msg.nParam1;
                ALOGV("%s: OMX_CommandStateSet begin", GetName());
                err = StateChangeStart(newState);
                if ( OMX_ErrorNone == err )
                {
                    err = CommandStateSet(newState, m_currentState);
                    if ( OMX_ErrorNone == err )
                    {
                        m_currentState = m_targetState;
                    }
                }
                if ( err != OMX_ErrorNone )
                {
                    // Unwind back to previous state.  Error will be sent below
                    m_targetState = m_currentState;
                }
                ALOGV("%s: OMX_CommandStateSet end %#x", GetName(), err);
            }
            break;
        case OMX_CommandFlush:
            ALOGV("%s: OMX_CommandFlush begin", GetName());
            if ( OMX_ALL == msg.nParam1 )
            {
                reply=false;
                for ( i = 0; i < m_numVideoPorts; i++ )
                {
                    if ( m_pVideoPorts[i]->IsEnabled() )
                    {
                        err = CommandFlush(m_videoPortBase+i);
                        if ( m_callbacks.EventHandler )
                        {
                            (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate,
                                                           OMX_EventCmdComplete, msg.command, m_videoPortBase+i, NULL);
                        }
                        if ( err != OMX_ErrorNone )
                        {
                            error = true;
                        }
                    }
                }
                for ( i = 0; i < m_numAudioPorts; i++ )
                {
                    if ( m_pAudioPorts[i]->IsEnabled() )
                    {
                        err = CommandFlush(m_audioPortBase+i);
                        if ( m_callbacks.EventHandler )
                        {
                            (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate,
                                                           OMX_EventCmdComplete, msg.command, m_audioPortBase+i, NULL);
                        }
                        if ( err != OMX_ErrorNone )
                        {
                            error = true;
                        }
                    }
                }
                for ( i = 0; i < m_numOtherPorts; i++ )
                {
                    if ( m_pOtherPorts[i]->IsEnabled() )
                    {
                        err = CommandFlush(m_otherPortBase+i);
                        if ( m_callbacks.EventHandler )
                        {
                            (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate,
                                                           OMX_EventCmdComplete, msg.command, m_otherPortBase+i, NULL);
                        }
                        if ( err != OMX_ErrorNone )
                        {
                            error = true;
                        }
                    }
                }
                for ( i = 0; i < m_numImagePorts; i++ )
                {
                    if ( m_pImagePorts[i]->IsEnabled() )
                    {
                        err = CommandFlush(m_imagePortBase+i);
                        if ( m_callbacks.EventHandler )
                        {
                            (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate,
                                                           OMX_EventCmdComplete, msg.command, m_imagePortBase+i, NULL);
                        }
                        if ( err != OMX_ErrorNone )
                        {
                            error = true;
                        }
                    }
                }
            }
            else
            {
                err = CommandFlush(msg.nParam1);
            }
            ALOGV("%s: OMX_CommandFlush end", GetName());
            break;
        case OMX_CommandPortEnable:
            ALOGV("%s: OMX_CommandPortEnable begin", GetName());
            if ( OMX_ALL == msg.nParam1 )
            {
                reply=false;
                for ( i = 0; i < m_numVideoPorts; i++ )
                {
                    if ( !m_pVideoPorts[i]->IsEnabled() )
                    {
                        err = CommandPortEnable(m_videoPortBase+i);
                        if ( m_callbacks.EventHandler )
                        {
                            (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate,
                                                           OMX_EventCmdComplete, msg.command, m_videoPortBase+i, NULL);
                        }
                        if ( err != OMX_ErrorNone )
                        {
                            error = true;
                        }
                    }
                }
                for ( i = 0; i < m_numAudioPorts; i++ )
                {
                    if ( !m_pAudioPorts[i]->IsEnabled() )
                    {
                        err = CommandPortEnable(m_audioPortBase+i);
                        if ( m_callbacks.EventHandler )
                        {
                            (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate,
                                                           OMX_EventCmdComplete, msg.command, m_audioPortBase+i, NULL);
                        }
                        if ( err != OMX_ErrorNone )
                        {
                            error = true;
                        }
                    }
                }
                for ( i = 0; i < m_numOtherPorts; i++ )
                {
                    if ( !m_pOtherPorts[i]->IsEnabled() )
                    {
                        err = CommandPortEnable(m_otherPortBase+i);
                        if ( m_callbacks.EventHandler )
                        {
                            (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate,
                                                           OMX_EventCmdComplete, msg.command, m_otherPortBase+i, NULL);
                        }
                        if ( err != OMX_ErrorNone )
                        {
                            error = true;
                        }
                    }
                }
                for ( i = 0; i < m_numImagePorts; i++ )
                {
                    if ( !m_pImagePorts[i]->IsEnabled() )
                    {
                        err = CommandPortEnable(m_imagePortBase+i);
                        if ( m_callbacks.EventHandler )
                        {
                            (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate,
                                                           OMX_EventCmdComplete, msg.command, m_imagePortBase+i, NULL);
                        }
                        if ( err != OMX_ErrorNone )
                        {
                            error = true;
                        }
                    }
                }
            }
            else
            {
                err = CommandPortEnable(msg.nParam1);
            }
            ALOGV("%s: OMX_CommandPortEnable end", GetName());
            break;
        case OMX_CommandPortDisable:
            ALOGV("%s: OMX_CommandPortDisable begin", GetName());
            if ( OMX_ALL == msg.nParam1 )
            {
                reply=false;
                for ( i = 0; i < m_numVideoPorts; i++ )
                {
                    if ( m_pVideoPorts[i]->IsEnabled() )
                    {
                        err = CommandPortDisable(m_videoPortBase+i);
                        if ( m_callbacks.EventHandler )
                        {
                            (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate,
                                                           OMX_EventCmdComplete, msg.command, m_videoPortBase+i, NULL);
                        }
                        if ( err != OMX_ErrorNone )
                        {
                            error = true;
                        }
                    }
                }
                for ( i = 0; i < m_numAudioPorts; i++ )
                {
                    if ( m_pAudioPorts[i]->IsEnabled() )
                    {
                        err = CommandPortDisable(m_audioPortBase+i);
                        if ( m_callbacks.EventHandler )
                        {
                            (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate,
                                                           OMX_EventCmdComplete, msg.command, m_audioPortBase+i, NULL);
                        }
                        if ( err != OMX_ErrorNone )
                        {
                            error = true;
                        }
                    }
                }
                for ( i = 0; i < m_numOtherPorts; i++ )
                {
                    if ( m_pOtherPorts[i]->IsEnabled() )
                    {
                        err = CommandPortDisable(m_otherPortBase+i);
                        if ( m_callbacks.EventHandler )
                        {
                            (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate,
                                                           OMX_EventCmdComplete, msg.command, m_otherPortBase+i, NULL);
                        }
                        if ( err != OMX_ErrorNone )
                        {
                            error = true;
                        }
                    }
                }
                for ( i = 0; i < m_numImagePorts; i++ )
                {
                    if ( m_pImagePorts[i]->IsEnabled() )
                    {
                        err = CommandPortDisable(m_imagePortBase+i);
                        if ( m_callbacks.EventHandler )
                        {
                            (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate,
                                                           OMX_EventCmdComplete, msg.command, m_imagePortBase+i, NULL);
                        }
                        if ( err != OMX_ErrorNone )
                        {
                            error = true;
                        }
                    }
                }
            }
            else
            {
                err = CommandPortDisable(msg.nParam1);
            }
            ALOGV("%s: OMX_CommandPortDisable end", GetName());
            break;
        case OMX_CommandMarkBuffer:
            ALOGV("%s: OMX_CommandMarkBuffer begin", GetName());
            err = CommandMarkBuffer(msg.nParam1, &msg.data.markType);
            ALOGV("%s: OMX_CommandMarkBuffer end", GetName());
            break;
        default:
            err = BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
            break;
        }

        // Report back to application
        if ( m_callbacks.EventHandler )
        {
            Unlock();
            if ( err == OMX_ErrorNone && reply )
            {
                ALOGV("%s: OMX_EventCmdComplete begin", GetName());
                (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate,
                                               OMX_EventCmdComplete, msg.command, msg.nParam1, (msg.command == OMX_CommandMarkBuffer)?(OMX_PTR)&msg.data.markType:NULL);
                ALOGV("%s: OMX_EventCmdComplete end", GetName());
            }
            else if ( err != OMX_ErrorNone || error )
            {
                ALOGV("%s: OMX_EventError begin", GetName());
                (void)m_callbacks.EventHandler((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate,
                                               OMX_EventError, (OMX_U32)err, (OMX_U32)msg.command, (msg.command == OMX_CommandMarkBuffer)?(OMX_PTR)&msg.data.markType:NULL);
                ALOGV("%s: OMX_EventError end", GetName());
            }
            Lock();
        }
    }
}

static OMX_ERRORTYPE BOMX_Component_GetComponentVersion(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_OUT OMX_STRING pComponentName,
    OMX_OUT OMX_VERSIONTYPE* pComponentVersion,
    OMX_OUT OMX_VERSIONTYPE* pSpecVersion,
    OMX_OUT OMX_UUIDTYPE* pComponentUUID)
{
    OMX_ERRORTYPE errorType;
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    ALOGV("%s: GetComponentVersion begin", pComponent->GetName());
    pComponent->Lock();
    errorType = pComponent->GetComponentVersion(pComponentName, pComponentVersion, pSpecVersion, pComponentUUID);
    pComponent->Unlock();
    ALOGV("%s: GetComponentVersion end %#x", pComponent->GetName(), errorType);

    return errorType;
}

static OMX_ERRORTYPE BOMX_Component_SendCommand(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_COMMANDTYPE Cmd,
    OMX_IN  OMX_U32 nParam1,
    OMX_IN  OMX_PTR pCmdData)
{
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    // Commands are queued per the OMX Spec
    BOMX_CommandMsg msg;
    msg.command = Cmd;
    msg.nParam1 = nParam1;
    if ( Cmd == OMX_CommandMarkBuffer )
    {
        ALOG_ASSERT(NULL != pCmdData);
        memcpy((void *)&msg.data.markType, pCmdData, sizeof(OMX_MARKTYPE));
    }

    ALOGV("%s: SendCommand %u", pComponent->GetName(), Cmd);
    return pComponent->QueueCommand(&msg);
}

static OMX_ERRORTYPE BOMX_Component_GetParameter(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nParamIndex,
    OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE errorType;
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    ALOGV("%s: GetParameter begin", pComponent->GetName());
    pComponent->Lock();
    errorType = pComponent->GetParameter(nParamIndex, pComponentParameterStructure);
    pComponent->Unlock();
    ALOGV("%s: GetParameter end %#x", pComponent->GetName(), errorType);

    return errorType;
}

static OMX_ERRORTYPE BOMX_Component_SetParameter(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nIndex,
    OMX_IN  OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE errorType;
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    ALOGV("%s: SetParameter begin", pComponent->GetName());
    pComponent->Lock();
    errorType = pComponent->SetParameter(nIndex, pComponentParameterStructure);
    pComponent->Unlock();
    ALOGV("%s: SetParameter end %#x", pComponent->GetName(), errorType);

    return errorType;
}

static OMX_ERRORTYPE BOMX_Component_GetConfig(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nIndex,
    OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE errorType;
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    ALOGV("%s: GetConfig begin", pComponent->GetName());
    pComponent->Lock();
    errorType = pComponent->GetConfig(nIndex, pComponentConfigStructure);
    pComponent->Unlock();
    ALOGV("%s: GetConfig end %#x", pComponent->GetName(), errorType);

    return errorType;
}

static OMX_ERRORTYPE BOMX_Component_SetConfig(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nIndex,
    OMX_IN  OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE errorType;
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    ALOGV("%s: SetConfig begin", pComponent->GetName());
    pComponent->Lock();
    errorType = pComponent->SetConfig(nIndex, pComponentConfigStructure);
    pComponent->Unlock();
    ALOGV("%s: SetConfig end %#x", pComponent->GetName(), errorType);

    return errorType;
}

static OMX_ERRORTYPE BOMX_Component_GetExtensionIndex(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_STRING cParameterName,
    OMX_OUT OMX_INDEXTYPE* pIndexType)
{
    OMX_ERRORTYPE errorType;
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    ALOGV("%s: GetExtensionIndex begin", pComponent->GetName());
    pComponent->Lock();
    errorType = pComponent->GetExtensionIndex(cParameterName, pIndexType);
    pComponent->Unlock();
    ALOGV("%s: GetExtensionIndex end %#x", pComponent->GetName(), errorType);

    return errorType;
}

static OMX_ERRORTYPE BOMX_Component_GetState(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_OUT OMX_STATETYPE* pState)
{
    OMX_ERRORTYPE errorType;
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    ALOGV("%s: GetState begin", pComponent->GetName());
    pComponent->Lock();
    errorType = pComponent->GetState(pState);
    pComponent->Unlock();
    ALOGV("%s: GetState end %#x", pComponent->GetName(), errorType);

    return errorType;
}

static OMX_ERRORTYPE BOMX_Component_ComponentTunnelRequest(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_U32 nPort,
    OMX_IN  OMX_HANDLETYPE hTunneledComp,
    OMX_IN  OMX_U32 nTunneledPort,
    OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
    OMX_ERRORTYPE errorType;
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    ALOGV("%s: ComponentTunnelRequest begin", pComponent->GetName());
    pComponent->Lock();
    errorType = pComponent->ComponentTunnelRequest(nPort, hTunneledComp, nTunneledPort, pTunnelSetup);
    pComponent->Unlock();
    ALOGV("%s: ComponentTunnelRequest end %#x", pComponent->GetName(), errorType);

    return errorType;
}

static OMX_ERRORTYPE BOMX_Component_UseBuffer(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes,
    OMX_IN OMX_U8* pBuffer)
{
    OMX_ERRORTYPE errorType;
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    ALOGV("%s: UseBuffer begin", pComponent->GetName());
    pComponent->Lock();
    errorType = pComponent->UseBuffer(ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, pBuffer);
    pComponent->Unlock();
    ALOGV("%s: UseBuffer end %#x", pComponent->GetName(), errorType);

    return errorType;
}

static OMX_ERRORTYPE BOMX_Component_AllocateBuffer(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes)
{
    OMX_ERRORTYPE errorType;
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    ALOGV("%s: AllocateBuffer begin", pComponent->GetName());
    pComponent->Lock();
    errorType = pComponent->AllocateBuffer(ppBuffer, nPortIndex, pAppPrivate, nSizeBytes);
    pComponent->Unlock();
    ALOGV("%s: AllocateBuffer end %#x", pComponent->GetName(), errorType);

    return errorType;
}

static OMX_ERRORTYPE BOMX_Component_FreeBuffer(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_U32 nPortIndex,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
    OMX_ERRORTYPE errorType;
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    ALOGV("%s: FreeBuffer begin", pComponent->GetName());
    pComponent->Lock();
    errorType = pComponent->FreeBuffer(nPortIndex, pBuffer);
    pComponent->Unlock();
    ALOGV("%s: FreeBuffer end %#x", pComponent->GetName(), errorType);

    return errorType;
}

static OMX_ERRORTYPE BOMX_Component_EmptyThisBuffer(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
    OMX_ERRORTYPE errorType;
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    ALOGV("%s: EmptyThisBuffer begin", pComponent->GetName());
    pComponent->Lock();
    errorType = pComponent->EmptyThisBuffer(pBuffer);
    pComponent->Unlock();
    ALOGV("%s: EmptyThisBuffer end %#x", pComponent->GetName(), errorType);

    return errorType;
}

static OMX_ERRORTYPE BOMX_Component_FillThisBuffer(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
    OMX_ERRORTYPE errorType;
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    ALOGV("%s: FillThisBuffer begin", pComponent->GetName());
    pComponent->Lock();
    errorType = pComponent->FillThisBuffer(pBuffer);
    pComponent->Unlock();
    ALOGV("%s: FillThisBuffer end %#x", pComponent->GetName(), errorType);

    return errorType;
}


static OMX_ERRORTYPE BOMX_Component_SetCallbacks(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
    OMX_IN  OMX_PTR pAppData)
{
    OMX_ERRORTYPE errorType;
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    ALOGV("%s: SetCallbacks begin", pComponent->GetName());
    pComponent->Lock();
    errorType = pComponent->SetCallbacks(pCallbacks, pAppData);
    pComponent->Unlock();
    ALOGV("%s: SetCallbacks end %#x", pComponent->GetName(), errorType);

    return errorType;
}


static OMX_ERRORTYPE BOMX_Component_ComponentDeInit(
    OMX_IN  OMX_HANDLETYPE hComponent)
{
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    delete pComponent;

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE BOMX_Component_UseEGLImage(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN void* eglImage)
{
    OMX_ERRORTYPE errorType;
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    ALOGV("%s: UseEGLImage begin", pComponent->GetName());
    pComponent->Lock();
    errorType = pComponent->UseEGLImage(ppBufferHdr, nPortIndex, pAppPrivate, eglImage);
    pComponent->Unlock();
    ALOGV("%s: UseEGLImage end %#x", pComponent->GetName(), errorType);

    return errorType;
}

static OMX_ERRORTYPE BOMX_Component_ComponentRoleEnum(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_OUT OMX_U8 *cRole,
    OMX_IN OMX_U32 nIndex)
{
    OMX_ERRORTYPE errorType;
    OMX_COMPONENTTYPE *pComponentType = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_Component *pComponent = (BOMX_Component *)pComponentType->pComponentPrivate;

    ALOGV("%s: ComponentRoleEnum begin", pComponent->GetName());
    pComponent->Lock();
    errorType = pComponent->ComponentRoleEnum(cRole, nIndex);
    pComponent->Unlock();
    ALOGV("%s: ComponentRoleEnum end %#x", pComponent->GetName(), errorType);

    return errorType;
}

OMX_ERRORTYPE BOMX_Component::QueueCommand(const BOMX_CommandMsg *pMsg)
{
    B_Error err = B_MessageQueue_Post(m_hCommandQueue, (const void *)pMsg, sizeof(BOMX_CommandMsg));
    if ( err )
    {
        return BOMX_ERR_TRACE(OMX_ErrorTimeout);
    }

    B_Event_Set(m_hCommandEvent);
    return OMX_ErrorNone;
}

void BOMX_Component::WalkTunnels(BOMX_ComponentArray *pArray)
{
    unsigned i;

    for ( i = 0; i < pArray->numComponents; i++ )
    {
        if ( pArray->pComponents[i] == this )
        {
            ALOGV("Component %p (%s) already in array", this, GetName());
            return;
        }
    }
    ALOGV("Component %p (%s) not in array", this, GetName());

    if ( pArray->numComponents >= BOMX_MAX_COMPONENTS_IN_ARRAY )
    {
        ALOGW("Component Array Full");
        return;
    }

    pArray->pComponents[pArray->numComponents] = this;
    pArray->numComponents++;

    for ( i = 0; i < m_numVideoPorts; i++ )
    {
        BOMX_Component *pTunnel = m_pVideoPorts[i]->GetTunnelComponent();
        if ( pTunnel )
        {
            pTunnel->WalkTunnels(pArray);
        }
    }
    for ( i = 0; i < m_numAudioPorts; i++ )
    {
        BOMX_Component *pTunnel = m_pAudioPorts[i]->GetTunnelComponent();
        if ( pTunnel )
        {
            pTunnel->WalkTunnels(pArray);
        }
    }
    for ( i = 0; i < m_numImagePorts; i++ )
    {
        BOMX_Component *pTunnel = m_pImagePorts[i]->GetTunnelComponent();
        if ( pTunnel )
        {
            pTunnel->WalkTunnels(pArray);
        }
    }
    for ( i = 0; i < m_numOtherPorts; i++ )
    {
        BOMX_Component *pTunnel = m_pOtherPorts[i]->GetTunnelComponent();
        if ( pTunnel )
        {
            pTunnel->WalkTunnels(pArray);
        }
    }
}

NEXUS_Error BOMX_Component::AllocateResources()
{
    unsigned i;
    NEXUS_Error errCode = NEXUS_SUCCESS;

    // Build component map
    BOMX_ComponentArray compArray;
    memset(&compArray, 0, sizeof(compArray));
    this->WalkTunnels(&compArray);

    BOMX_ComponentResources *pRes;
    B_Mutex_Lock(g_resourceMutex);
    for ( pRes = BLST_Q_FIRST(&g_resourceList);
          NULL != pRes;
          pRes = BLST_Q_NEXT(pRes, node) )
    {
        unsigned numMatches=0, j;
        if ( pRes->componentArray.numComponents != compArray.numComponents )
        {
            continue;
        }
        for ( i = 0; i < compArray.numComponents; i++ )
        {
            for ( j = 0; j < compArray.numComponents; j++ )
            {
                if ( compArray.pComponents[i] == pRes->componentArray.pComponents[j] )
                {
                    numMatches++;
                    break;
                }
            }
        }
        if ( numMatches == compArray.numComponents )
        {
            // Found a match
            break;
        }
    }
    if ( NULL == pRes )
    {
        pRes = new BOMX_ComponentResources;
        if ( pRes )
        {
            memset(pRes, 0, sizeof(BOMX_ComponentResources));
            pRes->componentArray = compArray;
            /* TODO: This was required for nxclient w/tunneling */
            BLST_Q_INSERT_HEAD(&g_resourceList, pRes, node);
        }
    }

    if ( pRes )
    {
        for ( i = 0; i < pRes->componentArray.numComponents; i++ )
        {
            if ( pRes->componentArray.pComponents[i] == this )
            {
                ALOG_ASSERT(pRes->allocated[i] == false);
                pRes->allocated[i] = true;
                this->m_pResources = pRes;
                break;
            }
        }
        ALOG_ASSERT(i < pRes->componentArray.numComponents);
    }
    B_Mutex_Unlock(g_resourceMutex);

    return errCode;
}

void BOMX_Component::ReleaseResources()
{
    B_Mutex_Lock(g_resourceMutex);
    if ( m_pResources )
    {
        unsigned i;
        bool active=false;
        BOMX_ComponentResources *pRes = m_pResources;
        m_pResources = NULL;
        for ( i = 0; i < pRes->componentArray.numComponents; i++ )
        {
            if ( pRes->componentArray.pComponents[i] == this )
            {
                ALOG_ASSERT(pRes->allocated[i] == true);
                pRes->allocated[i] = false;
                break;
            }
        }
        ALOG_ASSERT(i < pRes->componentArray.numComponents);
        for ( i = 0; i < pRes->componentArray.numComponents; i++ )
        {
            if ( pRes->allocated[i] )
            {
                active = true;
                break;
            }
        }
        if ( !active )
        {
            /* TODO: This was required for nxclient w/tunneling */
            BLST_Q_REMOVE(&g_resourceList, pRes, node);
            delete pRes;
        }
    }
    B_Mutex_Unlock(g_resourceMutex);
}

B_Error BOMX_Component::PortWait()
{
    B_Error rc;
    B_Time now;
    unsigned timeout;

    ALOG_ASSERT(m_portWaitTimeout > 0);

    B_Time_Get(&now);
    timeout = B_Time_Diff(&now, &m_portWaitStartTime);

    // Allow non-task events to occur during the wait
    Unlock();
    if ( timeout > m_portWaitTimeout )
    {
        timeout = 0;
    }
    else
    {
        timeout = m_portWaitTimeout - timeout;
    }
    rc = B_Event_Wait(m_hPortEvent, timeout);
    Lock();

    return rc;
}

OMX_ERRORTYPE BOMX_Component::CommandMarkBuffer(
    OMX_U32 portIndex,
    const OMX_MARKTYPE *pMarkType)
{
    BSTD_UNUSED(portIndex);
    BSTD_UNUSED(pMarkType);
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

void BOMX_Component::ReturnPortBuffers(BOMX_Port *pPort)
{
    BOMX_Buffer *pBuffer;

    ALOG_ASSERT(NULL != pPort);

    while ( (pBuffer = pPort->GetBuffer()) )
    {
        ReturnPortBuffer(pPort, pBuffer);
    }
}

void BOMX_Component::ReturnPortBuffer(BOMX_Port *pPort, BOMX_Buffer *pBuffer)
{
    ALOG_ASSERT(NULL != pPort);

    pPort->BufferComplete(pBuffer);
    if ( !pPort->IsTunneled() )
    {
        if ( pPort->GetDir() == OMX_DirInput )
        {
            if ( NULL != m_callbacks.EmptyBufferDone )
            {
                m_callbacks.EmptyBufferDone((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate, pBuffer->GetHeader());
            }
        }
        else
        {
            if ( NULL != m_callbacks.FillBufferDone )
            {
                m_callbacks.FillBufferDone((OMX_HANDLETYPE)m_pComponentType, m_pComponentType->pApplicationPrivate, pBuffer->GetHeader());
            }
        }
    }
}

void BOMX_Component::ShutdownScheduler()
{
    if ( !m_schedulerStopped )
    {
        B_Error rc;

        B_Scheduler_Stop(m_hScheduler);
        m_schedulerStopped = true;
        rc = B_Event_Wait(m_hExitEvent, B_THREAD_EXIT_TIMEOUT);
        if ( rc )
        {
            ALOGE("Timed out waiting for component thread to finish (%d)!", rc);
        }
    }
}
