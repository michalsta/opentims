%load_ext autoreload
%autoreload 2
import numpy as np
import pathlib
from collections import Counter
import matplotlib.pyplot as plt
import pandas as pd
pd.set_option('max_columns', None)
import plotnine as p

from timspy.timspy import TimsPyDF
from rmodel.polyfit import polyfit
from test_dir.test import OpenTIMS

path = pathlib.Path('~/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d').expanduser().resolve()
path.exists()


standardize = lambda x: (x-x.mean()) / x.std()

class SimpleEstimator(object):
    def __init__(self, 
                 path,
                 tof_min=0,
                 tof_max=None,
                 tof_grid_points=1000,
                 mz_max_if_no_tof_max=3000):
        assert path.exists(), 'No such path.'
        self.D = TimsPyDF(path)
        if tof_max is None:
            tof_max = self.D.mzToIndex(1, [mz_max_if_no_tof_max]).astype(int)
        self.tof = np.unique(np.linspace(tof_min, tof_max, tof_grid_points, dtype=int))
        T1 = self.D.frames.T1.values
        T2 = self.D.frames.T2.values
        self.t1 = standardize(T1)
        self.t2 = standardize(T2)
        SE.A, SE.B, SE.C = 0,0,0

    def estimate(self, deg=1):
        self.params = np.array([np.polyfit(np.sqrt(self.D.indexToMz(f+1, self.tof)), self.tof, deg=deg)
                                for f in range(self.D.max_frame)])
        self.A = self.params[:,1].mean()
        self.C, self.B = np.polyfit(self.t1,
                                    self.params[:,0], deg=1)
        # self.C = params[:,0].std()
        # self.B = params[:,0].mean()

    def res(self, A=None,B=None,C=None):
        A = self.A if A is None else A
        B = self.B if B is None else B
        C = self.C if C is None else C

        for f in range(1, self.D.max_frame):
            mz = self.D.indexToMz(f, self.tof)
            mz_est = ((self.tof - A)/(B + C*self.t1[f-1]))**2
            yield pd.DataFrame({'mz':mz, 'mz_est':mz_est, 'f':f})

    def plot_res(self, show=True):
        X = pd.concat(self.res())
        plt.hist(X.mz - X.mz_est, bins=1000)
        if show:
            plt.show()




SE = SimpleEstimator(path)
# SE.D.indexToMz(10, np.arange(1,10000))
SE.estimate(deg=3)
MzCal = SE.D.table2df('MzCalibration')

plt.plot(standardize(SE.params[:,0]))
plt.plot(standardize(-SE.params[:,1]))
plt.plot(standardize(SE.params[:,2]))
plt.plot(standardize(-SE.params[:,3]))
plt.plot(standardize(-SE.t1))
plt.plot(standardize(-SE.t2))
plt.show()
SE.params
SE.tof

# (-124068.17347764065, 12730.865914763803, -7.486911887053376e-07)
X = pd.DataFrame(SE.params, columns=[f"a{i}" for i in range(3,-1,-1)])
X['t1'] = SE.t1
X['t2'] = SE.t2
X['T1'] = SE.D.frames.T1
X['frame'] = range(11553)
Y = pd.melt(X, id_vars='frame', var_name='col', value_name='val')
(p.ggplot(Y) + p.geom_line(p.aes(x='frame', y='val'))+ p.facet_grid('col~.', scales='free') )



import sqlite3
from timspy.sql import table2df

conn = sqlite3.connect(str(path/'analysis.tdf'))
table2df(conn,'MzCalibration')
sql = ''' UPDATE MzCalibration
          SET ModelType = ?
          WHERE Id = ?;'''
conn.execute(sql, (1,1))
table2df(conn,'MzCalibration')
