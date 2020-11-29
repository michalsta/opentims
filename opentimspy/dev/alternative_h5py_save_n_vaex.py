import vaex as vx
import h5py
import numpy as np

f = h5py.File('test_h5.hdf5','w')
x = np.arange(100)
y = np.arange(100,0,-1)
f.create_dataset('x', data=x)
f.create_dataset('y', data=y)
f.close()
vx.open('test_h5.hdf5')
!h5ls test_h5.hdf5 
!rm test_h5.hdf5

f = h5py.File('test_h5_2.hdf5','w')
D = np.array([x, y, x, y]).T
f.create_dataset('D', data=D)
f.close()
vx.open('test_h5_2.hdf5')
!h5ls test_h5_2.hdf5

# SO: vaex reads only nicely parsed data.
# Each data-frame column is mapped to a separate dataset (1D).