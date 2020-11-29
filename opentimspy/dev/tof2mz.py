%load_ext autoreload
%autoreload 2
import numpy as np
import pathlib
from collections import Counter
import matplotlib.pyplot as plt
import pandas as pd
pd.set_option('max_columns', None)

from test_dir.test import OpenTIMS
from timspy.timspy import TimsPyDF
from rmodel.polyfit import polyfit

path = pathlib.Path('~/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d').expanduser().resolve()
path.exists()

A = OpenTIMS(path)
B = TimsPyDF(path)

mz_min=0
mz_max=3000
mzIdx_min, mzIdx_max = B.mzToIndex(1, [mz_min, mz_max]).astype(int)
max_frame=B.max_frame
T1 = B.frames.T1.values
T2 = B.frames.T2.values

plt.plot((T1-T1.mean())/T1.std())
plt.plot((T2-T2.mean())/T2.std())
plt.show()

mzIdx_step = (mzIdx_max - mzIdx_min) // 1000
tof = np.arange(mzIdx_min, mzIdx_max, mzIdx_step)

M = np.empty(shape=(len(mzIdx), max_frame))
for j in range(max_frame):
    M[:,j] = B.indexToMz(j+1, mzIdx)

standardize = lambda x: (x-x.mean())/x.std()
norm2_0 = lambda x: (x-x[0])/x.std()

N100 = M[:,100]
mzIdx
plt.plot()
plt.show()

plt.plot(M[1000,:]/M[1000,:].sum())
plt.plot(T1/T1.sum())
plt.show()

M[1000,:].mean()
M[1,:].mean()

M[1000,:].std()
M[1,:].std()

plt.plot(mzIdx, standardize(M.mean(1)))
plt.plot(mzIdx, standardize(M.std(1)))
plt.show()




plt.plot(mzIdx, M.mean(1) - M.mean())
plt.plot(mzIdx, M.std(1) - M.std(1).mean())
plt.show()




plt.plot(standardize(M[1000,:]))
plt.plot(standardize(T1))
plt.show()

plt.plot(norm2_0(M[100,:]))
plt.scatter(np.arange(len(norm2_0(T1))), norm2_0(T1))
plt.show()

plt.plot((M[100,:] - M[100,:].mean())/M[100,:].std())
plt.plot(standardize(T1))
plt.show()



for i in range(1, len(M), 10):
    mz = M[i,:10]/M[i,0]
    plt.scatter(mz/max(mz), T1[:10]/max(T1))
plt.show()





for i in range(1,11553,100):
    plt.plot(mzIdx, M[:,i] - M[:,1])
plt.show()

np.polyfit()

coefs = np.array([np.polyfit(mzIdx,
                             B.indexToMz(i+1, mzIdx), deg=2)
                  for i in range(1,11553,1)])
tof2mz = polyfit(mzIdx, mz, deg=2)
tof2mz.plot()

plt.plot(B.frames.T1/max(B.frames.T1))
plt.plot(coefs[:,0]/max(coefs[:,0]))
plt.plot(coefs[:,1]/max(coefs[:,1])) 
plt.plot(coefs[:,2]/max(coefs[:,2]));plt.show()

plt.hist(coefs[:,2], bins=100);plt.show()

plt.scatter(coefs[:,0], coefs[:,2]);plt.show()

plt.scatter(coefs[:,0], coefs[:,1]);plt.show()

mz = B.indexToMz(1, mzIdx)
