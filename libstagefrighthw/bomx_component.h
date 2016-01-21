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
#ifndef BOMX_COMPONENT_H__
#define BOMX_COMPONENT_H__

#include "OMX_Component.h"
#include "bomx_utils.h"
#include "bomx_port.h"
#include "b_os_lib.h"
#include "bfifo.h"
#include "nexus_ipc_client_factory.h"
#include "nexus_simple_stc_channel.h"

#define BOMX_MAX_ROLES_PER_COMPONENT (16)
#define BOMX_COMPONENT_MAX_MSGS (16)
#define BOMX_COMPONENT_MAX_PORTS (16)
#define BOMX_MAX_COMPONENTS_IN_ARRAY (16)

enum BOMX_ComponentId
{
    BOMX_ComponentId_eClock,
    BOMX_ComponentId_eVideoDecoder,
    BOMX_ComponentId_eVideoScheduler,
    BOMX_ComponentId_eVideoRenderer,
    BOMX_ComponentId_eAudioDecoder,
    BOMX_ComponentId_eAudioRenderer,
    BOMX_ComponentId_eMax
};

#define BOMX_COMPONENT_PORT_BASE(compId, domain) ( (1000*(unsigned)(compId)) + (100 * (unsigned)(domain)) )
#define BOMX_HANDLE_TO_COMPONENT(pHandle) (static_cast <BOMX_Component *> ((static_cast <OMX_COMPONENTTYPE*> (pHandle))->pComponentPrivate))

struct BOMX_CommandMsg
{
    OMX_COMMANDTYPE command;
    OMX_U32 nParam1;
    union {
        OMX_MARKTYPE markType;
    } data;
};

class BOMX_Component;

struct BOMX_ComponentArray
{
    unsigned numComponents;
    BOMX_Component *pComponents[BOMX_MAX_COMPONENTS_IN_ARRAY];
};

struct BOMX_ComponentResources
{
    BLST_Q_ENTRY(BOMX_ComponentResources) node;
    BOMX_ComponentArray componentArray;
    unsigned connectId;
    bool allocated[BOMX_MAX_COMPONENTS_IN_ARRAY];
#if 0 /* Only required for tunneled mode */    
    NxClient_AllocSettings allocSettings;
    NxClient_AllocResults allocResults;
    NxClient_ConnectSettings connectSettings;
#endif    
};

OMX_ERRORTYPE BOMX_InitComponentResourceList();
void BOMX_UninitComponentResourceList();

class BOMX_Component
{
public:
    BOMX_Component(
        OMX_COMPONENTTYPE *pComponentType,
        const OMX_STRING pName,
        const OMX_PTR pAppData,
        const OMX_CALLBACKTYPE *pCallbacks,
        const char *(*pGetRole)(unsigned roleIndex));

    virtual ~BOMX_Component();

    OMX_ERRORTYPE QueueCommand(const BOMX_CommandMsg *pMsg);
    void CommandEventHandler();

    void RunScheduler() {
                B_Scheduler_Run(m_hScheduler);
                B_Event_Set(m_hExitEvent); }
    void Lock() { B_Mutex_Lock(m_hMutex); };
    void Unlock() { B_Mutex_Unlock(m_hMutex); };

    bool IsValid() { return m_currentState == OMX_StateInvalid ? false : true; }

    // Component Name
    const char *GetName() const {return m_componentName;}

    // Find Port by Index
    BOMX_Port *FindPortByIndex(unsigned index);

    // Begin OMX Standard functions.
    virtual OMX_ERRORTYPE GetComponentVersion(
            OMX_OUT OMX_STRING pComponentName,
            OMX_OUT OMX_VERSIONTYPE* pComponentVersion,
            OMX_OUT OMX_VERSIONTYPE* pSpecVersion,
            OMX_OUT OMX_UUIDTYPE* pComponentUUID);

    virtual OMX_ERRORTYPE GetParameter(
            OMX_IN  OMX_INDEXTYPE nParamIndex,
            OMX_INOUT OMX_PTR pComponentParameterStructure);

    virtual OMX_ERRORTYPE SetParameter(
            OMX_IN  OMX_INDEXTYPE nIndex,
            OMX_IN  OMX_PTR pComponentParameterStructure);

