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
#' @slot analysis.tdf_bin Path to raw data (typically *.d/analysis.tdf_bin).
#' @slot analysis.tdf Path to sqlite database (typically *.d/analysis.tdf).
#' @slot handle Pointer to raw data.
#' @slot min_frame The index of the minimal frame.
#' @slot max_frame The index of the miximal frame.
#' @slot frames A data.frame with information on the frames (contents of the Frames table in the sqlite db).
#' @slot windows A data.frame with information on the windows.
#' @slot MS1 Indices of frames corresponding to MS1, i.e. precursor ions.
#' @slot MS2 Indices of frames corresponding to MS2, i.e. fragment ions.
#' @slot analysis.tdf.table_names Names of tables in the accompanying sqlite database.
#' @export
setClass('OpenTIMS',
         slots=c(analysis.tdf_bin='character',
                 analysis.tdf='character',
                 handle='externalptr',
                 min_frame='integer',
                 max_frame='integer',
                 frames='data.frame'),
         validity=function(object){
            analysis.tdf_bin_OK = file.exists(object@analysis.tdf_bin)
            if(!analysis.tdf_bin_OK) print('Missing analysis.tdf_bin')
            analysis.tdf_OK = file.exists(object@analysis.tdf)
            if(!analysis.tdf_OK) print('Missing analysis.tdf')
            analysis.tdf_bin_OK & analysis.tdf_OK
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
    sql_conn = DBI::dbConnect(RSQLite::SQLite(), opentims@analysis.tdf)
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
    sql_conn = DBI::dbConnect(RSQLite::SQLite(), opentims@analysis.tdf)
    tables_names = DBI::dbListTables(sql_conn)
    DBI::dbDisconnect(sql_conn)
    return(tables_names)
}



#' Get OpenTIMS data representation.
#' 
#' @param path Path to the TimsTOF .d folder. Otherwise direct path to .tdf_bin file containing the raw data.
#' @param db_path Path to the sqlite database with the .tdf extension. Only supply it when 'path' corresponded to raw data file. If 'path' corresponds to the whole folder, you can skip this argument.
#' @examples
#' \dontrun{
#' D = OpenTIMS(path_to_.d_folder)
#' D[1] # First frame.
#' }
#' @importFrom methods new
#' @export
OpenTIMS = function(path, analysis.tdf=''){
    if(analysis.tdf==''){
        analysis.tdf_bin = file.path(path,'analysis.tdf_bin')
        analysis.tdf = file.path(path,'analysis.tdf')
    } else {
        analysis.tdf = path
    }

    # getting necessary tables from the accompanying sqlite database. 
    sql_conn = DBI::dbConnect(RSQLite::SQLite(), analysis.tdf)
    frames = DBI::dbReadTable(sql_conn, 'Frames')
    DBI::dbDisconnect(sql_conn)

    handle = tdf_open(analysis.tdf_bin, analysis.tdf)

    new("OpenTIMS",
        analysis.tdf_bin=analysis.tdf_bin, 
        analysis.tdf=analysis.tdf,
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


#' Get MS2 frame numbers.
#'
#' @param opentims Instance of OpenTIMS
#' @return Numbers of frames corresponding to MS2, i.e. fragment ions.
#' @export
MS2 = function(opentims) opentims@frames$Id[opentims@frames$MsMsType == 9]


