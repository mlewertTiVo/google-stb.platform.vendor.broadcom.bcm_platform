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
 * ANY LIMITED REMEDY. */

 
#include <dlfcn.h>      /* For dynamic loading */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>


/* #include "OMX_RegLib.h" */
#include "OMX_Component.h"
#include "OMX_Core.h"
#include "OMX_ComponentRegistry.h"
#include <utils/Log.h>
#undef LOG_TAG
#define LOG_TAG "BCM_OMX_CORE"
#define MAXCOMP (50)
#define MAXROLES (20)


/** Determine the number of elements in an array */
#define COUNTOF(x) (sizeof(x)/sizeof(x[0]))

/** Array to hold the DLL pointers for each allocated component */
static void *pModules[MAXCOMP] = { 0 };

/** Array to hold the component handles for each allocated component */
static void *pComponents[COUNTOF(pModules)] = { 0 };

ComponentTable componentTable[] = {
 {"OMX.BCM.aac.decoder", 1, {"1"} }, // roles to be populated 
 {"OMX.BCM.mpeg2.decoder", 1, {"1"} },
 {"OMX.BCM.h264.decoder", 1, {"1"} },
#ifdef ENABLE_SECURE_DECODERS
 {"OMX.BCM.h264.decoder.secure", 1, {"1"} },
#endif
 {"OMX.BCM.h263.decoder", 1, {"1"} },
 {"OMX.BCM.mpeg4.decoder", 1, {"1"} },
 {"OMX.BCM.vpx.decoder", 1, {"1"} },
#ifdef OMX_EXTEND_CODECS_SUPPORT
 {"OMX.BCM.wmv.decoder", 1, {"1"} },
 {"OMX.BCM.vc1.decoder", 1, {"1"} },
 {"OMX.BCM.spark.decoder", 1, {"1"} },
 //{"OMX.BCM.rv.decoder", 1, {"1"} },
 {"OMX.BCM.divx.decoder", 1, {"1"} },
 {"OMX.BCM.h265.decoder", 1, {"1"} },
// {"OMX.BCM.mjpeg.decoder", 1, {"1"} },
#endif
#ifdef BCM_OMX_SUPPORT_ENCODER
 {"OMX.BCM.h264.encoder", 1, {"1"} },
#endif
#ifdef BCM_OMX_SUPPORT_AC3_CODEC
 {"OMX.BCM.ac3.decoder", 1, {"1"} }, // roles to be populated 
 #endif
}; 

OMX_U32 tableCount = sizeof(componentTable)/sizeof(componentTable[0]);

//AD


/******************************Public*Routine******************************\
* OMX_Init()
*
* Description:This method will initialize the OMX Core.  It is the
* responsibility of the application to call OMX_Init to ensure the proper
* set up of core resources.
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
\**************************************************************************/
OMX_ERRORTYPE OMX_Init()
{
    LOGV("BCM OMX CORE OMX_Init\n");
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    return eError;
}

/******************************Public*Routine******************************\
* OMX_GetHandle
*
* Description: This method will create the handle of the COMPONENTTYPE
* If the component is currently loaded, this method will reutrn the
* hadle of existingcomponent or create a new instance of the component.
* It will call the OMX_ComponentInit function and then the setcallback
* method to initialize the callback functions
* Parameters:
* @param[out] pHandle            Handle of the loaded components
* @param[in] cComponentName     Name of the component to load
* @param[in] pAppData           Used to identify the callbacks of component
* @param[in] pCallBacks         Application callbacks
*
* @retval OMX_ErrorUndefined
* @retval OMX_ErrorInvalidComponentName
* @retval OMX_ErrorInvalidComponent
* @retval OMX_ErrorInsufficientResources
* @retval OMX_NOERROR                      Successful
*
* Note
*
\**************************************************************************/



