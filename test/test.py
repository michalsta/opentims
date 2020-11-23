import opentims
import numpy as np

# path = "/home/lackistar/data.d"
path = "/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d"

TDH = opentims.opentims_cpp.TimsDataHandle(path)

size = TDH.no_peaks_in_slice(100,110,1)
print(size)

mz = np.empty(shape=size, dtype=np.double)    
intensity = np.empty(shape=size, dtype=np.uint32)
rt = np.empty(shape=size, dtype=np.double)    
dt = np.empty(shape=size, dtype=np.double)
scan = np.empty(shape=size, dtype=np.uint32)
tofs = np.empty(shape=size, dtype=np.uint32)
frames = np.empty(shape=size, dtype=np.uint32)

TDH.extract_frames_slice(100,110,1, frames, scan, tofs, intensity, mz, dt, rt)



def et():
    return np.empty(shape=0, dtype=np.uint32)

# TDH.extract_frames_slice(100,101,1, et(), scan, tofs, intensity, mz, dt, rt)
# print(scan)
# print(frames)

# print(scan)
print(frames)


X = np.empty(shape=(size,4), order='F', dtype=np.uint32)
TDH.extract_frames_slice(100,11,1, X[:,0], X[:,1], X[:,2], X[:,3], mz, dt, rt)
print(X)


import opentims
import numpy as np

# path = "/home/lackistar/data.d"
path = "/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d"

TDH = opentims.opentims_cpp.TimsDataHandle(path)


to_extract = np.array([1,10], dtype=np.uint32)
size = TDH.no_peaks_in_frames(to_extract)


mz = np.empty(shape=size, dtype=np.double)    
intensity = np.empty(shape=size, dtype=np.uint32)
rt = np.empty(shape=size, dtype=np.double)    
dt = np.empty(shape=size, dtype=np.double)
scan = np.empty(shape=size, dtype=np.uint32)
tofs = np.empty(shape=size, dtype=np.uint32)
frames = np.empty(shape=size, dtype=np.uint32)

TDH.extract_frames(to_extract, frames, scan, tofs, intensity, mz, dt, rt)

from collections import Counter

Counter(frames)

