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


setClass('OpenTIMS',
         slots=c(path='character',
                 db_path='character',
                 handle='externalptr',
                 min_frame='integer',
                 max_frame='integer'),
         validity=function(object){
            if(object@db_path=='') dir.exists(object@path)
            else file.exists(object@path)&file.exists(object@db_path)
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
            tdf_get_indexes(x@handle, i)
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

#' Get OpenTIMS data representation.
#'
#' 
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
OpenTIMS = function(path, db_path=''){
    handle = ifelse(db_path == '',
                    tdf_open_dir(path),
                    tdf_open(path, db_path))
    new("OpenTIMS",
        path=path, 
        db_path=db_path,
        handle=handle,
        min_frame=as.integer(tdf_min_frame_id(handle)),
        max_frame=as.integer(tdf_max_frame_id(handle)))
}