OMX_ERRORTYPE OMX_GetHandle(OMX_HANDLETYPE * pHandle,
    OMX_STRING cComponentName, OMX_PTR pAppData,
    OMX_CALLBACKTYPE * pCallBacks)
{
    ALOGD("[%s][%s][%d]: Enter",
      __FILE__, 
      __FUNCTION__,
      __LINE__);


    static const char prefix[] = "lib";
    static const char postfix[] = ".so";
    OMX_ERRORTYPE(*pComponentInit) (OMX_HANDLETYPE *);
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *componentType;
    int i;
    const char *pErr = dlerror();
    char *dlError = NULL;
    char buf[sizeof(prefix) + 128 + sizeof(postfix)];


    strcpy(buf, cComponentName);

    /* Locate the first empty slot for a component.  If no slots
        are available, error out */
        
    for (i = 0; i < COUNTOF(pModules); i++)
    {
        if (pModules[i] == NULL)
            break;
    }

  if (i >= COUNTOF(pModules))
  {
    LOGE ("%s Couldn't find empty slot",__FUNCTION__);
        eError = OMX_ErrorComponentNotFound;
    return eError;
  }
   
    strcpy(buf, prefix);    /* the lengths are defined herein or have been */
    strcat(buf, cComponentName);    /* checked already, so strcpy and strcat are  */
    strcat(buf, postfix);   /* are safe to use in this context. */
    
    pModules[i] = dlopen(buf,  RTLD_LAZY | RTLD_GLOBAL);
    if (pModules[i] == NULL)
    {
        dlError = dlerror();
        eError = OMX_ErrorComponentNotFound;
        goto EXIT;
    }
    /* Get a function pointer to the "OMX_ComponentInit" function.  If
     * there is an error, we can't go on, so set the error code and exit */
    pComponentInit = dlsym(pModules[i], "OMX_ComponentInit");
    pErr = dlerror();

    if(pComponentInit == NULL)
    {
        LOGE ("pComponentInit == NULL\n");
        goto EXIT;
    }   

    /* We now can access the dll.  So, we need to call the "OMX_ComponentInit"
     * method to load up the "handle" (which is just a list of functions to
     * call) and we should be all set.*/
    *pHandle = malloc(sizeof(OMX_COMPONENTTYPE));
    pComponents[i] = *pHandle;
    componentType = (OMX_COMPONENTTYPE *) * pHandle;
    componentType->nSize = sizeof(OMX_COMPONENTTYPE);

    componentType->nVersion.s.nVersionMajor = 1;
    componentType->nVersion.s.nVersionMinor = 1;
    componentType->nVersion.s.nRevision = 0;
    componentType->nVersion.s.nStep = 0;
    
    eError = (*pComponentInit) (*pHandle);
    
    if (OMX_ErrorNone == eError)
    {
        eError =  (componentType->SetCallbacks) (*pHandle, pCallBacks, pAppData);
    }
    else
    {
        /* when the component fails to initialize, release the
           component handle structure */
        free(*pHandle);
        /* mark the component handle as NULL to prevent the caller from
           actually trying to access the component with it, should they
           ignore the return code */
        *pHandle = NULL;
        pComponents[i] = NULL;
        dlclose(pModules[i]);
        goto EXIT;
    }
    eError = OMX_ErrorNone;
    EXIT:
    return (eError);
}


/******************************Public*Routine******************************\
* OMX_FreeHandle()
*
* Description:This method will unload the OMX component pointed by
* OMX_HANDLETYPE. It is the responsibility of the calling method to ensure that
* the Deinit method of the component has been called prior to unloading component
*
* Parameters:
* @param[in] hComponent the component to unload
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
\**************************************************************************/
OMX_ERRORTYPE OMX_FreeHandle(OMX_HANDLETYPE hComponent)
{
    ALOGD("[%s][%s][%d]: Enter",
          __FILE__, 
          __FUNCTION__,
          __LINE__);
    
    OMX_ERRORTYPE eError = OMX_ErrorUndefined;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *) hComponent;
    int i;

    
    /* Locate the component handle in the array of handles */
    for (i = 0; i < COUNTOF(pModules); i++)
    {
        if (pComponents[i] == hComponent)
            break;
    }

  if (i >= COUNTOF(pModules))
  {
    ALOGE ("%s Couldn't find component handle",__FUNCTION__);
        eError = OMX_ErrorComponentNotFound;
    return eError;
  }

    eError = pHandle->ComponentDeInit(hComponent);
    if (eError != OMX_ErrorNone)
    {
        LOGE("Error From ComponentDeInit..");
    }
    
    /* release the component and the component handle */
    dlclose(pModules[i]);
    pModules[i] = NULL;
    free(pComponents[i]);

    pComponents[i] = NULL;
    eError = OMX_ErrorNone;

    return eError;
}

/******************************Public*Routine******************************\
* OMX_DeInit()
*
* Description:This method will release the resources of the OMX Core.  It is the
* responsibility of the application to call OMX_DeInit to ensure the clean up of these
* resources.
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
\**************************************************************************/
OMX_ERRORTYPE OMX_Deinit()
{
    ALOGD("[%s][%s][%d]: Enter-Returning OMX_ErrorNone",
          __FILE__, 
          __FUNCTION__,
          __LINE__);

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    return eError;
}

/*************************************************************************
* OMX_SetupTunnel()
*
* Description: Setup the specified tunnel the two components
*
* Parameters:
* @param[in] hOutput     Handle of the component to be accessed
* @param[in] nPortOutput Source port used in the tunnel
* @param[in] hInput      Component to setup the tunnel with.
* @param[in] nPortInput  Destination port used in the tunnel
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
**************************************************************************/
/* OMX_SetupTunnel */
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_SetupTunnel(OMX_IN OMX_HANDLETYPE
    hOutput, OMX_IN OMX_U32 nPortOutput, OMX_IN OMX_HANDLETYPE hInput,
    OMX_IN OMX_U32 nPortInput)
{
    ALOGD("[%s][%s][%d]: Enter-Returning OMX_ErrorNone",
          __FILE__, 
          __FUNCTION__,
          __LINE__);

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    return eError;
}

