%load_ext autoreload
%autoreload 2
from pathlib import Path
from opentimspy.opentims import OpenTIMS
import numpy as np
import pandas as pd

folder_d = Path("/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d")
path = folder_d/"analysis.tdf"

D = OpenTIMS(folder_d)

D.min_frame
D.max_frame
D.min_scan
D.max_scan
D.min_intensity
D.max_intensity
D.min_retention_time
D.max_retention_time
D.min_inv_ion_mobility
D.max_inv_ion_mobility
D.min_mz
D.max_mz

pd.DataFrame({'statistic':['min','max'],
              'frame':[D.min_frame, D.max_frame],
              'scan':[D.min_scan, D.max_scan],
              'intensity':[D.min_intensity, D.max_intensity],
              'retention_time':[D.min_retention_time, D.max_retention_time],
              'inv_ion_mobility':[D.min_inv_ion_mobility, D.max_inv_ion_mobility],
              'mz':[D.min_mz,D.max_mz]}).set_index('statistic')

