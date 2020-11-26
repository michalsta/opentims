library(devtools)
library(opentims)
library(data.table)
library(microbenchmark)

path_dll = '/home/matteo/Projects/opentims/opentims_bruker_bridge/opentims_bruker_bridge/libtimsdata.so'
path = '/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d'

D = OpenTIMS(path)
X = as.data.table(D[1:100])


microbenchmark(X = D[1:1000])

length(D)
D@min_frame
D@max_frame
data.table(range(D, 1, 10, 3))

document()
document()
build()
install()
load_all()