    virtual OMX_ERRORTYPE GetConfig(
            OMX_IN  OMX_INDEXTYPE nIndex,
            OMX_INOUT OMX_PTR pComponentConfigStructure);

    virtual OMX_ERRORTYPE SetConfig(
            OMX_IN  OMX_INDEXTYPE nIndex,
            OMX_IN  OMX_PTR pComponentConfigStructure);

    virtual OMX_ERRORTYPE GetExtensionIndex(
            OMX_IN  OMX_STRING cParameterName,
            OMX_OUT OMX_INDEXTYPE* pIndexType);

    virtual OMX_ERRORTYPE GetState(
            OMX_OUT OMX_STATETYPE* pState);

    virtual OMX_ERRORTYPE ComponentTunnelRequest(
        OMX_IN  OMX_U32 nPort,
        OMX_IN  OMX_HANDLETYPE hTunneledComp,
        OMX_IN  OMX_U32 nTunneledPort,
        OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup);

    virtual OMX_ERRORTYPE UseBuffer(
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes,
            OMX_IN OMX_U8* pBuffer);

    virtual OMX_ERRORTYPE AllocateBuffer(
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes);

    virtual OMX_ERRORTYPE FreeBuffer(
            OMX_IN  OMX_U32 nPortIndex,
            OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

    virtual OMX_ERRORTYPE EmptyThisBuffer(
            OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

    virtual OMX_ERRORTYPE FillThisBuffer(
            OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

    virtual OMX_ERRORTYPE SetCallbacks(
            OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
            OMX_IN  OMX_PTR pAppData);

    virtual OMX_ERRORTYPE UseEGLImage(
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN void* eglImage);

    virtual OMX_ERRORTYPE ComponentRoleEnum(
        OMX_OUT OMX_U8 *cRole,
        OMX_IN OMX_U32 nIndex);
    // End OMX Standard functions.

    // Begin OMX Command Handlers - These functions should block until completion.  The return code
    // will take care of sending the correct callback to the client.
    // All commands must be implemented except for MarkBuffer, which is optional.
    virtual OMX_ERRORTYPE CommandStateSet(
        OMX_STATETYPE newState,
        OMX_STATETYPE oldState) = 0;

    virtual OMX_ERRORTYPE CommandFlush(
        OMX_U32 portIndex) = 0;

    virtual OMX_ERRORTYPE CommandPortEnable(
        OMX_U32 portIndex) = 0;
    virtual OMX_ERRORTYPE CommandPortDisable(
        OMX_U32 portIndex) = 0;

    // MarkBuffer is optional.  If not implemented, it will return OMX_ErrorNotImplemented.
    virtual OMX_ERRORTYPE CommandMarkBuffer(
        OMX_U32 portIndex,
        const OMX_MARKTYPE *pMarkType);
    // End OMX Command Handlers

    virtual void GetMediaTime(OMX_TICKS *pTicks) {BDBG_ASSERT(NULL != pTicks); *pTicks = 0;}
    virtual NEXUS_SimpleStcChannelHandle GetStcChannel() {return NULL;}

    void HandleEOS() {B_Event_Set(m_hEosEvent);}
    void PortFormatChanged(BOMX_Port *pPort);

    // Handle EOS event internally - do not call outside component class
    void Eos();
protected:
    OMX_COMPONENTTYPE *m_pComponentType;
    OMX_CALLBACKTYPE m_callbacks;
    bool m_schedulerStopped;
    uint32_t m_version;
    unsigned m_numAudioPorts, m_audioPortBase;
    unsigned m_numImagePorts, m_imagePortBase;
    unsigned m_numVideoPorts, m_videoPortBase;
    unsigned m_numOtherPorts, m_otherPortBase;
    BOMX_AudioPort *m_pAudioPorts[BOMX_COMPONENT_MAX_PORTS];
    BOMX_ImagePort *m_pImagePorts[BOMX_COMPONENT_MAX_PORTS];
    BOMX_VideoPort *m_pVideoPorts[BOMX_COMPONENT_MAX_PORTS];
    BOMX_OtherPort *m_pOtherPorts[BOMX_COMPONENT_MAX_PORTS];

    // If a constructor fails, mark as invalid
    void Invalidate() { m_currentState = m_targetState = OMX_StateInvalid; };

    // State Management Routines
    OMX_ERRORTYPE StateChangeStart(OMX_STATETYPE newState);
    bool StateChangeInProgress() { return (m_currentState != m_targetState)?true:false; }
    bool StateChangeInProgress(OMX_STATETYPE *pOldState, OMX_STATETYPE *pNewState);
    OMX_STATETYPE StateGet() const { return m_currentState; };

    // NxClient Resources
    NEXUS_Error AllocateResources();
    void ReleaseResources();
#if 0 /* Only required for tunneled mode */    
    virtual void GetNxClientAllocSettings(NxClient_AllocSettings *pSettings);
    virtual void GetNxClientConnectSettings(NxClient_ConnectSettings *pSettings);

    NxClient_AllocSettings m_nxClientAllocSettings;
    NxClient_AllocResults m_nxClientAllocResults;
#endif    
    BOMX_ComponentResources *m_pResources;

    // Event Routines
    B_SchedulerEventId RegisterEvent(
        B_EventHandle event,                        /* Event that will trigger the callback */
        B_EventCallback callback,                   /* Callback routine to execute when event is set */
        void *pContext                              /* Value passed to callback routine */
        ) { return B_Scheduler_RegisterEvent(m_hScheduler, m_hMutex, event, callback, pContext); }

    void UnregisterEvent(
        B_SchedulerEventId id
        ) { B_Scheduler_UnregisterEvent(m_hScheduler, id); }

    // Timer Routnes
    B_SchedulerTimerId StartTimer(
        int timeoutMsec,                        /* Timer expiration time in msec */
        B_TimerCallback callback,               /* Callback to call when timer expires */
        void *pContext                          /* Value passed to callback routine */
        ) { return B_Scheduler_StartTimer(m_hScheduler, m_hMutex, timeoutMsec, callback, pContext); }

    // Timer Routnes
    void CancelTimer(
        B_SchedulerTimerId id
        ) { B_Scheduler_CancelTimer(m_hScheduler, id); }

    // Shut down the sceduler - should be called in a destructor for a component class
    void ShutdownScheduler();

    // Component Role
    void SetRole(const char *pNewRole) { strncpy(m_componentRole, pNewRole, OMX_MAX_STRINGNAME_SIZE-1); m_componentRole[OMX_MAX_STRINGNAME_SIZE-1] = '\0'; }
    void GetRole(char *pRole) { strncpy(pRole, m_componentRole, OMX_MAX_STRINGNAME_SIZE); }

    // Walk through tunneled components
    void WalkTunnels(BOMX_ComponentArray *pArray);

    // Indicate Port Status Has Changed
    void PortStatusChanged() {B_Event_Set(m_hPortEvent);}
    void PortWaitBegin(unsigned timeout=3000) {BDBG_ASSERT(m_portWaitTimeout==0); BDBG_ASSERT(timeout > 0); m_portWaitTimeout=timeout; B_Time_Get(&m_portWaitStartTime); }
    B_Error PortWait();
    void PortWaitEnd() {BDBG_ASSERT(m_portWaitTimeout>0); m_portWaitTimeout=0;}

    // Return port buffers on a flush or disable
    void ReturnPortBuffers(BOMX_Port *pPort);
    // Return port buffer to client
    void ReturnPortBuffer(BOMX_Port *pPort, BOMX_Buffer *pBuffer);

private:
    OMX_STATETYPE m_currentState, m_targetState;
    char m_componentName[OMX_MAX_STRINGNAME_SIZE];
    char m_componentRole[OMX_MAX_STRINGNAME_SIZE];
    char m_uuid[36];
    B_SchedulerHandle m_hScheduler;
    B_MutexHandle m_hMutex;
    B_EventHandle m_hCommandEvent;
    B_EventHandle m_hPortEvent;
    B_EventHandle m_hEosEvent;
    B_EventHandle m_hExitEvent;
    B_ThreadHandle m_hThread;
    B_SchedulerEventId m_commandEventId;
    B_SchedulerEventId m_eosEventId;
    B_MessageQueueHandle m_hCommandQueue;
    B_Time m_portWaitStartTime;
    unsigned m_portWaitTimeout;
    const char *(*m_pGetRole)(unsigned roleIndex);
};

#endif //BOMX_COMPONENT_H__
