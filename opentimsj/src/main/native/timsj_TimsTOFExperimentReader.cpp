#include "timsj_TimsTOFExperimentReader.h"
#include <ctype.h>
#include <string.h>
#include <vector>

#include <iostream>
#include <cstdlib>
#include <stdio.h>

#include </home/administrator/Documents/promotion/opentims/opentims++/opentims_all.cpp>

// helper to convert from jstring to c++ string
std::string jstring2string(JNIEnv *env, jstring jStr) {
  if (!jStr)
      return "";

  const jclass stringClass = env->GetObjectClass(jStr);
  const jmethodID getBytes = env->GetMethodID(stringClass, "getBytes", "(Ljava/lang/String;)[B");
  const jbyteArray stringJbytes = (jbyteArray) env->CallObjectMethod(jStr, getBytes, env->NewStringUTF("UTF-8"));

  size_t length = (size_t) env->GetArrayLength(stringJbytes);
  jbyte* pBytes = env->GetByteArrayElements(stringJbytes, NULL);

  std::string ret = std::string((char *)pBytes, length);
  env->ReleaseByteArrayElements(stringJbytes, pBytes, JNI_ABORT);

  env->DeleteLocalRef(stringJbytes);
  env->DeleteLocalRef(stringClass);
  return ret;
}

// exposes opentims frame fetching to java / scala
JNIEXPORT jobject JNICALL Java_timsj_TimsTOFExperimentReader_getTimsTofRawFrameNative
(JNIEnv *env, jobject obj, jint frameId, jstring d, jstring b){

  const std::string data_path = jstring2string(env, d);
  const std::string bruker_binary_lib_path = jstring2string(env, b);

  DefaultTof2MzConverterFactory::setAsDefault<BrukerTof2MzConverterFactory, const char*>(bruker_binary_lib_path.c_str());
  DefaultScan2InvIonMobilityConverterFactory::setAsDefault<BrukerScan2InvIonMobilityConverterFactory, const char*>(bruker_binary_lib_path.c_str());

  // Open the dataset
  TimsDataHandle TDH(data_path);

  // Allocate buffers for data: instead of reallocating for every frame, we just allocate a buffer that will fit all
  // and reuse it.
  const size_t buffer_size_needed = TDH.max_peaks_in_frame();

  std::unique_ptr<uint32_t[]> frame_ids = std::make_unique<uint32_t[]>(buffer_size_needed);
  std::unique_ptr<uint32_t[]> scan_ids = std::make_unique<uint32_t[]>(buffer_size_needed);
  std::unique_ptr<uint32_t[]> tofs = std::make_unique<uint32_t[]>(buffer_size_needed);
  std::unique_ptr<uint32_t[]> intensities = std::make_unique<uint32_t[]>(buffer_size_needed);

  std::unique_ptr<double[]> mzs = std::make_unique<double[]>(buffer_size_needed);
  std::unique_ptr<double[]> inv_ion_mobilities = std::make_unique<double[]>(buffer_size_needed);

  std::unique_ptr<double[]> retention_times = std::make_unique<double[]>(buffer_size_needed);

  TimsFrame& frame = TDH.get_frame(frameId);
  frame.save_to_buffs(nullptr, scan_ids.get(), tofs.get(),
  intensities.get(), mzs.get(), inv_ion_mobilities.get(), nullptr);

  const int length = buffer_size_needed;
  jclass scalaFrame = env->FindClass("timsj/TimsTOFRawFrame");

  // check for class
  if (NULL == scalaFrame)
    std::cout << "/* class could not be found. */" << '\n';

  // check for class method
  jmethodID constructor = env->GetMethodID(scalaFrame, "<init>", "(ID[I[I[I[D[D)V");
  if(NULL == constructor)
    std::cout << "/* method could not be found. */" << '\n';

  jintArray resultScans = env->NewIntArray(length);
  jintArray resultTofs = env->NewIntArray(length);
  jintArray resultIntensities = env->NewIntArray(length);

  jdoubleArray resultMzs = env->NewDoubleArray(length);
  jdoubleArray resultOneOverK0s = env->NewDoubleArray(length);

  // check for allocation failure
  if (resultScans == NULL ||
    resultTofs == NULL ||
    resultIntensities == NULL ||
    resultOneOverK0s == NULL ||
    resultMzs == NULL) {
      return NULL;
  }

  jint *pScans = env->GetIntArrayElements(resultScans, NULL);
  jint *pTofs = env->GetIntArrayElements(resultTofs, NULL);
  jint *pIntens = env->GetIntArrayElements(resultIntensities, NULL);

  jdouble *pMzs = env->GetDoubleArrayElements(resultMzs, NULL);
  jdouble *pK0s = env->GetDoubleArrayElements(resultOneOverK0s, NULL);

  // copy values into java arrays
  for(int i = 0; i < length; i++){

    pScans[i] = scan_ids[i];
    pTofs[i] = tofs[i];
    pIntens[i] = intensities[i];

    pMzs[i] = mzs[i];
    pK0s[i] = inv_ion_mobilities[i];
  }

  // expose to garbage collection
  env->ReleaseIntArrayElements(resultScans, pScans, NULL);
  env->ReleaseIntArrayElements(resultTofs, pTofs, NULL);
  env->ReleaseIntArrayElements(resultIntensities, pIntens, NULL);

  env->ReleaseDoubleArrayElements(resultMzs, pMzs, NULL);
  env->ReleaseDoubleArrayElements(resultOneOverK0s, pK0s, NULL);

  // instanciate java object
  return env->NewObject(scalaFrame, constructor,
    TDH.get_frame(frameId).id,
    TDH.get_frame(frameId).time,
    resultScans,
    resultTofs,
    resultIntensities,
    resultMzs,
    resultOneOverK0s);
}

