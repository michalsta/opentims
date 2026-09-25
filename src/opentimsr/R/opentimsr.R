# *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
# *   Copyright (C) 2020-2024 Michał Startek and Mateusz Łącki
# *
# *   Licensed under the MIT License. See LICENCE file in the project root for details.


#' @useDynLib opentimsr
#' @importFrom Rcpp sourceCpp
NULL

.onUnload <- function (libpath) {
  library.dynam.unload("opentimsr", libpath)
}

all_columns = c('frame','scan','tof','intensity','mz','inv_ion_mobility','retention_time')


#' TimsTOF data accessor.
#'
#' S4 class that facilitates data queries for TimsTOF data.
#'
#' @slot path.d Path to raw data folder (typically *.d).
#' @slot handle Pointer to raw data.
#' @slot min_frame The index of the minimal frame.
#' @slot max_frame The index of the miximal frame.
#' @slot min_scan The minimal scan number. It is assumed to be equal to 1.
#' @slot max_scan The maximal scan number.
#' @slot min_intensity The minimal value of intensity. Set to 0, but actually 9 is more sensible.
#' @slot max_intensity The maximal intensity: the max over values reported in the frames.
#' @slot min_retention_time The lowest recorded retention time.
#' @slot max_retention_time The highest recorded retention time.
#' @slot min_inv_ion_mobility The minimal recorded inverse ion mobility.
#' @slot max_inv_ion_mobility The maximal recorded inverse ion mobility.
#' @slot min_mz The minimal recorded mass to charge ratio.
#' @slot max_mz The maximal recorded mass to charge ratio.
#' @slot frames A data.frame with information on the frames (contents of the Frames table in the sqlite db).
#' @slot all_columns Names of available columns.
#' @export
setClass('OpenTIMS',
         slots = c(path.d='character',
                   handle='externalptr',
                   min_frame='integer',
                   max_frame='integer',
                   min_scan='integer',
                   max_scan='integer',
                   min_intensity='integer',
                   max_intensity='integer',
                   min_retention_time='numeric',
                   max_retention_time='numeric',
                   min_inv_ion_mobility='numeric',
                   max_inv_ion_mobility='numeric',
                   min_mz='numeric',
                   max_mz='numeric',
                   frames='data.frame',
                   all_columns='character')
)

setMethod('show',
          'OpenTIMS',
          function(object) 
            cat(paste0(class(object),
                        "(",
                        tdf_no_peaks_total(object@handle),
                        ")",
                        "\n",
                        separator='')))

#' Get the overall number of peaks.
#'
#' @param x OpenTIMS data instance.
#' @return The total number of peaks in the dataset.
#' @examples
#' path.d = system.file("extdata", "test.d", package = "opentimsr")
#' setup_opensource()
#' D = OpenTIMS(path.d)
#' length(D) # number of peaks in the dataset
#' CloseTIMS(D)
setMethod('length', 
          'OpenTIMS', 
          function(x) tdf_no_peaks_total(x@handle))

#' Get some frames of data.
#'
#' @param x OpenTIMS data instance.
#' @param i An array of nonzero indices to extract.
#' @return For the requested frames, a data.frame with columns 'frame', 'scan', 'tof' and 'intensity', one row per peak.
#' @examples
#' path.d = system.file("extdata", "test.d", package = "opentimsr")
#' setup_opensource()
#' D = OpenTIMS(path.d)
#' head(D[1])   # first frame
#' head(D[1:2]) # frames 1 and 2
#' CloseTIMS(D)
setMethod("[", 
          signature(x = "OpenTIMS", i = "ANY"),
          function(x, i){
            i = as.integer(i)
            stopifnot(all(x@min_frame <= i, i <= x@max_frame))
            return(tdf_get_indexes(x@handle, i))
        })

#' Select a range of frames to extract.
#'
#' This is similar to using the from:to:by operator in Python.
#'
#' @param x OpenTIMS data instance.
#' @param from The first frame to extract.
#' @param to The last+1 frame to extract. Frame with that number will not get extracted, but some below that number might.
#' @param by Extract each by-th frame.
#' @param na.rm Needed to comply with the generic. There should not be any NA frames, 
#' since the from and to are checked against the limits of the data.
#' @return For the selected frames, a data.frame with columns 'frame', 'scan', 'tof' and 'intensity', one row per peak.
#' @examples
#' path.d = system.file("extdata", "test.d", package = "opentimsr")
#' setup_opensource()
#' D = OpenTIMS(path.d)
#' head(range(D, 1, 3))    # frames 1 and 2
#' head(range(D, 1, 3, 2)) # every second frame, i.e. only frame 1
#' CloseTIMS(D)
setMethod("range", 
          "OpenTIMS",
          function(x, from, to, by=1L, na.rm=FALSE){ 
            from = as.integer(from)
            to  = as.integer(to)
            by  = as.integer(by)
            stopifnot(from >= x@min_frame,
                      to <= x@max_frame + 1,
                      by >= 0)
            tdf_get_range(x@handle, from, to, by)
          })