/*************************************************************************
* OMX_ComponentNameEnum()
*
* Description: This method will provide the name of the component at the given nIndex
*
*Parameters:
* @param[out] cComponentName       The name of the component at nIndex
* @param[in] nNameLength                The length of the component name
* @param[in] nIndex                         The index number of the component
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
**************************************************************************/
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_ComponentNameEnum(OMX_OUT OMX_STRING
    cComponentName, OMX_IN OMX_U32 nNameLength, OMX_IN OMX_U32 nIndex)
{
    ALOGD("[%s][%s][%d]: nIndex:%d nNameLength:%d \n",
          __FILE__, 
          __FUNCTION__,
          __LINE__,
          nIndex,
          nNameLength);

    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if (nIndex >= tableCount)
    {
        ALOGE("[%s][%s][%d]: No More Components\n",__FILE__, __FUNCTION__,__LINE__);
        eError = OMX_ErrorNoMore;
    } else {
        ALOGD("[%s][%s][%d]: LengthOfNameString:%d PassedInBufferSz:%d\n",
              __FILE__, __FUNCTION__,__LINE__,
              strlen(componentTable[nIndex].name),
              nNameLength);

        strcpy(cComponentName, componentTable[nIndex].name);
    }

EXIT:
    return eError;
}


/*************************************************************************
* OMX_GetRolesOfComponent()
*
* Description: This method will query the component for its supported roles
*
*Parameters:
* @param[in] cComponentName     The name of the component to query
* @param[in] pNumRoles     The number of roles supported by the component
* @param[in] roles      The roles of the component
*
* Returns:    OMX_NOERROR          Successful
*                 OMX_ErrorBadParameter     Faliure due to a bad input parameter
*
* Note
*
**************************************************************************/
OMX_API OMX_ERRORTYPE OMX_GetRolesOfComponent(OMX_IN OMX_STRING
    cComponentName, OMX_INOUT OMX_U32 * pNumRoles, OMX_OUT OMX_U8 ** roles)
{
    ALOGD("[%s][%s][%d]: Enter- Returning None",
          __FILE__, 
          __FUNCTION__,
          __LINE__);

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    return eError;
}

/*************************************************************************
* OMX_GetComponentsOfRole()
*
* Description: This method will query the component for its supported roles
*
*Parameters:
* @param[in] role     The role name to query for
* @param[in] pNumComps     The number of components supporting the given role
* @param[in] compNames      The names of the components supporting the given role
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
**************************************************************************/
OMX_API OMX_ERRORTYPE OMX_GetComponentsOfRole(OMX_IN OMX_STRING role,
    OMX_INOUT OMX_U32 * pNumComps, OMX_INOUT OMX_U8 ** compNames)
{
    ALOGD("[%s][%s][%d]: Enter-Returning None",
      __FILE__, 
      __FUNCTION__,
      __LINE__);

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    return eError;
}


/***************************************
PRINT TABLE FOR DEBUGGING PURPOSES ONLY
***************************************/

OMX_API OMX_ERRORTYPE OMX_PrintComponentTable()
{
    ALOGD("[%s][%s][%d]: Enter-Returning None",
          __FILE__, 
          __FUNCTION__,
          __LINE__);

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    
    return eError;

}


OMX_ERRORTYPE OMX_BuildComponentTable()
{
    ALOGD("[%s][%s][%d]: Enter-Returning None",
          __FILE__, 
          __FUNCTION__,
          __LINE__);

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    return eError;
}

OMX_ERRORTYPE ComponentTable_EventHandler(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_EVENTTYPE eEvent,
    OMX_IN OMX_U32 nData1, OMX_IN OMX_U32 nData2, OMX_IN OMX_PTR pEventData)
{
    ALOGD("[%s][%s][%d]: Enter-Returning OMX_ErrorNotImplemented",
          __FILE__, 
          __FUNCTION__,
          __LINE__);

    return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE ComponentTable_EmptyBufferDone(OMX_OUT OMX_HANDLETYPE
    hComponent, OMX_OUT OMX_PTR pAppData,
    OMX_OUT OMX_BUFFERHEADERTYPE * pBuffer)
{
    ALOGD("[%s][%s][%d]: Enter-Returning OMX_ErrorNotImplemented",
          __FILE__, 
          __FUNCTION__,
          __LINE__);

    return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE ComponentTable_FillBufferDone(OMX_OUT OMX_HANDLETYPE hComponent,
    OMX_OUT OMX_PTR pAppData, OMX_OUT OMX_BUFFERHEADERTYPE * pBuffer)
{
    ALOGD("[%s][%s][%d]: Enter-Returning OMX_ErrorNotImplemented",
          __FILE__, 
          __FUNCTION__,
          __LINE__);

    return OMX_ErrorNotImplemented;
}



