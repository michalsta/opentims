package timsj

import com.almworks.sqlite4java.SQLiteConnection

import java.io.File

/**
 * connector for reading of raw timsTOF data
 * @param experimentPath path to timsTOF experiment (.d)
 * @param brukerBinaryPath path to binary for bruker file format reading (.so or .dll)
 */
class TimsTOFExperimentReader(experimentPath: String, brukerBinaryPath: String) {
  /**
   * get all available meta data of frames present in a given experiment
   * @return map<frameId, TimsTOFFrameMetadata> collection of meta data for all frames
   */
  def getFrameMetaData: Map[Int, TimsTOFFrameMetaData] = {
    // open sqlite database connection
    val db = new SQLiteConnection(new File(experimentPath + "/analysis.tdf"))
    db.openReadonly()
    val st = db.prepare("SELECT * FROM Frames")

    // fetch all meta data from frames and save them to map
    val mMap = scala.collection.mutable.Map[Int, TimsTOFFrameMetaData]()
    while (st.step()) {
      val frameId = st.columnInt(0)
      val time = st.columnDouble(1)
      val polarity = st.columnString(2)
      val scanMode = st.columnInt(3)
      val msMstype = st.columnInt(4)
      val timsId = st.columnInt(5)
      val maxIntensity = st.columnInt(6)
      val summedIntensities = st.columnInt(7)
      val numScans = st.columnInt(8)
      val numPeaks = st.columnInt(9)
      val mzCalibration = st.columnInt(10)
      val temp1 = st.columnDouble(11)
      val temp2 = st.columnDouble(12)
      val timsCalibration = st.columnInt(13)
      val propertyGroup = st.columnInt(14)
      val accumTime = st.columnDouble(15)
      val rampTime = st.columnDouble(16)

      mMap += (frameId -> TimsTOFFrameMetaData(frameId = frameId,
        time = time, polarity = polarity, scanMode = scanMode, msMsType = msMstype,
        timsId = timsId, maxIntensity = maxIntensity, sumIntensity = summedIntensities, numScans = numScans,
        numPeaks = numPeaks, mzCalibration = mzCalibration, temp1 = temp1, temp2 = temp2,
        timsCalibration = timsCalibration, porpertyGroup = propertyGroup, accumTime = accumTime, rampTime = rampTime))
    }
    st.dispose()
    db.dispose()
    // make immutable and return
    scala.collection.immutable.Map[Int, TimsTOFFrameMetaData]() ++ mMap
  }

  /**
   * get a timsTOF raw frame
   * @param frameId id of raw frame that should be fetched
   * @return bruker raw frame
   */
  def getTimsTofRawFrame(frameId: Int): TimsTOFRawFrame = {
    getTimsTofRawFrameNative(frameId = frameId)
  }

  /**
   * hidden native call to opentims++ and bruker SDK for data access
   *
   * @param frameId       id of raw frame that should be fetched
   * @param expPath       path where .d bruker raw file can be found
   * @param brukerLibPath path to bruker SDK
   * @return TimsRawFrame object
   */
  @native private def getTimsTofRawFrameNative(frameId: Int, expPath: String = experimentPath,
                                               brukerLibPath: String = brukerBinaryPath): TimsTOFRawFrame

  /**
   * convert scan number to 1/K0 value
   * @param frameId id of raw frame that should be fetched
   * @param scans vector of scan values to convert
   * @return vector of one-over-k0 values
   */
  def scanToOneOverK0(frameId: Int, scans: Array[Int]): Array[Double] = {
    scanToOneOverK0Native(frameId, scans, experimentPath, brukerBinaryPath)
  }

  /**
   * hidden native call to opentims++ and bruker SDK for data conversion
   * @param frameId id of raw frame that should be fetched
   * @param scans array of scans to convert
   * @param expPath path to experiment
   * @param brukerLibPath path to bruker binary
   * @return array of one-over-k0 values
   */
  @native private def scanToOneOverK0Native(frameId: Int, scans: Array[Int], expPath: String = experimentPath,
                                            brukerLibPath: String = brukerBinaryPath): Array[Double]

  /**
   * convert tof to mz value
   * @param frameId id of raw frame that should be fetched
   * @param tofs vector of tof values to convert
   * @return vector of mz values
   */
  def tofToMz(frameId: Int, tofs: Array[Int]): Array[Double] = {
    tofToMzNative(frameId, tofs, experimentPath, brukerBinaryPath)
  }

  /**
   * hidden native call to opentims++ and bruker SDK for data conversion
   * @param frameId id of raw frame that should be fetched
   * @param tofs array of tof values to convert
   * @param expPath path to experiment
   * @param brukerLibPath path to bruker binary
   * @return array of mz values
   */
  @native private def tofToMzNative(frameId: Int, tofs: Array[Int], expPath: String = experimentPath,
                                            brukerLibPath: String = brukerBinaryPath): Array[Double]
}
