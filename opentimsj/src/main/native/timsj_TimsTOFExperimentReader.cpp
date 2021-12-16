#include <ctype.h>
#include <string.h>
#include <vector>

#include <iostream>
#include <cstdlib>
#include <stdio.h>

#include "timsj_TimsTOFExperimentReader.h"
#include "../../../../opentims++/opentims_all.cpp"


struct TimsFramePl {
    int frameId; // coordinate
    std::vector<double> mzs, intensities, inv_ion_mobs; // data
    std::vector<int> scans, tofs; // data
    // constructors
    TimsFramePl(){}
    TimsFramePl(int id, std::vector<int> scan, std::vector<double> mz, std::vector<double> intensity, std::vector<int> tof, std::vector<double> inv_ion_mob):
    frameId(id), mzs(mz), intensities(intensity), scans(scan), tofs(tof), inv_ion_mobs(inv_ion_mob){}
};

/**
 * helper for TDH initialization
 * @param dp data path
 * @param bp binary path
 * @return a tims data handle
 */
TimsDataHandle get_tdh(std::string dp, std::string bp){
    // need to translate TOF to MZ
    DefaultTof2MzConverterFactory::setAsDefault<BrukerTof2MzConverterFactory, const char*>(bp.c_str());
    // need to translate SCAN to 1/K0
    DefaultScan2InvIonMobilityConverterFactory::setAsDefault<BrukerScan2InvIonMobilityConverterFactory, const char*>(bp.c_str());

    return TimsDataHandle(dp);
}

/**
 *
 */
class ExposedTimsDataHandle{
private:
    TimsDataHandle handle;

public:
    // path to dataset, path to bruker binaries
    std::string datasetPath, binaryPath;
    // handle to read from raw data
    ExposedTimsDataHandle(std::string dp, std::string bp);

    TimsFramePl getTimsFramePl(const int frameId);
};

ExposedTimsDataHandle::ExposedTimsDataHandle(std::string dp, std::string bp) : handle(get_tdh(dp, bp)) {
    datasetPath = dp;
    binaryPath = bp;
};


TimsFramePl ExposedTimsDataHandle::getTimsFramePl(const int frameId) {
    // allocate buffer
    const size_t buffer_size_needed = handle.max_peaks_in_frame();
    std::unique_ptr<uint32_t[]> scan_ids = std::make_unique<uint32_t[]>(buffer_size_needed);
    std::unique_ptr<uint32_t[]> intens = std::make_unique<uint32_t[]>(buffer_size_needed);
    std::unique_ptr<uint32_t[]> tofs = std::make_unique<uint32_t[]>(buffer_size_needed);
    std::unique_ptr<double[]> inv_ion_mob = std::make_unique<double[]>(buffer_size_needed);
    std::unique_ptr<double[]> mz = std::make_unique<double[]>(buffer_size_needed);

    // fetch
    TimsFrame& frame = handle.get_frame(frameId);
    frame.save_to_buffs(nullptr, scan_ids.get(), tofs.get(), intens.get(), mz.get(), inv_ion_mob.get(), nullptr);

    // allocate concrete vectors
    std::vector<double> mzs, intensities, inv_ion_mobility;
    std::vector<int> scans, tof;

    // pre-allocate to avoid resizing on fill
    mzs.reserve(frame.num_peaks);
    intensities.reserve(frame.num_peaks);
    inv_ion_mobility.reserve(frame.num_peaks);
    scans.reserve(frame.num_peaks);
    tof.reserve(frame.num_peaks);

    // copy
    for(size_t peak_id = 0; peak_id < frame.num_peaks; peak_id++) {
        tof.push_back(tofs[peak_id]);
        inv_ion_mobility.push_back(inv_ion_mob[peak_id]);
        intensities.push_back(intens[peak_id]);
        mzs.push_back(mz[peak_id]);
        scans.push_back(scan_ids[peak_id]);
    }

    return TimsFramePl(frameId, scans, mzs, intensities, tof, inv_ion_mobility);
}

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
    for(size_t i = 0; i < length; i++) {
        scans_c[i] = body[i];
    }

    TDH.scan2inv_ion_mobility_converter->convert(frameId, oneOverK0s, scans_c, length);

    jdoubleArray resultOneOverK0s = env->NewDoubleArray(length);

    if (resultOneOverK0s == NULL)
        return NULL;

    jdouble *pK0s = env->GetDoubleArrayElements(resultOneOverK0s, NULL);

    // copy values into java arrays
    for(size_t i = 0; i < length; i++) {
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
    for(size_t i = 0; i < length; i++) {
        tofs_c[i] = body[i];
    }

    TDH.tof2mz_converter->convert(frameId, mzs, tofs_c, length);

    jdoubleArray resultMzs = env->NewDoubleArray(length);

    if (resultMzs == NULL)
          return NULL;

    jdouble *pMzs = env->GetDoubleArrayElements(resultMzs, NULL);

      // copy values into java arrays
    for(size_t i = 0; i < length; i++) {
        pMzs[i] = mzs[i];
    }

    env->ReleaseDoubleArrayElements(resultMzs, pMzs, NULL);

    return resultMzs;
}

