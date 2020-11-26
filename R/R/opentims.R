# *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
# *   Copyright (C) 2020 Michał Startek and Mateusz Łącki
# *
# *   This program is free software: you can redistribute it and/or modify
# *   it under the terms of the GNU General Public License, version 3 only,
# *   as published by the Free Software Foundation.
# *
# *   This program is distributed in the hope that it will be useful,
# *   but WITHOUT ANY WARRANTY; without even the implied warranty of
# *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# *   GNU General Public License for more details.
# *
# *   You should have received a copy of the GNU General Public License
# *   along with this program.  If not, see <https://www.gnu.org/licenses/>.


#' @useDynLib  opentims
#' @importFrom Rcpp sourceCpp
NULL

.onUnload <- function (libpath) {
  library.dynam.unload("opentims", libpath)
}


#' TimsTOF data accessor.
#'
#' S4 class that facilitates data queries for TimsTOF data.
#'
#' @slot path.d Path to raw data folder (typically *.d).
#' @slot handle Pointer to raw data.
#' @slot min_frame The index of the minimal frame.
#' @slot max_frame The index of the miximal frame.
#' @slot frames A data.frame with information on the frames (contents of the Frames table in the sqlite db).
#' @slot windows A data.frame with information on the windows.
#' @slot MS1 Indices of frames corresponding to MS1, i.e. precursor ions.
#' @export
setClass('OpenTIMS',
         slots = c(path.d='character',
                   handle='externalptr',
                   min_frame='integer',
                   max_frame='integer',
                   frames='data.frame'),
         validity = function(object){
            d.folder = file.exists(object@path.d) 
            if(!d.folder) print('The folder with data (typically named *.d) does not exist.')
          
            d.folder.analysis.tdf = file.exists(file.path(object@path.d, 'analysis.tdf'))
            if(!d.folder.analysis.tdf) print('The .d folder does not contain the sqlite data-base called "analysis.tdf".')
          
            d.folder.analysis.tdf_bin = file.exists(file.path(object@path.d, 'analysis.tdf_bin'))
            if(!d.folder.analysis.tdf_bin) print('The .d folder does not contain the raw data file called "analysis.tdf_bin".')

            d.folder & d.folder.analysis.tdf & d.folder.analysis.tdf_bin
         }
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
setMethod('length', 
          'OpenTIMS', 
          function(x) tdf_no_peaks_total(x@handle))

#' Get some frames of data.
#'
#' @param x OpenTIMS data instance.
#' @param i An array of nonzero indices to extract. 
setMethod("[", 
          signature(x = "OpenTIMS", i = "ANY"),
          function(x, i){
            i = as.integer(i)
            stopifnot(all(x@min_frame <= i, i <= x@max_frame))
            return(tdf_get_indexes(x@handle, i))
        })


#' Select a range of frames to extract.
#'
#' This is similar to using the start:stop:step operator in Python.
#'
#' @param x OpenTIMS data instance.
#' @param start The first frame to extract.
#' @param stop The last+1 frame to extract. Frame with that number will not get extracted, but some below that number might.
#' @param step The step
setMethod("range", 
          "OpenTIMS",
          function(x, start, stop, step=1L){ 
            start = as.integer(start)
            stop  = as.integer(stop)
            step  = as.integer(step)
            stopifnot(start >= x@min_frame,
                      stop <= x@max_frame + 1,
                      step >= 0)
            tdf_get_range(x@handle, start, stop, step)
          })



#' Extract tables from sqlite database analysis.tdf.
#'
#' @param opentims Instance of OpenTIMS
#' @param names Names to extract from the sqlite database.
#' @return A list of tables.
#' @export
table2df = function(opentims, names){
    analysis.tdf = file.path(opentims@path.d, 'analysis.tdf')
    sql_conn = DBI::dbConnect(RSQLite::SQLite(), analysis.tdf)
    tables = lapply(names, function(name) DBI::dbReadTable(sql_conn, name))
    DBI::dbDisconnect(sql_conn)
    return(tables)
}


#' Extract tables from sqlite database analysis.tdf.
#'
#' @param opentims Instance of OpenTIMS
#' @return Names of tables.
#' @export
tables_names = function(opentims, names){
    analysis.tdf = file.path(opentims@path.d, 'analysis.tdf')
    sql_conn = DBI::dbConnect(RSQLite::SQLite(), analysis.tdf)
    tables_names = DBI::dbListTables(sql_conn)
    DBI::dbDisconnect(sql_conn)
    return(tables_names)
}



#' Get OpenTIMS data representation.
#' 
#' @param path.d Path to the TimsTOF '*.d' folder containing the data (requires the folder to contain only 'analysis.tdf' and 'analysis.tdf_bin').
#' @examples
#' \dontrun{
#' D = OpenTIMS(path_to_.d_folder)
#' D[1] # First frame.
#' }
#' @importFrom methods new
#' @export
OpenTIMS = function(path.d){
    # getting tables from SQlite 
    analysis.tdf = file.path(path.d, 'analysis.tdf')
    sql_conn = DBI::dbConnect(RSQLite::SQLite(), analysis.tdf)
    frames = DBI::dbReadTable(sql_conn, 'Frames')
    DBI::dbDisconnect(sql_conn)
    handle = tdf_open(path.d)

    new("OpenTIMS",
        path.d=path.d,
        handle=handle,
        min_frame=as.integer(tdf_min_frame_id(handle)),
        max_frame=as.integer(tdf_max_frame_id(handle)),
        frames=frames)
}

#' Get MS1 frame numbers.
#'
#' @param opentims Instance of OpenTIMS
#' @return Numbers of frames corresponding to MS1, i.e. precursor ions.
#' @export
MS1 = function(opentims) opentims@frames$Id[opentims@frames$MsMsType == 0]


#' Explore the contentents of the sqlite .tdf database.
#'
#' @param opentims Instance of OpenTIMS
#' @param ... Parameters passed to head and tail functions.
#' @export
explore.tdf.tables = function(opentims, ...){
    for(table_name in tables_names(opentims)){
        print(table_name)
        df=table2dt(opentims, table_name)
        print(head(df,...))
        print(tail(df,...))
        readline("PRESS ENTER")
    }
    print('Get full tables using "table2df".')
}


#' Get the number of peaks per frame.
#'
#' @param opentims Instance of OpenTIMS.
#' @export
peaks_per_frame_cnts = function(opentims){
  opentims@frames$NumPeaks
}


#' Get the retention time for each frame.
#'
#' @param opentims Instance of OpenTIMS.
#' @export
peaks_per_frame_cnts = function(opentims){
  opentims@frames$Time
}


#' Query for raw data.
#'
#' Get the raw data from Bruker's 'tdf_bin' format.
#' Extract either by frame numbers ('frames') or by starting frame ('start'), ending frame (inclusive, 'end'), and step in frames ('step'), like in the 'seq' function.
#' If 'frames' and one of the end will not be specified, the minimal or maximal frame will be chosen instead.
#' If both 'frames', 'start' and 'end' are left at default, all of the data will be copied into RAM. Like ALL, so don't do it, unless you have a lot of RAM.
#' Defaults to both raw data ('frame','scan','tof','intensity') and its tranformations into physical units ('mz','dt','rt').
#'
#' @param opentims Instance of OpenTIMS.
#' @param frames Vector of frame numbers to extract.
#' @param start Vector of frame numbers to extract.
#' @param columns Vector of columns to extract. Defaults to all columns.
#' @export
query = function(opentims,
                 frames = NULL,
                 start = NULL,
                 stop = NULL,
                 step = 1L,
                 columns = c('frame','scan','tof','intensity','mz','dt','rt')){

  all_columns = c('frame','scan','tof','intensity','mz','dt','rt')
  col = all_columns %in% columns

  if(!is.null(frames)){

    df = tdf_extract_frames( opentims@handle,
                             frames,
                             get_frames = col[1],
                             get_scans = col[2],
                             get_tofs = col[3],
                             get_intensities = col[4],
                             get_mzs = col[5],
                             get_dts = col[6],
                             get_rts = col[7] )

  } else {
    
    if(is.null(start)) start = opentims@min_frame
    if(is.null(stop)) stop = opentims@max_frame + 1

    df = tdf_extract_frames_slice( opentims@handle,
                                   start,
                                   stop,
                                   step,
                                   get_frames = col[1],
                                   get_scans = col[2],
                                   get_tofs = col[3],
                                   get_intensities = col[4],
                                   get_mzs = col[5],
                                   get_dts = col[6],
                                   get_rts = col[7] )
  }

  as.data.frame(df)
}




#' Get the retention time for each frame.
#'
#' @param opentims Instance of OpenTIMS.
#' @param frames Vector of frame numbers to extract.
#' @param columns Vector of columns to extract. Defaults to all columns.
#' @export
rt_query = function(opentims,
                 frames,
                 columns=c('frame','scan','tof','intensity','mz','dt','rt')){
  

}
