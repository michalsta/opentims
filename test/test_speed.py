%load_ext autoreload
%autoreload 2
import tqdm

from opentimspy import OpenTIMS

op = OpenTIMS("rawdata/G211202_004_Slot1-1_1_3342.d")

index_based_only = ("frame","scan","tof","intensity")
for fr in tqdm.tqdm(op.frames["Id"]):
	res = op.query(fr, columns=index_based_only)


physical_only = ("retention_time","inv_ion_mobility","mz","intensity")
for fr in tqdm.tqdm(op.frames["Id"]):
	res = op.query(fr, columns=physical_only)

