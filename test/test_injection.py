import numpy as np
import opentimspy
import pathlib

small5p = pathlib.Path("/home/matteo/Projects/MIDIA/rawdata/small5P.d")

raw_data = opentimspy.OpenTIMS(small5p)
scans = np.arange(1, 100).astype(np.uint32)
inv_ion_mobilities = raw_data.handle.scan_to_inv_mobility(1, scans)
scans_back = raw_data.handle.inv_mobility_to_scan(1, inv_ion_mobilities)
scans - scans_back
