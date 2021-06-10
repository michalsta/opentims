package timsj

/**
 * container for raw data extracted with opentims++
 *
 * @param frameId       frame identifier
 * @param retentionTime retention time in seconds
 * @param scanIds       scan identifier
 * @param tofs          array of tof values
 * @param intensities   array of intensity values
 * @param mzs           array of mz values
 * @param oneOverK0s    array of inverse mobility values
 */
case class TimsTOFRawFrame(frameId: Int, retentionTime: Double, scanIds: Array[Int], tofs: Array[Int],
                           intensities: Array[Int], mzs: Array[Double], oneOverK0s: Array[Double]) {

  /**
   * some convenient statistics, compare to meta data to check extraction
   */
  lazy val maxI: Int = intensities.max
  lazy val mzByMaxI: Double = intensities.zip(mzs).maxBy(_._1)._2
  lazy val scanByMaxI: Int = intensities.zip(scanIds).maxBy(_._1)._2
  lazy val sumI: Int = intensities.sum

  override def toString: String = {
    s"frameId: $frameId\n" + s"retention time: $retentionTime\n" +
      s"max intensity peak: $maxI\n" +
      s"mz by max intensity: $mzByMaxI\n" +
      s"sumI: $sumI"
  }

  /**
   * get a bruker raw frame as a collection of spectra grouped by scan
   * @return a split frame as collection of spectra grouped by scan
   */
  def groupToSpectra: Vector[(Int, Double, Int, Vector[Double], Vector[Int])] = {
    // zip to tripples (scan, i, mz)
    val tripples = scanIds.zip(intensities).zip(mzs).map {
      case ((scan, mz), i) => (scan, mz, i)
    }

    // group to individual spectra by scan
    val r = tripples.groupBy(_._1).map {
      case (key, values) => (key, values.map {
        case (_, i, mz) => (mz, i)
      })
    }

    // extract as individual vectors
    val grouped = r.map {
      case (scan, spec) => (frameId, retentionTime, scan, spec.map(_._1).toVector, spec.map(_._2).toVector)
    }.toVector

    grouped
  }

}
