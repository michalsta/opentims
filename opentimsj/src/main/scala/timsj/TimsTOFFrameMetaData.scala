package timsj

/**
 * container class for meta data representation of bruker timsTOF frames
 * @param frameId unique frame identifier
 * @param time retention time of frame in seconds
 * @param polarity polarity
 * @param scanMode ?
 * @param msMsType 0 == precursor, 8 == DDA fragment, 9 == DIA fragment
 * @param timsId ?
 * @param maxIntensity maximun intensity in frame
 * @param sumIntensity sum intensity of frame
 * @param numScans how many scan spectra (drift  times) are in this frame
 * @param numPeaks number of peaks in frame
 * @param mzCalibration ?
 * @param temp1 ?
 * @param temp2 ?
 * @param timsCalibration ?
 * @param porpertyGroup ?
 * @param accumTime ?
 * @param rampTime ?
 */
case class TimsTOFFrameMetaData(frameId: Int = -1, time: Double = -1, polarity: String = "NULL", scanMode: Int = -1,
                                msMsType: Int = -1, timsId: Int = -1, maxIntensity: Int = -1, sumIntensity: Int = -1,
                                numScans: Int = -1, numPeaks: Int = -1, mzCalibration: Int = -1,
                                temp1: Double = -1, temp2: Double = -1, timsCalibration: Int = -1,
                                porpertyGroup: Int = -1, accumTime: Double = -1, rampTime: Double = -1)