#' Extract tables from sqlite database analysis.tdf.
#'
#' Export a table from sqlite.
#'
#' @param opentims Instance of OpenTIMS
#' @param names Names to extract from the sqlite database.
#' @return A list of tables.
#' @export
#' @examples
#' path.d = system.file("extdata", "test.d", package = "opentimsr")
#' setup_opensource()
#' D = OpenTIMS(path.d)
#' head(table2df(D, "Frames")$Frames) # extract table "Frames"
#' CloseTIMS(D)
table2df <- function(opentims, names){
    analysis.tdf = file.path(opentims@path.d, 'analysis.tdf')
    sql_conn = DBI::dbConnect(RSQLite::SQLite(), analysis.tdf)
    on.exit(DBI::dbDisconnect(sql_conn))
    tables = lapply(names, function(name) DBI::dbReadTable(sql_conn, name))
    names(tables) = names
    return(tables)
}


#' Extract tables from sqlite database analysis.tdf.
#'
#' @param opentims Instance of OpenTIMS
#' @return Names of tables.
#' @export
#' @examples
#' path.d = system.file("extdata", "test.d", package = "opentimsr")
#' setup_opensource()
#' D = OpenTIMS(path.d)
#' tables_names(D)
#' CloseTIMS(D)
tables_names <- function(opentims){
    analysis.tdf = file.path(opentims@path.d, 'analysis.tdf')
    sql_conn = DBI::dbConnect(RSQLite::SQLite(), analysis.tdf)
    on.exit(DBI::dbDisconnect(sql_conn))
    tables_names = DBI::dbListTables(sql_conn)
    return(tables_names)
}



#' Get OpenTIMS data representation.
#' 
#' @param path.d Path to the TimsTOF '*.d' folder containing the data (requires the folder to contain only 'analysis.tdf' and 'analysis.tdf_bin').
#' @return An object of class \code{OpenTIMS}, used as the data handle by the other functions.
#' @examples
#' path.d = system.file("extdata", "test.d", package = "opentimsr")
#' setup_opensource()
#' D = OpenTIMS(path.d)
#' D[1] # first frame
#' CloseTIMS(D)
#' @importFrom methods new
#' @export
OpenTIMS <- function(path.d){
    # C++ side does not expand '~' nor resolve relative paths.
    path.d = normalizePath(path.d, mustWork=TRUE)
    # getting tables from SQlite
    analysis.tdf = file.path(path.d, 'analysis.tdf')
    sql_conn = DBI::dbConnect(RSQLite::SQLite(), analysis.tdf)
    on.exit(DBI::dbDisconnect(sql_conn))
    frames = DBI::dbReadTable(sql_conn, 'Frames')
    GlobalMetadata = DBI::dbReadTable(sql_conn, 'GlobalMetadata')
    handle = tdf_open(path.d, frames, GlobalMetadata$Key, GlobalMetadata$Value)
    GlobalMetadata = array(GlobalMetadata$Value, dimnames=list(GlobalMetadata$Key))

    ## Extracting basic info on the limits of reported measurements.
    ## Does not include TOF index

    # since we must use RSQLite anyway, we don't have to read it from C++
    # min_frame=as.integer(tdf_min_frame_id(handle)),
    # max_frame=as.integer(tdf_max_frame_id(handle)),
    min_frame = as.integer(min(frames$Id)) # just in case
    max_frame = as.integer(max(frames$Id)) # just in case
    min_scan = 1L
    max_scan = as.integer(max(frames$NumScans))
    min_intensity = 0L
    max_intensity = max(frames$MaxIntensity)
    min_retention_time = min(frames$Time)
    max_retention_time = max(frames$Time)
    min_inv_ion_mobility = as.numeric(GlobalMetadata['OneOverK0AcqRangeLower'])
    max_inv_ion_mobility = as.numeric(GlobalMetadata['OneOverK0AcqRangeUpper'])
    min_mz = as.numeric(GlobalMetadata['MzAcqRangeLower']) # would "float" be 
    max_mz = as.numeric(GlobalMetadata['MzAcqRangeUpper']) # a bad name to use?

    new("OpenTIMS",
        path.d=path.d,
        handle=handle,
        min_frame=min_frame,
        max_frame=max_frame,
        min_scan=min_scan,
        max_scan=max_scan,
        min_intensity=min_intensity,
        max_intensity=max_intensity,
        min_retention_time=min_retention_time,
        max_retention_time=max_retention_time,
        min_inv_ion_mobility=min_inv_ion_mobility,
        max_inv_ion_mobility=max_inv_ion_mobility,
        min_mz=min_mz,
        max_mz=max_mz,
        frames=frames,
        all_columns=all_columns)
}


