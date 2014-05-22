#include "com_broadcom_NdkSample_MyJni.h"

JNIEXPORT jstring JNICALL Java_com_broadcom_NdkSample_MyJni_hello
  (JNIEnv * env, jobject obj) {
		return (*env)->NewStringUTF(env, "This string is acquired from c function through JNI!");
}

JNIEXPORT jint JNICALL Java_com_broadcom_NdkSample_MyJni_add
  (JNIEnv * env, jobject obj, jint value1, jint value2) {
		return (value1 + value2);
}
