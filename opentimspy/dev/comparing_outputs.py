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

pd.set_option('display.max_columns', None)

path = pathlib.Path('/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d')
path.exists()

D = OpenTIMS(path)
E = TimsPyDF(path)
# E.frames

# are frames obtained with bruker SDK and OpenTIMS are the same?
# frame_comp = [i for i in range(E.min_frame, E.max_frame+1) if not (E[i].values == D[i]).all()]

I = Counter()
# S = Counter()
TOF = Counter()
N = D.max_frame+1
# N = 100

for i in tqdm.tqdm(range(D.min_frame, N)):
    d = D[i]
    e = E[i]
    for a, b in zip(d[:,3], e.i.values):
        I[(a,b-a)] += 1
    # for a, b in zip(d[:,1], e.scan.values): # FIXED
    #     if b != a:
    #         S[(a,b-a)] += 1
    for a, b in zip(d[:,2], e.tof.values):
        if b != a:
            TOF[(a,b-a)] += 1

def process(I):
    A, B =  list(zip(*I))
    C = list(I.values())
    X = pd.DataFrame({"A":A,"B":B,"C":C})
    X = X.sort_values(['A','B','C'])
    return X

intensities = process(I)
plt.plot(intensities.A, intensities.B)
plt.tight_layout()
plt.show()

pd.concat([intensities.groupby('B').A.min(), intensities.groupby('B').A.max()], axis=1)
ABmin = intensities.groupby('B').A.min().values
np.diff(ABmin)

tofs = process(TOF)
plt.scatter(tofs.A, tofs.B)
plt.show()

i_D = D[400][:,3]
i_E = E[400].i.values
(i_D == i_E).all()

M = 100.0/E.frames['AccumulationTime'].iloc[0]
np.all(np.round(i_D * M) == np.round(i_E * M))

1063 * M


plt.scatter(i_D, np.round(i_E * M) - np.round(i_D * M) )
plt.scatter(i_D, i_E - i_D )
plt.show()


pd.set_option('display.max_columns', None)



