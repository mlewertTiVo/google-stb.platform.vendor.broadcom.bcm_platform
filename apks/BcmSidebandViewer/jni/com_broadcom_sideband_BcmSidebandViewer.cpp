//#define LOG_NDEBUG 0
#define LOG_TAG "BcmSidebandViewer-JNI"

#include <JNIHelp.h>

#include <system/window.h>
#include <android/native_window_jni.h>
#include <utils/Log.h>
#include <errno.h>
#include <string.h>

#include <bcmsideband.h>
#include <bcmsidebandplayerfactory.h>

namespace android {

BcmSidebandPlayer *sidebandPlayer = NULL;
struct bcmsideband_ctx *sidebandContext = NULL;

static void sb_geometry_update(void *, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
    ALOGV("%s", __FUNCTION__);
    if (sidebandPlayer)
        sidebandPlayer->setWindowPosition(x, y, width, height);
}

static jboolean start_sideband(JNIEnv *env, jobject /*thizvoid*/, jobject jsurface)
{
    ANativeWindow *native_window;
    int video_id = -1, audio_id = -1, surface_id = -1;

    ALOGV("%s", __FUNCTION__);
    if (sidebandContext == NULL) {
        native_window = ANativeWindow_fromSurface(env, jsurface);
        sidebandContext = libbcmsideband_init_sideband(native_window, &video_id, &audio_id, &surface_id, &sb_geometry_update);
        if (!sidebandContext) {
            ALOGE("Unable to initalize the sideband");
            return JNI_FALSE;
        }
        ALOGI("Sideband initialized video:%d audio:%d surface:%d", video_id, audio_id, surface_id);
    }
    return JNI_TRUE;
}

static void stop_sideband(JNIEnv */*env*/, jobject /*thizvoid*/)
{
    ALOGV("%s", __FUNCTION__);
    if (sidebandContext != NULL) {
        libbcmsideband_release(sidebandContext);
        sidebandContext = NULL;
    }
}

static jboolean start_file_player(JNIEnv *env, jobject /*thizvoid*/, jint x, jint y, jint w, jint h, jstring path)
{
    ALOGV("%s", __FUNCTION__);
    if (sidebandPlayer == NULL) {
        const char* cpath = env->GetStringUTFChars(path, NULL);
        BcmSidebandPlayer* player = BcmSidebandPlayerFactory::createFilePlayer(cpath);
        env->ReleaseStringUTFChars(path, cpath);
        if (player == NULL) {
            ALOGE("Unable to create a sideband player");
            return JNI_FALSE;
        }
        int err;
        err = player->start(x, y, w, h);
        if (err) {
            ALOGE("Unable to start sideband player (%d)", err);
            delete player;
            return JNI_FALSE;
        }
        sidebandPlayer = player;
    }
    return JNI_TRUE;
}

static void stop_player(JNIEnv */*env*/, jobject /*thizvoid*/)
{
    ALOGV("%s", __FUNCTION__);
    if (sidebandPlayer != NULL) {
        sidebandPlayer->stop();
        delete sidebandPlayer;
        sidebandPlayer = NULL;
    }
}

static jboolean can_access_file(JNIEnv *env, jobject /*thizvoid*/, jstring path)
{
    const char* cpath = env->GetStringUTFChars(path, NULL);
    FILE *file = fopen(cpath, "rb");
    if (!file) {
        ALOGW("Can't open %s (%s)", cpath, strerror(errno));
        env->ReleaseStringUTFChars(path, cpath);
        return JNI_FALSE;
    }
    fclose(file);
    env->ReleaseStringUTFChars(path, cpath);
    return JNI_TRUE;
}

const JNINativeMethod g_methods[] = {
    { "start_sideband", "(Landroid/view/Surface;)Z", (void*)start_sideband },
    { "stop_sideband", "()V", (void*)stop_sideband },
    { "start_file_player", "(IIIILjava/lang/String;)Z", (void*)start_file_player },
    { "stop_player", "()V", (void*)stop_player },
    { "can_access_file", "(Ljava/lang/String;)Z", (void*)can_access_file },
};

int register_com_broadcom_sideband(JNIEnv *env) {
    if (jniRegisterNativeMethods(
            env, "com/broadcom/sideband/BcmSidebandViewer", g_methods, NELEM(g_methods)) < 0) {
        return JNI_ERR;
    }

    return JNI_VERSION_1_6;
}

} // namespace android

int JNI_OnLoad(JavaVM *jvm, void* /* reserved */) {
    JNIEnv *env;

    if (jvm->GetEnv((void**)&env, JNI_VERSION_1_6)) {
        return JNI_ERR;
    }

    return android::register_com_broadcom_sideband(env);
}
