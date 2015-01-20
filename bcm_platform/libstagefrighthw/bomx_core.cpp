/***************************************************************************
 *     Copyright (c) 2002-2012, Broadcom Corporation
 *     All Rights Reserved
 *     Confidential Property of Broadcom Corporation
 *
 *  THIS SOFTWARE MAY ONLY BE USED SUBJECT TO AN EXECUTED SOFTWARE LICENSE
 *  AGREEMENT  BETWEEN THE USER AND BROADCOM.  YOU HAVE NO RIGHT TO USE OR
 *  EXPLOIT THIS MATERIAL EXCEPT SUBJECT TO THE TERMS OF SUCH AN AGREEMENT.
 *
 * Module Description:
 *
 * Revision History:
 *
 *************************************************************************/
#undef LOG_TAG
//#define LOG_NDEBUG 0
#define LOG_TAG "bomx_core"
#include <cutils/log.h>

#include "OMX_Core.h"
#include "OMX_Component.h"
#include "OMX_Types.h"
#include "nexus_platform.h"
#include "bdbg.h"
#include "bkni.h"
#include <string.h>
#include <stdlib.h>
#include "bomx_component.h"
#include "bomx_video_decoder.h"
#include "bomx_audio_decoder.h"
#ifdef SECURE_DECODER_ON
#include "bomx_video_decoder_secure.h"
#endif

// Table of known components, constructors, and roles
static const struct ComponentEntry
{
    const char *pName;
    OMX_ERRORTYPE (*pConstructor)(OMX_COMPONENTTYPE *, OMX_IN OMX_STRING, OMX_IN OMX_PTR, OMX_IN OMX_CALLBACKTYPE*);
    const char *(*pGetRole)(unsigned index);
} g_components[] =
{
    {"OMX.broadcom.video_decoder", BOMX_VideoDecoder_Create, BOMX_VideoDecoder_GetRole},
#if 0 /* Enable when audio is ready */    
    {"OMX.broadcom.audio_decoder", BOMX_AudioDecoder_Create, BOMX_AudioDecoder_GetRole},
#endif
// Use a macro for now. Need to find a better way to avoid using macros in cpp code
#ifdef SECURE_DECODER_ON
    {"OMX.broadcom.video_decoder.secure", BOMX_VideoDecoder_Secure_Create, BOMX_VideoDecoder_Secure_GetRole},
#endif

};

// Max number of components available
static const int g_numComponents = sizeof(g_components)/sizeof(ComponentEntry);

// Lookup a component entry by name
static const ComponentEntry *GetComponentByName(const char *pName)
{
    int i;
    for ( i = 0; i < g_numComponents; i++ )
    {
        if ( 0 == strcmp(pName, g_components[i].pName) )
        {
            return g_components+i;
        }
    }
    // Not found
    return NULL;
}

