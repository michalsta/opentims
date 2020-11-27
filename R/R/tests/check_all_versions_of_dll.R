library(devtools)
library(opentims)
library(data.table)
library(microbenchmark)
library(hexbin)

load_all()
library(data.table)
path_dll_old = '/home/matteo/Projects/opentims/opentims_bruker_bridge/opentims_bruker_bridge/libtimsdata.so'
path_dll_2.7.0 = '/home/matteo/Projects/opentims/tdf_sdk_2_7_0/linux64/libtimsdata.so'
path_dll_2.8.7 = '/home/matteo/Projects/opentims/tdf_sdk_2_8_7/linux64/libtimsdata.so'

path = '/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d'

# setup_bruker_so(path_dll_old)
# setup_bruker_so(path_dll_2.7.0)
path_dll = download_bruker_proprietary_code('/home/matteo/Projects/opentims')

setup_bruker_so(path_dll_2.8.7)
D = OpenTIMS(path)

# Z.old = as.data.table(query(D, frames=1:10))
# save(Z.old, file='/home/matteo/Projects/opentims/Z.old')

# Z.2.7.0 = as.data.table(query(D, frames=1:10))
# save(Z.2.7.0, file='/home/matteo/Projects/opentims/Z.2.7.0')

# Z.2.8.7 = as.data.table(query(D, frames=1:10))
# save(Z.2.8.7, file='/home/matteo/Projects/opentims/Z.2.8.7')

# load('/home/matteo/Projects/opentims/Z.old')
# load('/home/matteo/Projects/opentims/Z.2.7.0')
# load('/home/matteo/Projects/opentims/Z.2.8.7')

# all.equal(Z.old, Z.2.8.7)
