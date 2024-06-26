/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class timsj_TimsTOFExperimentReader */

#ifndef _Included_timsj_TimsTOFExperimentReader
#define _Included_timsj_TimsTOFExperimentReader
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     timsj_TimsTOFExperimentReader
 * Method:    scanToOneOverK0Native
 * Signature: (I[ILjava/lang/String;Ljava/lang/String;)[D
 */
JNIEXPORT jdoubleArray JNICALL Java_timsj_TimsTOFExperimentReader_scanToOneOverK0Native
  (JNIEnv *, jobject, jint, jintArray, jstring, jstring);

/*
 * Class:     timsj_TimsTOFExperimentReader
 * Method:    tofToMzNative
 * Signature: (I[ILjava/lang/String;Ljava/lang/String;)[D
 */
JNIEXPORT jdoubleArray JNICALL Java_timsj_TimsTOFExperimentReader_tofToMzNative
  (JNIEnv *, jobject, jint, jintArray, jstring, jstring);


/*
 * Class:     timsj_TimsTOFExperimentReader
 * Method:    getDataHandlePointer
 * Signature: (Ljava/lang/String;Ljava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_timsj_TimsTOFExperimentReader_getDataHandlePointer
  (JNIEnv *env, jobject obj, jstring d, jstring b);


/*
 * Class:     timsj_TimsTOFExperimentReader
 * Method:    getTimsTofRawFrameNative
 * Signature: (IJ;)Ltimsj/TimsTOFRawFrame;
 */
JNIEXPORT jobject JNICALL Java_timsj_TimsTOFExperimentReader_getTimsTofRawFrameNative
(JNIEnv *env, jobject obj, jint frameId, jlong ptr);

#ifdef __cplusplus
}
#endif
#endif