/*
 * Class:     timsj_TimsTOFExperimentReader
 * Method:    getDataHandlePointer
 * Signature: (Ljava/lang/String;Ljava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_timsj_TimsTOFExperimentReader_getDataHandlePointer
(JNIEnv *env, jobject obj, jstring d, jstring b){
  
  const std::string data_path = jstring2string(env, d);
  const std::string bruker_binary_lib_path = jstring2string(env, b);

  DefaultTof2MzConverterFactory::setAsDefault<BrukerTof2MzConverterFactory, const char*>(bruker_binary_lib_path.c_str());
  DefaultScan2InvIonMobilityConverterFactory::setAsDefault<BrukerScan2InvIonMobilityConverterFactory, const char*>(bruker_binary_lib_path.c_str());

  // Open the dataset
  // ExposedTimsDataHandle tdh = new ExposedTimsDataHandle(data_path, bruker_binary_lib_path);
  
  return (long)(new ExposedTimsDataHandle(data_path, bruker_binary_lib_path));
}

/*
 * Class:     timsj_TimsTOFExperimentReader
 * Method:    getTimsTofRawFrameNative
 * Signature: (IJ;)Ltimsj/TimsTOFRawFrame;
 */
JNIEXPORT jobject JNICALL Java_timsj_TimsTOFExperimentReader_getTimsTofRawFrameNative
(JNIEnv *env, jobject obj, jint frameId, jlong ptr){

  // get tdh object by pointer (god help me.)
  ExposedTimsDataHandle *tdh  = (ExposedTimsDataHandle *) ptr;
  TimsFramePl frame = tdh->getTimsFramePl(frameId);

  // get java class 
  jclass scalaFrame = env->FindClass("timsj/TimsTOFRawFrame");

  // check for class
  if (NULL == scalaFrame)
    std::cout << "/* class could not be found. */" << '\n';

  // check for class method
  jmethodID constructor = env->GetMethodID(scalaFrame, "<init>", "(ID[I[I[I[D[D)V");
  if(NULL == constructor)
    std::cout << "/* method could not be found. */" << '\n';

  jintArray resultScans = env->NewIntArray(frame.mzs.size());
  jintArray resultTofs = env->NewIntArray(frame.mzs.size());
  jintArray resultIntensities = env->NewIntArray(frame.mzs.size());

  jdoubleArray resultMzs = env->NewDoubleArray(frame.mzs.size());
  jdoubleArray resultOneOverK0s = env->NewDoubleArray(frame.mzs.size());

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
  for(size_t peak_id = 0; peak_id < frame.mzs.size(); peak_id++) {

    pScans[peak_id] = frame.scans[peak_id];
    pTofs[peak_id] = frame.tofs[peak_id];
    pIntens[peak_id] = frame.intensities[peak_id];

    pMzs[peak_id] = frame.mzs[peak_id];
    pK0s[peak_id] = frame.inv_ion_mobs[peak_id];
  }

  // expose to garbage collection
  env->ReleaseIntArrayElements(resultScans, pScans, NULL);
  env->ReleaseIntArrayElements(resultTofs, pTofs, NULL);
  env->ReleaseIntArrayElements(resultIntensities, pIntens, NULL);

  env->ReleaseDoubleArrayElements(resultMzs, pMzs, NULL);
  env->ReleaseDoubleArrayElements(resultOneOverK0s, pK0s, NULL);

  // instanciate java object
  return env->NewObject(scalaFrame, constructor,
    frame.frameId,
    0.0,
    resultScans,
    resultTofs,
    resultIntensities,
    resultMzs,
    resultOneOverK0s);
}