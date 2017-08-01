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
 * Module Description:
 *
 *****************************************************************************/
#undef LOG_TAG
#define LOG_NDEBUG 0
#define LOG_TAG "bomx_android_plugin"

#include "bomx_android_plugin.h"
#include "bomx_utils.h"

#include <media/stagefright/foundation/ADebug.h>
#include <utils/Log.h>
#include <media/stagefright/omx/SoftOMXComponent.h>

#include <media/openmax/OMX_Core.h>
#include <dlfcn.h>

namespace android {

OMXPluginBase *createOMXPlugin()
{
    ALOGI("OMX plugin created\n");
    return new BOMX_AndroidPlugin;
}

BOMX_AndroidPlugin::BOMX_AndroidPlugin()
{
    OMX_ERRORTYPE omxErr;

    ALOGI("OMX_Init\n");
    omxErr = OMX_Init();
    if ( OMX_ErrorNone != omxErr )
    {
        ALOGE("OMX_Init failed!");
    }
}

BOMX_AndroidPlugin::~BOMX_AndroidPlugin()
{
    ALOGI("OMX_Deinit\n");
    (void)OMX_Deinit();
}

OMX_ERRORTYPE BOMX_AndroidPlugin::makeComponentInstance(
        const char *name,
        const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData,
        OMX_COMPONENTTYPE **component)
{
    OMX_ERRORTYPE omxErr;

    ALOGV("OMX_GetHandle(%s)", name);
    if (strcmp(name, "OMX.broadcom.audio_decoder.aac") == 0) {
        return allocateSoftAAC(name, callbacks, appData, component);
    }

    omxErr = OMX_GetHandle(reinterpret_cast<OMX_HANDLETYPE *>(component), const_cast<char *>(name), appData, const_cast<OMX_CALLBACKTYPE *>(callbacks));
    if ( OMX_ErrorNone != omxErr )
    {
        return BOMX_ERR_TRACE(omxErr);
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_AndroidPlugin::destroyComponentInstance(
        OMX_COMPONENTTYPE *component)
{
    OMX_ERRORTYPE omxErr;
    char componentName[OMX_MAX_STRINGNAME_SIZE];
    OMX_VERSIONTYPE componentVersion;
    OMX_VERSIONTYPE specVersion;
    OMX_UUIDTYPE componentUUID;

    ALOGV("OMX_FreeHandle(%p)", component);
    // Revise this later. We use the weak assumption that the softaac
    // decoder doesn't implement this function and bcm decoders do.
    if (component->GetComponentVersion == NULL) {
        return deAllocateSoftAAC(component);
    }

    strcpy(componentName, "");
    omxErr =  component->GetComponentVersion(component, componentName, &componentVersion, &specVersion, &componentUUID);
    if (omxErr != OMX_ErrorNone) {
        ALOGE("Error retrieving component name");
        return omxErr;
    }
    if (strcmp(componentName, "OMX.broadcom.audio_decoder.aac") == 0) {
        return deAllocateSoftAAC(component);
    }

    omxErr = OMX_FreeHandle(reinterpret_cast<OMX_HANDLETYPE *>(component));
    if ( OMX_ErrorNone != omxErr )
    {
        return BOMX_ERR_TRACE(omxErr);
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_AndroidPlugin::enumerateComponents(
        OMX_STRING name,
        size_t size,
        OMX_U32 index)
{
    OMX_ERRORTYPE omxErr;

    omxErr = OMX_ComponentNameEnum(name, size, index);
    ALOGV("enumerateComponents: %s %u => %u", name, index, omxErr);
    switch ( omxErr )
    {
    case OMX_ErrorNone:
    case OMX_ErrorNoMore:
        return omxErr;
    default:
        return BOMX_ERR_TRACE(omxErr);
    }
}

OMX_ERRORTYPE BOMX_AndroidPlugin::getRolesOfComponent(
        const char *name,
        Vector<String8> *roles)
{
    roles->clear();

    OMX_U32 numRoles = 0;
    ALOGV("getRolesOfComponent: %s", name);
    if (strcmp(name, "OMX.broadcom.audio_decoder.aac") == 0) {
        return getRolesSoftAAC(name, roles);
    }

    OMX_ERRORTYPE err = OMX_GetRolesOfComponent(const_cast<OMX_STRING>(name), &numRoles, NULL);

    if (err != OMX_ErrorNone)
    {
        return BOMX_ERR_TRACE(err);
    }

    if (numRoles > 0)
    {
        OMX_U8 **array = new OMX_U8 *[numRoles];
        for (OMX_U32 i = 0; i < numRoles; ++i)
        {
            array[i] = new OMX_U8[OMX_MAX_STRINGNAME_SIZE];
        }

        OMX_U32 numRoles2 = 0;
        err = OMX_GetRolesOfComponent(const_cast<OMX_STRING>(name), &numRoles2, array);

        CHECK_EQ(err, OMX_ErrorNone);
        CHECK_EQ(numRoles, numRoles2);

        for (OMX_U32 i = 0; i < numRoles; ++i)
        {
            String8 s((const char *)array[i]);
            ALOGV("getRolesOfComponent: %s [%d] -> %s", name, i, array[i]);
            roles->push(s);

            delete[] array[i];
            array[i] = NULL;
        }

        delete[] array;
        array = NULL;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_AndroidPlugin::allocateSoftAAC(
        const char *name,
        const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData,
        OMX_COMPONENTTYPE **component)
{
    AString libName = "libstagefright_soft_aacdec.so";
    void *libHandle = dlopen(libName.c_str(), RTLD_NOW);

    if (libHandle == NULL) {
        ALOGE("unable to dlopen %s: %s", libName.c_str(), dlerror());
        return OMX_ErrorComponentNotFound;
    }

    typedef SoftOMXComponent *(*CreateSoftOMXComponentFunc)(
            const char *, const OMX_CALLBACKTYPE *,
            OMX_PTR, OMX_COMPONENTTYPE **);

    CreateSoftOMXComponentFunc createSoftOMXComponent =
        (CreateSoftOMXComponentFunc)dlsym(
                libHandle,
                "_Z22createSoftOMXComponentPKcPK16OMX_CALLBACKTYPE"
                "PvPP17OMX_COMPONENTTYPE");

    if (createSoftOMXComponent == NULL) {
        dlclose(libHandle);
        libHandle = NULL;
        return OMX_ErrorComponentNotFound;
    }

    sp<SoftOMXComponent> codec =
        (*createSoftOMXComponent)(name, callbacks, appData, component);

    if (codec == NULL) {
        dlclose(libHandle);
        libHandle = NULL;

        return OMX_ErrorInsufficientResources;
    }

    OMX_ERRORTYPE err = codec->initCheck();
    if (err != OMX_ErrorNone) {
        dlclose(libHandle);
        libHandle = NULL;

        return err;
    }

    codec->incStrong(this);
    codec->setLibHandle(libHandle);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_AndroidPlugin::deAllocateSoftAAC(
        OMX_COMPONENTTYPE *component)
{
    SoftOMXComponent *me =
        (SoftOMXComponent *)
            ((OMX_COMPONENTTYPE *)component)->pComponentPrivate;

    me->prepareForDestruction();

    void *libHandle = me->libHandle();

    CHECK_EQ(me->getStrongCount(), 1);
    me->decStrong(this);
    me = NULL;

    dlclose(libHandle);
    libHandle = NULL;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE BOMX_AndroidPlugin::getRolesSoftAAC(
        const char *name,
        Vector<String8> *roles)
{
    (void) name;
    roles->clear();
    roles->push(String8("audio_decoder.aac"));
    return OMX_ErrorNone;
}

}  // namespace android
