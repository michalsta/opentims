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
D = OpenTIMS('dupa')
D@min_frame
D@max_frame
D@min_scan
D@max_scan
D@min_inv_ion_mobility
D@max_inv_ion_mobility
D@min_mz
D@max_mz
D@min_intensity
D@max_intensity

min_max_measurements(D)
?min_max_measurements

library(devtools)

rm(list=c('all_columns'))
document()
document()
build()
install()
load_all()




