library(opentimsr)
# library(data.table)
# library(ggplot2)
library(stringr)
# library(patchwork)

raw_folders = Sys.glob("/home/matteo/raw_data/external/opentims_validation/*/*.d")
file_names = basename(raw_folders)
file_names_short = str_sub(file_names, 17, -3)
raw_folder = raw_folders[1]

setup_bruker_so('/home/matteo/libtimsdata.so')
all_columns = c('frame','scan','tof','intensity','mz','inv_ion_mobility','retention_time')

D = OpenTIMS(raw_folder)
D@min_frame
D@max_frame

Y = query(D, 1:12, "inv_ion_mobility")
X = D[1:12] # add this back!
dim(X)
dim(Y)

rts = retention_times(D)
min_retention_time = rts[1]
max_retention_time = rts[length(rts)]



self.GlobalMetadata = self.table2df('GlobalMetadata')
self.min_mz = float(self.GlobalMetadata.Value['MzAcqRangeLower'])
self.max_mz = float(self.GlobalMetadata.Value['MzAcqRangeUpper'])
self.min_inv_ion_mobility = float(self.GlobalMetadata.Value['OneOverK0AcqRangeLower'])
self.max_inv_ion_mobility = float(self.GlobalMetadata.Value['OneOverK0AcqRangeUpper'])

colnames(D[2, "inv_ion_mobility"])


#' Get border values for measurements.
#'
#' Get the min-max values of the measured variables (except for TOFs, that would require iteration through data rather than parsing metadata).
#'
#' @param opentims Instance of OpenTIMS.
#' @param long Return results in a long format.
#' @return data.frame with columns corresponding to 
#' @export
#' @examples
#' \dontrun{
#' D = OpenTIMS('path/to/your/folder.d')
#' print(query(D, c(1,20, 53)) # extract all columns
#' print(query(D, c(1,20, 53), columns=c('scan','intensity')) # only 'scan' and 'intensity'
#' }
measurement_borders = function(opentims, long=TRUE){
	if(long){
		res = data.frame(measurement=c(  'frame',
										 'retention_time',
										 'scan',
										 'inv_ion_mobility',
										 'mz'),
				   		 min=c( opentims@min_frame, 
				   		 		opentims@min_retention_time, 
				   		 		opentims@min_scan,
				   		 		opentims@min_inv_ion_mobility,
				   		 		opentims@min_mz ), 
				   		 max=c( opentims@max_frame,
				   		 		opentims@max_retention_time,
				   		 		opentims@max_scan,
				   		 		opentims@max_inv_ion_mobility,
				   		 		opentims@max_mz)) 
	} else {
		res = data.frame(min.frame=opentims@min_frame,
				   	     max.frame=opentims@max_frame,
					     min.retention.time=opentims@min_retention_time, 
					     max.retention_time=opentims@max_retention_time,
					     min.scan=opentims@min_scan,
					     max.scan=opentims@max_scan,
					     min.inv_ion_mobility=opentims@min_inv_ion_mobility,
					     max.inv_ion_mobitliy=opentims@max_inv_ion_mobility,
					     min.mz=opentims@min_mz,
					     max.mx=opentims@max_mz)
	}
	return(res)
}


									
