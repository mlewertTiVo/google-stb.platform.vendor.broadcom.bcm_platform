//#define LOG_NDEBUG 0
#define LOG_TAG "BcmSidebandViewer-JNI"

#include <system/window.h>
#include <android/native_window_jni.h>
#include <cutils/native_handle.h>
#include <utils/Log.h>
#include <utils/Mutex.h>
#include <errno.h>
#include <string.h>

#include <nativehelper/JNIHelp.h>
#include <bcm/hardware/dspsvcext/1.0/IDspSvcExt.h>
#include <bcm/hardware/sdbhak/1.0/ISdbHak.h>

using namespace android;
using namespace android::hardware;
using namespace bcm::hardware::dspsvcext::V1_0;
using namespace bcm::hardware::sdbhak::V1_0;

typedef void(*sb_geometry_cb)(void *context, unsigned int x, unsigned int y,
                                    unsigned int width, unsigned int height);

static Mutex svclock;
#define ATTEMPT_PAUSE_USEC 500000
#define MAX_ATTEMPT_COUNT  4
static const sp<IDspSvcExt> idse(void) {
   static sp<IDspSvcExt> idse = NULL;
   Mutex::Autolock _l(svclock);
   int c = 0;

   if (idse != NULL) {
      return idse;
   }

   do {
      idse = IDspSvcExt::getService();
      if (idse != NULL) {
         return idse;
      }
      usleep(ATTEMPT_PAUSE_USEC);
      c++;
   }
   while(c < MAX_ATTEMPT_COUNT);
   // can't get interface.
   ALOGE("failed to acquire service interface: IDspSvcExt!");
   return NULL;
}

static const sp<ISdbHak> ish(void) {
   static sp<ISdbHak> ish = NULL;
   Mutex::Autolock _l(svclock);
   int c = 0;

   if (ish != NULL) {
      return ish;
   }

   do {
      ish = ISdbHak::getService();
      if (ish != NULL) {
         return ish;
      }
      usleep(ATTEMPT_PAUSE_USEC);
      c++;
   }
   while(c < MAX_ATTEMPT_COUNT);
   // can't get interface.
   ALOGE("failed to acquire service interface: ISdbHak!");
   return NULL;
}

class SdbGeomCb : public IDspSvcExtSdbGeomCb {
public:
   SdbGeomCb(sb_geometry_cb cb, void *cb_data): geom_cb(cb), geom_ctx(cb_data) {};
   sb_geometry_cb geom_cb;
   void *geom_ctx;
   Return<void> onGeom(int32_t i, const DspSvcExtGeom& geom);
};

static sp<SdbGeomCb> gSdbGeomCb = NULL;
static Mutex lock;
static ANativeWindow *native_window = NULL;
static native_handle_t *native_handle = NULL;
static int player = 0;

Return<void> SdbGeomCb::onGeom(int32_t i, const DspSvcExtGeom& geom) {
   if (geom_cb) {
      if (i != 0) {
         ALOGW("SdbGeomCb::onGeom(%d), but registered for 0", i);
      } else {
         geom_cb(NULL, geom.geom_x, geom.geom_y, geom.geom_w, geom.geom_h);
      }
   }
   return Void();
}

static void sb_geometry_update(void *, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
   ALOGI("%s", __FUNCTION__);
   AutoMutex _l(lock);
   if (player && ish() != NULL) {
      ish()->position(x, y, width, height);
   }
   ALOGI("%s - signaled", __FUNCTION__);
}

static jboolean start_sideband(JNIEnv *env, jobject /*thizvoid*/, jobject jsurface)
{
   uint32_t context;

   ALOGV("%s", __FUNCTION__);
   AutoMutex _l(lock);

   native_window = ANativeWindow_fromSurface(env, jsurface);
   if (idse() != NULL) {
      gSdbGeomCb = new SdbGeomCb(&sb_geometry_update, NULL);
      context = idse()->regSdbCb(0, NULL);
      context = idse()->regSdbCb(0, gSdbGeomCb);
   }
   native_handle = native_handle_create(0, 2);
   if (!native_handle) {
      gSdbGeomCb = NULL;
      ALOGE("failed to allocate native handle");
      return JNI_FALSE;
   }
   native_handle->data[0] = 1;
   native_handle->data[1] = context;
   native_window_set_sideband_stream(native_window, native_handle);
   return JNI_TRUE;
}

static void stop_sideband(JNIEnv */*env*/, jobject /*thizvoid*/)
{
  ALOGV("%s", __FUNCTION__);
   AutoMutex _l(lock);

   if (native_window)
      native_window_set_sideband_stream(native_window, NULL);
   native_handle_delete(native_handle);
   gSdbGeomCb = NULL;
}

static jboolean start_file_player(JNIEnv *env, jobject /*thizvoid*/, jint x, jint y, jint w, jint h, jstring path)
{
   ALOGV("%s", __FUNCTION__);
   AutoMutex _l(lock);
   if (!player && ish() != NULL) {
      int rc = 0;
      const char* cpath = env->GetStringUTFChars(path, NULL);
      rc = ish()->create(cpath);
      env->ReleaseStringUTFChars(path, cpath);
      if (rc < 0) {
         ALOGE("Unable to create a sideband player (%d)", rc);
         return JNI_FALSE;
      }
      rc = ish()->start(x, y, w, h);
      if (rc < 0) {
         ALOGE("Unable to start sideband player (%d)", rc);
         ish()->destroy();
         return JNI_FALSE;
      }
      player = 1;
   }

   if (player) {
      return JNI_TRUE;
   } else {
      ALOGE("Unable to create sideband media player %d, ish: %s",
         player, ish()!=NULL?"AVAILABLE":"UNSUPPORTED");
      return JNI_FALSE;
   }
}

static void stop_player(JNIEnv */*env*/, jobject /*thizvoid*/)
{
   ALOGV("%s", __FUNCTION__);
   AutoMutex _l(lock);
   if (player && ish() != NULL) {
      ish()->stop();
      ish()->destroy();
      player = 0;
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

int JNI_OnLoad(JavaVM *jvm, void* /* reserved */) {
    JNIEnv *env;

    if (jvm->GetEnv((void**)&env, JNI_VERSION_1_6)) {
        return JNI_ERR;
    }

    return register_com_broadcom_sideband(env);
}
