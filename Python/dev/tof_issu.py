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

def iter_wrong_frames_idx():
    for i in range(D.min_frame, D.max_frame+1):
        d = D[i]
        e = E[i]
        if (d[:,2] != e.tof.values).any():
            yield i

x = list(itertools.islice(iter_wrong_frames_idx(), 100))

plt.plot(x)
plt.show()

d = D[x[0]]
e = E[x[0]].values


def iter_wrong_frames():
    for i in range(D.min_frame, D.max_frame+1):
        d = D[i]
        e = E[i]
        selection = d[:,2] != e.tof.values
        if selection.any():
            yield d[selection], e[selection] 

X = list(itertools.islice(iter_wrong_frames(), 100))
a,b = X[-3]

plt.plot(a[:,2], b.tof)
plt.show()

a[:,2] - b.tof

diffs = np.concatenate()

diffs = [list(set(b.tof-a[:,2]))[0] for a,b in X]
T2 = E.frames.T2.iloc[x].values

plt.scatter(diffs, T2)
plt.show()