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
# path_dll = download_bruker_proprietary_code('/home/matteo/Projects/opentims')
path_dll = "/home/matteo/Projects/opentims/libtimsdata.so"

setup_bruker_so(path_dll)
D = OpenTIMS(path)

as.data.table(query(D, from=91, to=185))

as.data.table(query(D, from=91, to=185))
as.data.table(query(D, from=91, to=185, by=4, columns=c('frame', 'intensity', 'scan')))
as.data.table(rt_query(D, 10, 20, columns=c("frame")))


X = as.data.table(query(D, frames=1:10))

for(i in 1:10000){
	X = query(D, frames=i)
	plot(X$scan, X$tof, pch='.')
}


library(devtools)
document()
document()
build()
install()
load_all()

