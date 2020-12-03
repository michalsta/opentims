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


#' @useDynLib opentimsr
#' @importFrom Rcpp sourceCpp
NULL

.onUnload <- function (libpath) {
  library.dynam.unload("opentimsr", libpath)
}

# The order of these matters!!!
all_columns = c('frame','scan','tof','intensity','mz','dt','rt')


#' TimsTOF data accessor.
#'
#' S4 class that facilitates data queries for TimsTOF data.
#'
#' @slot path.d Path to raw data folder (typically *.d).
#' @slot handle Pointer to raw data.
#' @slot min_frame The index of the minimal frame.
#' @slot max_frame The index of the miximal frame.
#' @slot frames A data.frame with information on the frames (contents of the Frames table in the sqlite db).
#' @slot all_columns Names of available columns.
#' @export
setClass('OpenTIMS',
         slots = c(path.d='character',
                   handle='externalptr',
                   min_frame='integer',
                   max_frame='integer',
                   frames='data.frame',
                   all_columns='character'),
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
#' This is similar to using the from:to:by operator in Python.
#'
#' @param x OpenTIMS data instance.
#' @param from The first frame to extract.
#' @param to The last+1 frame to extract. Frame with that number will not get extracted, but some below that number might.
#' @param by The by
setMethod("range", 
          "OpenTIMS",
          function(x, from, to, by=1L){ 
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
tables_names = function(opentims){
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
        frames=frames,
        all_columns=all_columns)
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
#' @importFrom utils head tail
#' @export
explore.tdf.tables = function(opentims, ...){
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
peaks_per_frame_cnts = function(opentims){
  opentims@frames$NumPeaks
}


#' Get the retention time for each frame.
#'
#' @param opentims Instance of OpenTIMS.
#' @return Retention times corresponding to each frame.
#' @export
rts = function(opentims){
  opentims@frames$Time
}


#' Query for raw data.
#'
#' Get the raw data from Bruker's 'tdf_bin' format.
#' Defaults to both raw data ('frame','scan','tof','intensity') and its tranformations into physical units ('mz','dt','rt').
#'
#' @param opentims Instance of OpenTIMS.
#' @param frames Vector of frame numbers to extract.
#' @param columns Vector of columns to extract. Defaults to all columns.
#' @return data.frame with selected columns.
#' @export
query = function(opentims,
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
                           get_dts = col[6],
                           get_rts = col[7] )

  as.data.frame(df)[,columns, drop=F] # What an idiot invented drop=T as default???? 
}


#' Query for raw data.
#'
#' Get the raw data from Bruker's 'tdf_bin' format.
#' Defaults to both raw data ('frame','scan','tof','intensity') and its tranformations into physical units ('mz','dt','rt').
#'
#' We assume 'from' <= 'to'.
#'
#' @param opentims Instance of OpenTIMS.
#' @param from First frame to extract.
#' @param to Last frame to extract.
#' @param step Every step-th frame gets extracted (starting from the first one).
#' @param columns Vector of columns to extract. Defaults to all columns.
#' @return data.frame with selected columns.
#' @export
query_slice = function(opentims, from=NULL, to=NULL, step=1L, columns=all_columns){

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
                                 get_dts = col[6],
                                 get_rts = col[7] )
  as.data.frame(df)[,columns, drop=F] # What an idiot invented drop=T as default????
}


get_left_frame = function(x,y) ifelse(x > y[length(y)], NA, findInterval(x, y, left.open=T) + 1)
get_right_frame = function(x,y) ifelse(x < y[1], NA, findInterval(x, y, left.open=F))


#' Get the retention time for each frame.
#'
#' Extract all frames corresponding to retention times inside [min_rt, max_rt] closed borders interval.
#'
#' @param opentims Instance of OpenTIMS.
#' @param min_rt Lower boundry on retention time.
#' @param max_rt Upper boundry on retention time.
#' @param columns Vector of columns to extract. Defaults to all columns.
#' @return data.frame with selected columns.
#' @export
rt_query = function(opentims,
                    min_rt,
                    max_rt,
                    columns=c('frame','scan','tof','intensity','mz','dt','rt')){
  RTS = rts(opentims)

  min_frame = get_left_frame(min_rt, RTS)
  max_frame = get_right_frame(max_rt, RTS)

  if(is.na(min_frame) | is.na(max_frame))
    stop("The [min_rt,max_rt] interval does not hold any data.")

  query_slice(opentims,
              from=min_frame,
              to=max_frame,
              columns=columns)
}


#' Get Bruker's code needed for running proprietary time of flight to mass over charge and scan to drift time conversion. 
#'
#' By using this function you aggree to terms of license precised in "https://github.com/MatteoLacki/opentims_bruker_bridge".
#' The conversion, due to independent code-base restrictions, are possible only on Linux and Windows operating systems.
#' Works on full open-source solution are on the way. 
#'
#' @param target.folder Folder where to store the 'dll' or 'so' file.
#' @param net_url The url with location of all files.
#' @param mode Which mode to use when downloading a file?
#' @param ... Other parameters to 'download.file'.
#' @return Path to the output 'timsdata.dll' on Windows and 'libtimsdata.so' on Linux.
#' @importFrom utils download.file
#' @export
download_bruker_proprietary_code = function(
  target.folder, 
  net_url=paste0("https://github.com/MatteoLacki/opentims_bruker_bridge/",
                 "raw/main/opentims_bruker_bridge/"),
  mode="wb",
  ...){
  sys_info = Sys.info()
  if(sys_info['sysname'] == "Linux"){
    print("Welcome to a real OS.")
    url_ending = file="libtimsdata.so"
  }
  if(sys_info['sysname'] == "Windows"){
    print("Detected Windows. Like seriously?")
    file = "timsdata.dll"
    if(sys_info['machine'] == "x86-64"){
      url_ending="win64/timsdata.dll"   
    } else {
      print("Assuming 32 bits")
      url_ending="win32/timsdata.dll"
    }
  }
  url = paste0(net_url, url_ending)
  target.file = file.path(target.folder, file)
  print(paste0("Downloading from: ", url))
  download.file(url, target.file, mode="wb", ...)

  target.file
}


#' Dynamically link Bruker's DLL to enable tof-mz and scan-dt conversion.
#'
#' By using this function you aggree to terms of license precised in "https://github.com/MatteoLacki/opentims_bruker_bridge".
#' The conversion, due to independent code-base restrictions, are possible only on Linux and Windows operating systems.
#' Works on full open-source solution are on the way. 
#'
#' @param path Path to the 'libtimsdata.so' on Linux or 'timsdata.dll' on Windows, as produced by 'download_bruker_proprietary_code'.
#' @export
setup_bruker_so = function(path) .setup_bruker_so(path)

