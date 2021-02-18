%load_ext autoreload
%autoreload 2
from pathlib import Path
from opentimspy.opentims import OpenTIMS

from timspy.df import TimsPyDF
# import numpy as np
import pandas as pd

folder_d = Path("/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d")

D = OpenTIMS(folder_d)
D.tables_names()
E = TimsPyDF(folder_d)
E.tables_names()
X = E.min_max_measurements()
X.loc['min']
X['frame']['min']

E.summary()
E.plot_peak_counts()
E.query([10,20], ['mz','inv_ion_mobility'])

E.table2df('Frames')
E.table2dict('Frames')

E.intensity_per_frame(recalibrated=False)
E.plot_TIC()

I, mz_bin_borders, inv_ion_mobility_bin_borders = E.intensity_given_mz_inv_ion_mobility(verbose=True)
E.plot_intensity_given_mz_inv_ion_mobility(I, mz_bin_borders, inv_ion_mobility_bin_borders)
