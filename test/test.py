import opentims
import numpy as np

TDH = opentims.opentims_cpp.TimsDataHandle("/home/lackistar/data.d")



size = TDH.no_peaks_in_slice(100,101,1)
print(size)

mz = np.empty(shape=size, dtype=np.double)    
intensity = np.empty(shape=size, dtype=np.uint32)
rt = np.empty(shape=size, dtype=np.double)    
dt = np.empty(shape=size, dtype=np.double)
scan = np.empty(shape=size, dtype=np.uint32)
tofs = np.empty(shape=size, dtype=np.uint32)


def et():
    return np.empty(shape=0, dtype=np.uint32)

TDH.extract_frames_slice(100,101,1, et(), scan, tofs, intensity, mz, dt, rt)
print(scan)
