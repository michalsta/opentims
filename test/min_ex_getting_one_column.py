"""This results in a segmentation fault INSTEAD of a Python exception."""
import opentims

path = "/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d"

TDH = opentims.opentims_cpp.TimsDataHandle(path)
size = TDH.no_peaks_in_slice(100,110,1)

mz = np.empty(shape=0, dtype=np.double)    
intensity = np.empty(shape=size, dtype=np.uint32)
rt = np.empty(shape=0, dtype=np.double)    
dt = np.empty(shape=0, dtype=np.double)
scan = np.empty(shape=0, dtype=np.uint32)
tofs = np.empty(shape=0, dtype=np.uint32)
frames = np.empty(shape=0, dtype=np.uint32)

TDH.extract_frames_slice(100,110,1, frames, scan, tofs, intensity, mz, dt, rt)
