/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class org_qwics_jni_QwicsTPMServerWrapper */

#ifndef _Included_org_qwics_jni_QwicsTPMServerWrapper
#define _Included_org_qwics_jni_QwicsTPMServerWrapper
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     org_qwics_jni_QwicsTPMServerWrapper
 * Method:    execCallbackNative
 * Signature: (Ljava/lang/String;[BIII)V
 */
JNIEXPORT void JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_execCallbackNative
  (JNIEnv *, jobject, jstring, jbyteArray, jint, jint, jint);

/*
 * Class:     org_qwics_jni_QwicsTPMServerWrapper
 * Method:    readByte
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_readByte
  (JNIEnv *, jobject, jlong);

/*
 * Class:     org_qwics_jni_QwicsTPMServerWrapper
 * Method:    writeByte
 * Signature: (JB)I
 */
JNIEXPORT jint JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_writeByte
  (JNIEnv *, jobject, jlong, jbyte);

/*
 * Class:     org_qwics_jni_QwicsTPMServerWrapper
 * Method:    init
 * Signature: ()[J
 */
JNIEXPORT jlongArray JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_init
  (JNIEnv *, jobject);

/*
 * Class:     org_qwics_jni_QwicsTPMServerWrapper
 * Method:    clear
 * Signature: ([J)V
 */
JNIEXPORT void JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_clear
  (JNIEnv *, jobject, jlongArray);

/*
 * Class:     org_qwics_jni_QwicsTPMServerWrapper
 * Method:    execInTransaction
 * Signature: (Ljava/lang/String;JII)I
 */
JNIEXPORT jint JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_execInTransaction
  (JNIEnv *, jobject, jstring, jlong, jint, jint);

/*
 * Class:     org_qwics_jni_QwicsTPMServerWrapper
 * Method:    execSqlNative
 * Signature: (Ljava/lang/String;JII)V
 */
JNIEXPORT void JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_execSqlNative
  (JNIEnv *, jobject, jstring, jlong, jint, jint);

#ifdef __cplusplus
}
#endif
#endif
