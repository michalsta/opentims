%load_ext autoreload
%autoreload 2
import opentims
import numpy as np
import pandas as pd
pd.set_option('display.max_columns', None)
pd.set_option('display.max_rows', 7)
import matplotlib.pyplot as plt

# from api_opentims import OpenTIMS
from opentims.opentims import OpenTIMS

D = OpenTIMS("/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d")

it = D.rt_query_iter(10,20)
next(it)

try:
	raise RuntimeError('test')
except RuntimeError as e:
	print(e)
	print(e.args)
	if e.args[0] == 'test':
		print('dupa')
	else:
		print('kupa')

D.frame2rt(np.arange(1, 10000, dtype=np.uint32))
D.MS1_frames

X = D.query(D.MS1_frames)
X['frame'].shape


D.query([10,49])


# too inefficient!
plt.hexbin(X['mz'], X['dt'])
plt.show()

Z = pd.DataFrame(D.query([10,40]))

plt.scatter(Z.mz, Z.dt, s=1)
plt.show()
plt.scatter(Z.tof, Z.mz)
plt.show()
plt.scatter(Z.scan, Z.dt)
plt.show()
D.frame_arrays(20)

list(D.query_iter([100,200]))


D[10:20:2]
X = D[[10,40]]
np.all(Z[['frame', 'scan', 'tof', 'intensity']].values == X)



pd.DataFrame(D.query([10,40]))
D.query([10,40])

pd.DataFrame(D.query([10,40], columns=('frame','intensity','tof','scan')))
pd.DataFrame(D.query([10,40], columns=('intensity',)))

pd.DataFrame(D.rt_query(10,30))
pd.DataFrame(D.rt_query(10,20))

D._get_empty_arrays(10, ('frame','intensity','tof','scan'))

W = pd.DataFrame(D.query(slice(10,101)))
X[10:101]
D[10]
D[40]
D[[10,40]]

np.searchsorted(D.rts, [10, 40])
D.rts[91]
D.rts[90]

D.frame2rt([10,20,40])
D.frame2rt([1])
D.frame2rt([1,10])


D._get_dict(10, ("rt","mz","frame","dupa"))

D.frame_array(100)
D[100:110].shape



D[10]

a, b = (10,'mz rt td')


size = TDH.no_peaks_in_slice(100,101,1)
print(size)


cols = {c:np.empty(shape=size, dtype=d) for c,d in c2d.items()}

np.searchsorted([0,1,2,3], (.5, 2.6))
np.searchsorted([0,1,2,3], [.5, 3, 3.5])
np.searchsorted([0,1,2,3], [-1, .5, 3, 3.5])
np.searchsorted([0,1,2,3], [3], side='left')-1
np.searchsorted([0,1,2,3], [1], side='left')

