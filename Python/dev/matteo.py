%load_ext autoreload
%autoreload 2
import itertools
import numpy as np
import pathlib
import pandas as pd
pd.set_option('display.max_columns', None)
import matplotlib.pyplot as plt

import opentims
from test import OpenTIMS

import timspy
from timspy.timspy import TimsPyDF

p = pathlib.Path('/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d')
print(p.exists())

E = TimsPyDF(p)

D = OpenTIMS(p)
D.ms_types
D.retention_times
#--------------------------------------------------------------------------------
# The conversion between tof/mz

E.top2mz(1000)
E.tof2mz_model.plot()
E.tof2mz_model.res().max()


E.scan2im_model.plot()

timspy.sql.list_tables(E.conn)
res = timspy.sql.get_all_tables(E.conn)
it = iter(res)
n = next(it)
print(n)
res[n]


# there is an error in exporting this value
our_rt = np.array([D.handle.get_frame(i).time for i in range(D.min_frame, D.max_frame+1)])
E.table2df('Frames').columns

#--------------------------------------------------------------------------------
# The conversion between frame and rt
plt.hist( np.diff(E.frames.rt), bins=500 )
plt.show()


real_rt = E.frames.rt

max(real_rt - our_rt)
plt.scatter(real_rt, our_rt)
plt.show()