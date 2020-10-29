%load_ext autoreload
%autoreload 2
import numpy as np
import pathlib
from collections import Counter
import itertools
import matplotlib.pyplot as plt
import pandas as pd

import opentims
from test import OpenTIMS

from timspy.timspy import TimsPyDF
import tqdm


path = pathlib.Path('/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d')
path.exists()

D = OpenTIMS(path)
E = TimsPyDF(path)



for i in range(D.min_frame, D.max_frame+1):
    d = D[i]
    e = E[i]
    if (d[:,1] != e.scan.values).any():
        break

list(E.iterScans(4,0,918))
E.frame_array(4,0,918)
# me good, mist wrong