#' Get border values for measurements.
#'
#' Get the min-max values of the measured variables (except for TOFs, that would require iteration through data rather than parsing metadata).
#' 
#' @param opentims Instance of OpenTIMS.
#' @return data.frame Limits of individual extracted quantities.
#' @export
#' @examples
#' path.d = system.file("extdata", "test.d", package = "opentimsr")
#' setup_opensource()
#' D = OpenTIMS(path.d)
#' min_max_measurements(D) # small data frame with min and max values
#' CloseTIMS(D)
min_max_measurements <- function(opentims){
    data.frame(stat=c('min','max'),
               frame=c(opentims@min_frame,opentims@max_frame),
               scan=c(opentims@min_scan, opentims@max_scan),
               intensity=c(opentims@min_intensity, opentims@max_intensity),
               retention.time=c(opentims@min_retention_time,
                                opentims@max_retention_time),
               inv_ion_mobility=c(opentims@min_inv_ion_mobility,
                                  opentims@max_inv_ion_mobility),
               mz=c(opentims@min_mz, opentims@max_mz))
}


#' Get MS1 frame numbers.
#'
#' @param opentims Instance of OpenTIMS
#' @return Numbers of frames corresponding to MS1, i.e. precursor ions.
#' @export
#' @examples
#' path.d = system.file("extdata", "test.d", package = "opentimsr")
#' setup_opensource()
#' D = OpenTIMS(path.d)
#' MS1(D)
#' CloseTIMS(D)
MS1 <- function(opentims) opentims@frames$Id[opentims@frames$MsMsType == 0]


#' Explore the contentents of the sqlite .tdf database.
#'
#' @param opentims Instance of OpenTIMS
#' @param ... Parameters passed to head and tail functions.
#' @return No return value, called for its side effect of printing the tables.
#' @importFrom utils head tail
#' @export
#' @examples
#' path.d = system.file("extdata", "test.d", package = "opentimsr")
#' setup_opensource()
#' D = OpenTIMS(path.d)
#' if (interactive()) explore.tdf.tables(D)
#' CloseTIMS(D)
explore.tdf.tables <- function(opentims, ...){
    for(table_name in tables_names(opentims)){
        print(table_name)
        df = table2df(opentims, table_name)
        print(head(df,...))
        print(tail(df,...))
        readline("PRESS ENTER")
    }
    print('Get full tables using "table2df".')
}


#' Get the number of peaks per frame.
#'
#' @param opentims Instance of OpenTIMS.
#' @return Number of peaks in each frame.
#' @export
#' @examples
#' path.d = system.file("extdata", "test.d", package = "opentimsr")
#' setup_opensource()
#' D = OpenTIMS(path.d)
#' peaks_per_frame_cnts(D)
#' CloseTIMS(D)
peaks_per_frame_cnts <- function(opentims){
  opentims@frames$NumPeaks
}


#' Get the retention time for each frame.
#'
#' @param opentims Instance of OpenTIMS.
#' @return Retention times corresponding to each frame.
#' @export
#' @examples
#' path.d = system.file("extdata", "test.d", package = "opentimsr")
#' setup_opensource()
#' D = OpenTIMS(path.d)
#' retention_times(D)
#' CloseTIMS(D)
retention_times <- function(opentims){
  opentims@frames$Time
}


#' Query for raw data.
#'
#' Get the raw data from Bruker's 'tdf_bin' format.
#' Defaults to both raw data ('frame','scan','tof','intensity') and its tranformations into physical units ('mz','inv_ion_mobility','retention_time').
#'
#' @param opentims Instance of OpenTIMS.
#' @param frames Vector of frame numbers to extract.
#' @param columns Vector of columns to extract. Defaults to all columns.
#' @return data.frame with selected columns.
#' @export
#' @examples
#' path.d = system.file("extdata", "test.d", package = "opentimsr")
#' setup_opensource()
#' D = OpenTIMS(path.d)
#' query(D, c(1, 2)) # extract all columns
#' query(D, c(1, 2), columns=c('scan','intensity')) # only 'scan' and 'intensity'
#' CloseTIMS(D)
query <- function(opentims,
                 frames,
                 columns=all_columns){
  col = opentims@all_columns %in% columns

  if(!all(columns %in% all_columns)) stop(paste0("Wrong column names. Choose among:\n", paste0(all_columns, sep=" ", collapse="")))

  df = tdf_extract_frames( opentims@handle,
                           frames,
                           get_frames = col[1],
                           get_scans = col[2],
                           get_tofs = col[3],
                           get_intensities = col[4],
                           get_mzs = col[5],
                           get_inv_ion_mobilities = col[6],
                           get_retention_times = col[7] )

  as.data.frame(df)[,columns, drop=F] # What an idiot invented drop=T as default????
}


