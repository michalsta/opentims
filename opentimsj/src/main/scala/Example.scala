import com.almworks.sqlite4java.SQLiteConnection
import timsj.{TimsTOFExperimentReader, TimsTOFFrameMetaData}

import java.io.File

object Example extends App {

  // CAUTION: libtimstofexperimentreader.so/.dll needs to be accessible via
  // -Djava.library.path="path/to/lib.*", set path accordingly
  // libtimsdata.so/.dll should be placed into resources folder of this project
  if(args.length != 3){
    println("usage: <path/to/timsTOFExperiment.d> <path/to/bruker/binary.so> <frameId>")
    System.exit(1)
  }

  val frameId = args(2).toInt

  // load native library
  System.loadLibrary("timstofexperimentreader")

  // create handle
  val timsTofExpReader = new TimsTOFExperimentReader(args(0), args(1))

  // fetch meta data
  val metaData = timsTofExpReader.getFrameMetaData

  // fetch frame
  val frame = timsTofExpReader.getTimsTofRawFrame(frameId)

  // check output
  println(metaData.getOrElse(frameId, TimsTOFFrameMetaData()))
  println(frame.retentionTime)
  println(frame)
}
