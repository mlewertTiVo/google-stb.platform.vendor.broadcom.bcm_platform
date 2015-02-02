/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class com_broadcom_tvinput_TunerHAL */

#ifndef _Included_com_broadcom_tvinput_TunerHAL
#define _Included_com_broadcom_tvinput_TunerHAL
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_broadcom_tvinput_TunerHAL
 * Method:    initialize
 * Signature: (Ljava/lang/Object;)I
 */
JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_initialize
  (JNIEnv *, jclass, jobject);

/*
 * Class:     com_broadcom_tvinput_TunerHAL
 * Method:    scan
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_startBlindScan
  (JNIEnv *, jclass);

/*
 * Class:     com_broadcom_tvinput_TunerHAL
 * Method:    scan
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_stopScan
  (JNIEnv *, jclass);

/*
 * Class:     com_broadcom_tvinput_TunerHAL
 * Method:    tune
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_tune
  (JNIEnv *, jclass, jstring);

/*
 * Class:     com_broadcom_tvinput_TunerHAL
 * Method:    getChannelList
 * Signature: ()[Lcom/broadcom/tvinput/ChannelInfo;
 */
JNIEXPORT jobjectArray JNICALL Java_com_broadcom_tvinput_TunerHAL_getChannelList
  (JNIEnv *, jclass);

/*
 * Class:     com_broadcom_tvinput_TunerHAL
 * Method:    getProgramList
 * Signature: (Ljava/lang/String;)[Lcom/broadcom/tvinput/ProgramInfo;
 */
JNIEXPORT jobjectArray JNICALL Java_com_broadcom_tvinput_TunerHAL_getProgramList
  (JNIEnv *, jclass, jstring);

/*
 * Class:     com_broadcom_tvinput_TunerHAL
 * Method:    getScanInfo
 * Signature: ()Lcom/broadcom/tvinput/ScanInfo;
 */
JNIEXPORT jobject JNICALL Java_com_broadcom_tvinput_TunerHAL_getScanInfo
  (JNIEnv *, jclass);

/*
 * Class:     com_broadcom_tvinput_TunerHAL
 * Method:    getTrackInfoList
 * Signature: ()[Lcom/broadcom/tvinput/TrackInfo;
 */
JNIEXPORT jobjectArray JNICALL Java_com_broadcom_tvinput_TunerHAL_getTrackInfoList
  (JNIEnv *, jclass);

/*
 * Class:     com_broadcom_tvinput_TunerHAL
 * Method:    selectTrack
 * Signature: (ILjava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_selectTrack
  (JNIEnv *, jclass, jint, jstring);

/*
 * Class:     com_broadcom_tvinput_TunerHAL
 * Method:    getUtcTime
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_com_broadcom_tvinput_TunerHAL_getUtcTime
  (JNIEnv *, jclass);

/*
 * Class:     com_broadcom_tvinput_TunerHAL
 * Method:    stop
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_stop
  (JNIEnv *, jclass);

/*
 * Class:     com_broadcom_tvinput_TunerHAL
 * Method:    release
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_release
  (JNIEnv *, jclass);

/*
 * Class:     com_broadcom_tvinput_TunerHAL
 * Method:    stop
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_broadcom_tvinput_TunerHAL_setSurface
  (JNIEnv *, jclass);

#ifdef __cplusplus
}
#endif
#endif