#' Query for raw data.
#'
#' Get the raw data from Bruker's 'tdf_bin' format.
#' Defaults to both raw data ('frame','scan','tof','intensity') and its tranformations into physical units ('mz','inv_ion_mobility','retention_time').
#'
#' We assume 'from' <= 'to'.
#'
#' @param opentims Instance of OpenTIMS.
#' @param from First frame to extract.
#' @param to Last frame to extract.
#' @param by Every by-th frame gets extracted (starting from the first one).
#' @param columns Vector of columns to extract. Defaults to all columns.
#' @return data.frame with selected columns.
#' @export
#' @examples
#' path.d = system.file("extdata", "test.d", package = "opentimsr")
#' setup_opensource()
#' D = OpenTIMS(path.d)
#' query_slice(D, 1, 2)    # frames 1 to 2
#' query_slice(D, 1, 2, 2) # every second frame between 1 and 2
#' query_slice(D, 1, 2, columns=c('scan','intensity')) # only 'scan' and 'intensity'
#' CloseTIMS(D)
query_slice <- function(opentims,
                       from=NULL,
                       to=NULL,
                       by=1,
                       columns=all_columns){
  col = opentims@all_columns %in% columns

  # Border conditions.
  if(is.null(from)) from = opentims@min_frame
  if(is.null(to)) to = opentims@max_frame

  df = tdf_extract_frames_slice( opentims@handle,
                                 from,
                                 to + 1, # For R numerations to work. 
                                 by,
                                 get_frames = col[1],
                                 get_scans = col[2],
                                 get_tofs = col[3],
                                 get_intensities = col[4],
                                 get_mzs = col[5],
                                 get_inv_ion_mobilities = col[6],
                                 get_retention_times = col[7] )
  as.data.frame(df)[,columns, drop=F] # What an idiot invented drop=T as default????
}


get_left_frame <- function(x,y) ifelse(x > y[length(y)], NA, findInterval(x, y, left.open=T) + 1)
get_right_frame <- function(x,y) ifelse(x < y[1], NA, findInterval(x, y, left.open=F))


#' Get the retention time for each frame.
#'
#' Extract all frames corresponding to retention times inside [min_retention_time, max_retention_time] closed borders interval.
#'
#' @param opentims Instance of OpenTIMS.
#' @param min_retention_time Lower boundry on retention time.
#' @param max_retention_time Upper boundry on retention time.
#' @param columns Vector of columns to extract. Defaults to all columns.
#' @return data.frame with selected columns.
#' @export
#' @examples
#' path.d = system.file("extdata", "test.d", package = "opentimsr")
#' setup_opensource()
#' D = OpenTIMS(path.d)
#' rt_query(D, 0.6, 0.7) # frames recorded between 0.6 and 0.7 seconds
#' CloseTIMS(D)
rt_query <- function(opentims,
                    min_retention_time,
                    max_retention_time,
                    columns=all_columns){
  RTS = retention_times(opentims)

  min_frame = get_left_frame(min_retention_time, RTS)
  max_frame = get_right_frame(max_retention_time, RTS)

  if(is.na(min_frame) | is.na(max_frame))
    stop("The [min_retention_time,max_retention_time] interval does not hold any data.")

  query_slice(opentims,
              from=min_frame,
              to=max_frame,
              columns=columns)
}