/*
 * Class:     timsj_TimsTOFExperimentReader
 * Method:    scanToOneOverK0Native
 * Signature: (I[ILjava/lang/String;Ljava/lang/String;)[D
 */
JNIEXPORT jdoubleArray JNICALL Java_timsj_TimsTOFExperimentReader_scanToOneOverK0Native
(JNIEnv *env, jobject obj, jint frameId, jintArray scans, jstring d, jstring b){

    const std::string data_path = jstring2string(env, d);
    const std::string bruker_binary_lib_path = jstring2string(env, b);

    DefaultTof2MzConverterFactory::setAsDefault<BrukerTof2MzConverterFactory, const char*>(bruker_binary_lib_path.c_str());
    DefaultScan2InvIonMobilityConverterFactory::setAsDefault<BrukerScan2InvIonMobilityConverterFactory, const char*>(bruker_binary_lib_path.c_str());

    // Open the dataset
    TimsDataHandle TDH(data_path);

    const size_t length = env->GetArrayLength(scans);
    uint32_t scans_c[length] = { 0 };
    double oneOverK0s[length];

    jint *body = env->GetIntArrayElements(scans, 0);
    for(int i = 0; i < length; i++){
        scans_c[i] = body[i];
    }

    TDH.scan2inv_ion_mobility_converter->convert(frameId, oneOverK0s, scans_c, length);

    jdoubleArray resultOneOverK0s = env->NewDoubleArray(length);

    if (resultOneOverK0s == NULL)
        return NULL;

    jdouble *pK0s = env->GetDoubleArrayElements(resultOneOverK0s, NULL);

    // copy values into java arrays
    for(int i = 0; i < length; i++){
        pK0s[i] = oneOverK0s[i];
    }

    env->ReleaseDoubleArrayElements(resultOneOverK0s, pK0s, NULL);

    return resultOneOverK0s;
}

/*
 * Class:     timsj_TimsTOFExperimentReader
 * Method:    tofToMzNative
 * Signature: (I[ILjava/lang/String;Ljava/lang/String;)[D
 */
JNIEXPORT jdoubleArray JNICALL Java_timsj_TimsTOFExperimentReader_tofToMzNative
(JNIEnv *env, jobject obj, jint frameId, jintArray tofs, jstring d, jstring b){

    const std::string data_path = jstring2string(env, d);
    const std::string bruker_binary_lib_path = jstring2string(env, b);

    DefaultTof2MzConverterFactory::setAsDefault<BrukerTof2MzConverterFactory, const char*>(bruker_binary_lib_path.c_str());
    DefaultScan2InvIonMobilityConverterFactory::setAsDefault<BrukerScan2InvIonMobilityConverterFactory, const char*>(bruker_binary_lib_path.c_str());

    // Open the dataset
    TimsDataHandle TDH(data_path);

    const size_t length = env->GetArrayLength(tofs);
    uint32_t tofs_c[length] = { 0 };
    double mzs[length];

    jint *body = env->GetIntArrayElements(tofs, 0);
    for(int i = 0; i < length; i++){
        tofs_c[i] = body[i];
    }

    TDH.tof2mz_converter->convert(frameId, mzs, tofs_c, length);

    jdoubleArray resultMzs = env->NewDoubleArray(length);

    if (resultMzs == NULL)
          return NULL;

    jdouble *pMzs = env->GetDoubleArrayElements(resultMzs, NULL);

      // copy values into java arrays
    for(int i = 0; i < length; i++){
        pMzs[i] = mzs[i];
    }

    env->ReleaseDoubleArrayElements(resultMzs, pMzs, NULL);

    return resultMzs;
}