static pthread_mutex_t g_initMutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned g_refCount=0;

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Init(void)
{
    BERR_Code errCode;
    B_Error berr;
    NEXUS_Error nerr;
    OMX_ERRORTYPE err;

    ALOGI("OMX_Init");

    pthread_mutex_lock(&g_initMutex);
    if ( ++g_refCount > 1 )
    {  
        ALOGI("Nested call to OMX_Init.  Refcount is now %u", g_refCount);
        pthread_mutex_unlock(&g_initMutex);
        return OMX_ErrorNone;
    }

    berr = B_Os_Init();
    if ( berr )
    {
        goto err_oslib;
    }

    err = BOMX_InitComponentResourceList();
    if ( err != OMX_ErrorNone )
    {
        goto err_resources;
    }

    pthread_mutex_unlock(&g_initMutex);
    return OMX_ErrorNone;

err_resources:
    if ( 1 == g_refCount )
    {
        B_Os_Uninit();
    }
err_oslib:
    --g_refCount;
    pthread_mutex_unlock(&g_initMutex);
    return BOMX_ERR_TRACE(OMX_ErrorUndefined);
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Deinit(void)
{
    ALOGI("OMX_Deinit");
    pthread_mutex_lock(&g_initMutex);
    if ( g_refCount == 1 )
    {
        BOMX_UninitComponentResourceList();
        B_Os_Uninit();
        g_refCount=0;
    }
    else
    {
        g_refCount--;
        ALOGI("Nested call to OMX_Deinit.  Refcount is now %u", g_refCount);
    }
    pthread_mutex_unlock(&g_initMutex);
    ALOGI("OMX_Deinit  Complete");
    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_ComponentNameEnum(
    OMX_OUT OMX_STRING cComponentName,
    OMX_IN  OMX_U32 nNameLength,
    OMX_IN  OMX_U32 nIndex)
{
    if ( NULL == cComponentName || nNameLength == 0 )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    else if ( (int)nIndex >= g_numComponents )
    {
        return OMX_ErrorNoMore;
    }
    strncpy(cComponentName, g_components[nIndex].pName, nNameLength);
    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_GetHandle(
    OMX_OUT OMX_HANDLETYPE* pHandle,
    OMX_IN  OMX_STRING cComponentName,
    OMX_IN  OMX_PTR pAppData,
    OMX_IN  OMX_CALLBACKTYPE* pCallBacks)
{
    if ( NULL == pHandle || NULL == cComponentName || NULL == pCallBacks )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    const ComponentEntry *pComponentEntry = GetComponentByName(cComponentName);
    if ( NULL == pComponentEntry )
    {
        BOMX_ERR(("Unable to find component '%s'", cComponentName));
        return BOMX_ERR_TRACE(OMX_ErrorComponentNotFound);
    }
    if ( NULL == pComponentEntry->pConstructor )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInvalidComponent);
    }
    // Alloc component structure
    OMX_COMPONENTTYPE *pComponent = (OMX_COMPONENTTYPE *)BKNI_Malloc(sizeof(OMX_COMPONENTTYPE));
    if ( NULL == pComponent )
    {
        return BOMX_ERR_TRACE(OMX_ErrorInsufficientResources);
    }
    // Init structure
    BKNI_Memset(pComponent, 0, sizeof(OMX_COMPONENTTYPE));
    pComponent->nSize = sizeof(OMX_COMPONENTTYPE);
    pComponent->nVersion.s.nVersionMajor = 1;
    pComponent->nVersion.s.nVersionMinor = 0;
    pComponent->nVersion.s.nRevision = 0;
    pComponent->nVersion.s.nStep = 0;
    // Invoke constructor to do the rest
    OMX_ERRORTYPE err = pComponentEntry->pConstructor(pComponent, cComponentName, pAppData, pCallBacks);
    if ( err != OMX_ErrorNone )
    {
        BKNI_Memset(pComponent, 0, sizeof(OMX_COMPONENTTYPE));
        BKNI_Free(pComponent);
        return err;
    }
    *pHandle = pComponent;
    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_FreeHandle(
    OMX_IN  OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pComponent = (OMX_COMPONENTTYPE *)hComponent;
    BOMX_ASSERT(NULL != pComponent);
    BOMX_ASSERT(pComponent->nSize == sizeof(OMX_COMPONENTTYPE));
    if ( NULL != pComponent->ComponentDeInit )
    {
        err = pComponent->ComponentDeInit(hComponent);
    }
    BKNI_Memset(hComponent, 0, sizeof(OMX_COMPONENTTYPE));
    BKNI_Free(hComponent);
    return err;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_SetupTunnel(
    OMX_IN  OMX_HANDLETYPE hOutput,
    OMX_IN  OMX_U32 nPortOutput,
    OMX_IN  OMX_HANDLETYPE hInput,
    OMX_IN  OMX_U32 nPortInput)
{
    OMX_ERRORTYPE err = OMX_ErrorTunnelingUnsupported;
    OMX_TUNNELSETUPTYPE tunnelSetup;
    OMX_COMPONENTTYPE *pOutputComponent = (OMX_COMPONENTTYPE *)hOutput;
    OMX_COMPONENTTYPE *pInputComponent = (OMX_COMPONENTTYPE *)hInput;

    memset(&tunnelSetup, 0, sizeof(tunnelSetup));
    if ( NULL == hOutput && NULL == hInput )
    {
        BOMX_ERR(("At least one component must be passed to OMX_SetupTunnel"));
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    if ( hOutput && hInput )
    {
        // Establishing tunnel
        if ( NULL == pOutputComponent->ComponentTunnelRequest )
        {
            BOMX_ERR(("Output component does not support tunneling"));
            return BOMX_ERR_TRACE(OMX_ErrorTunnelingUnsupported);
        }
        if ( NULL == pInputComponent->ComponentTunnelRequest )
        {
            BOMX_ERR(("Input component does not support tunneling"));
            return BOMX_ERR_TRACE(OMX_ErrorTunnelingUnsupported);
        }
        err = pOutputComponent->ComponentTunnelRequest(hOutput, nPortOutput, hInput, nPortInput, &tunnelSetup);
        if ( err )
        {
            BOMX_ERR(("Output Tunnel Request Failed"));
            return err;
        }
        err = pInputComponent->ComponentTunnelRequest(hInput, nPortInput, hOutput, nPortOutput, &tunnelSetup);
        if ( err )
        {
            BOMX_ERR(("Input Tunnel Request Failed"));
            memset(&tunnelSetup, 0, sizeof(tunnelSetup));
            (void)pOutputComponent->ComponentTunnelRequest(hOutput, nPortOutput, NULL, 0, &tunnelSetup);
            return err;
        }
    }
    else if ( hOutput )
    {
        // Tearing down tunnel from output port
        if ( NULL == pOutputComponent->ComponentTunnelRequest )
        {
            BOMX_ERR(("Output component does not support tunneling"));
            return BOMX_ERR_TRACE(OMX_ErrorTunnelingUnsupported);
        }
        err = pOutputComponent->ComponentTunnelRequest(hOutput, nPortOutput, NULL, 0, &tunnelSetup);
    }
    else if ( hInput )
    {
        // Tearing down tunnel into input port
        if ( NULL == pInputComponent->ComponentTunnelRequest )
        {
            BOMX_ERR(("Output component does not support tunneling"));
            return BOMX_ERR_TRACE(OMX_ErrorTunnelingUnsupported);
        }
        err = pInputComponent->ComponentTunnelRequest(hInput, nPortInput, NULL, 0, &tunnelSetup);
    }
    return err;
}

OMX_API OMX_ERRORTYPE   OMX_GetContentPipe(
    OMX_OUT OMX_HANDLETYPE *hPipe,
    OMX_IN OMX_STRING szURI)
{
    BSTD_UNUSED(hPipe);
    BSTD_UNUSED(szURI);
    return BOMX_ERR_TRACE(OMX_ErrorNotImplemented);
}

OMX_API OMX_ERRORTYPE OMX_GetComponentsOfRole (
    OMX_IN      OMX_STRING role,
    OMX_INOUT   OMX_U32 *pNumComps,
    OMX_INOUT   OMX_U8  **compNames)
{
    int searchIdx;
    OMX_U32 compIdx = 0;

    if ( NULL == role || NULL == pNumComps )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }

    OMX_U32 maxComps = *pNumComps;
    for ( searchIdx = 0; searchIdx < g_numComponents; searchIdx++ )
    {
        int roleIdx;
        const ComponentEntry *pEntry = g_components+searchIdx;
        for ( roleIdx = 0; ; roleIdx++ )
        {
            const char *pRole = pEntry->pGetRole(roleIdx);
            if ( NULL == pRole )
            {
                break;
            }

            if ( !strcmp(pRole,role) )
            {
                if ( NULL == compNames )
                {
                    compIdx++;
                }
                else if ( compIdx < maxComps )
                {
                    strncpy((char *)compNames[compIdx++], pEntry->pName, OMX_MAX_STRINGNAME_SIZE);
                }
                else
                {
                    // Array is full, exit now
                    goto done;
                }
            }
        }
    }
done:
    *pNumComps = compIdx;
    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_GetRolesOfComponent (
    OMX_IN      OMX_STRING compName,
    OMX_INOUT   OMX_U32 *pNumRoles,
    OMX_OUT     OMX_U8 **roles)
{
    if ( NULL == compName || NULL == pNumRoles )
    {
        return BOMX_ERR_TRACE(OMX_ErrorBadParameter);
    }
    const ComponentEntry *pEntry = GetComponentByName(compName);
    if ( NULL == pEntry )
    {
        BOMX_ERR(("Unable to find component '%s'", compName));
        return BOMX_ERR_TRACE(OMX_ErrorComponentNotFound);
    }
    OMX_U32 maxRoles = *pNumRoles;
    OMX_U32 roleIdx;
    for ( roleIdx = 0; ; roleIdx++ )
    {
        const char *pRole = pEntry->pGetRole(roleIdx);
        if ( NULL == pRole )
        {
            break;
        }
        if ( NULL == roles )
        {
            continue;
        }
        else if ( roleIdx < maxRoles )
        {
            strncpy((char *)roles[roleIdx], pRole, OMX_MAX_STRINGNAME_SIZE);
        }
        else
        {
            // We've filled the array passed in, exit now
            goto done;
        }
    }

done:
    *pNumRoles = roleIdx;
    return OMX_ErrorNone;
}