#' Get Bruker's code needed for running proprietary time of flight to mass over charge and scan to drift time conversion. 
#'
#' Bruker's 'timsdata' library is proprietary software owned by Bruker. It is not
#' part of opentimsr and is not distributed with it: this function downloads it
#' only when called, from "https://github.com/MatteoLacki/opentims_bruker_bridge",
#' and by using it you agree to the license terms stated there.
#' The library is available only for Linux and Windows. opentimsr works without it,
#' using its built-in open-source converters (see \code{\link{setup_opensource}}).
#'
#' @param target.folder Folder where to store the 'dll' or 'so' file.
#' @param net_url The url with location of all files.
#' @param mode Which mode to use when downloading a file?
#' @param ... Other parameters to 'download.file'.
#' @return Path to the output 'timsdata.dll' on Windows and 'libtimsdata.so' on Linux.
#' @importFrom utils download.file
#' @export
#' @examples
#' \dontrun{
#' download_bruker_proprietary_code("your/prefered/destination/folder")
#' }
download_bruker_proprietary_code <- function(
  target.folder, 
  net_url=paste0("https://raw.githubusercontent.com/MatteoLacki/",
                 "opentims_bruker_bridge/main/opentims_bruker_bridge/"),
  mode="wb",
  ...){
  sys_info = Sys.info()
  if(sys_info['sysname'] == "Linux"){
    message("Downloading 64-bit Linux binary.")
    url_ending = file="libtimsdata.so"
  }
  if(sys_info['sysname'] == "Windows"){
    file = "timsdata.dll"
    if(sys_info['machine'] == "x86-64"){
      message("Downloading Windows 64-bit binary.")
      url_ending="win64/timsdata.dll"   
    } else {
      message("Downloading Windows 32-bit binary.")
      url_ending="win32/timsdata.dll"
    }
  }
  url = paste0(net_url, url_ending)
  target.file = file.path(target.folder, file)
  message("Downloading from: ", url)
  download.file(url, target.file, mode="wb", ...)

  target.file
}



#' Close the TIMS data handle and release all resources.
#'
#' Calling this method is not mandatory - the resources will anyway
#' be cleanly released when the garbage collector decides to
#' destroy the OpenTIMS data handle. The only purpose of this
#' method is to enable them to be explicitly released earlier than that.
#'
#' @param opentims Instance of OpenTIMS.
#' @return No return value, called for its side effect of closing the data handle.
#' @export
#' @examples
#' path.d = system.file("extdata", "test.d", package = "opentimsr")
#' setup_opensource()
#' D = OpenTIMS(path.d)
#' CloseTIMS(D)
CloseTIMS <- function(opentims){
    tdf_close(opentims@handle)
}


#' Dynamically link Bruker's DLL to enable tof-mz and scan-inv_ion_mobility conversion.
#'
#' Bruker's 'timsdata' library is proprietary software owned by Bruker. It is not
#' part of opentimsr and is not distributed with it; obtain it with
#' \code{\link{download_bruker_proprietary_code}}, whose license terms apply.
#' The library is available only for Linux and Windows. opentimsr works without it,
#' using its built-in open-source converters (see \code{\link{setup_opensource}}).
#'
#' @param path Path to the 'libtimsdata.so' on Linux or 'timsdata.dll' on Windows, as produced by 'download_bruker_proprietary_code'.
#' @return No return value, called for its side effect of switching the conversion backend.
#' @export
#' @examples
#' \dontrun{
#' so_path = download_bruker_proprietary_code("your/prefered/destination/folder")
#' setup_bruker_so(so_path)
#' }
setup_bruker_so <- function(path) .setup_bruker_so(path)


#' Use the built-in open-source tof-to-mz and scan-to-inv_ion_mobility converters.
#'
#' Activates the built-in open-source converters for tof-to-mz and
#' scan-to-inv_ion_mobility transformations. No proprietary Bruker libraries
#' are required. This is the default mode; call this function before querying
#' data if you have not called \code{\link{setup_bruker_so}}.
#'
#' @return No return value, called for its side effect of switching the conversion backend.
#' @examples
#' setup_opensource()
#' @export
setup_opensource <- function() invisible(.setup_opensource())


#' Set the number of threads to be used for data processing by OpenTIMS
#'
#' Frames are decoded in parallel by \code{\link{query}}, \code{\link{query_slice}},
#' \code{\link{rt_query}}, \code{[} and \code{range}. By default all detected cores
#' are used; a value of 0 restores that default. When the environment variable
#' \code{_R_CHECK_LIMIT_CORES_} is set (as during \code{R CMD check --as-cran}),
#' the default is two threads.
#'
#' @param n The number of worker threads to be used.
#' @return No return value, called for its side effect of setting the number of threads.
#' @export
#' @examples
#' opentims_set_threads(1)
opentims_set_threads <- function(n){
    tdf_set_num_threads(n)
}


.onLoad <- function(libname, pkgname){
    # CRAN checks may use at most two cores.
    limit_cores = tolower(Sys.getenv("_R_CHECK_LIMIT_CORES_", ""))
    if(nzchar(limit_cores) && limit_cores != "false") tdf_set_num_threads(2)
}
