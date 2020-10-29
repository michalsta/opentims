%load_ext autoreload
%autoreload 2
import numpy as np
import pathlib
from collections import Counter
import itertools
from collections import Counter
import hashlib

import opentims
from test_dir.test import OpenTIMS
from test_dir.hashing import hash_frame, hash_dataset

path = pathlib.Path('~/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d').expanduser().resolve()

path = pathlib.Path('~/Projects/bruker/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d').expanduser().resolve()
path.exists()

D = OpenTIMS(path)
X = D[2]
h = hash_dataset(D)
b'NfP\xbch\xeb\xc4\x9eG\xcf\xb2i\x83!\xfdX4\xc6\xd8\xe0?\\\xb5B\x90\x0e\x99[\xb5\xfa\x87\xfe<&\xe3F\x0b\x8e\xd8\x9a\xab\xad\xda\xf3\xb2\x83\xd9\x14\x91\xcf\x989\xe6\x12\xe0\xc3.\x0bf\xd6\xa0"\x96\xc0'

with open('../hash', 'r') as f:
    h_linux = f.read()

h_linux == str(h)


# H = [get_hashes(X) for X in D]
# H = np.array(H)

np.save('hashes.npy', H)
H0 = np.load('hashes.npy')
H1 = H0.copy()
H1[100,2] = 'a'
H0[100,2]

def diff_hashes(H0, H1):
    assert H0.shape == H1.shape, "The elements dimensions differ."
    for i, (r0, r1) in enumerate(zip(H0, H1)):
        for j, (a0, a1) in enumerate(zip(r0, r1)):
            if a0 != a1:
                yield i,j

list(diff_hashes(H0, H1))

# main data on Windows
path = pathlib.Path('C:/Projects/bruker_data/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d')
D = OpenTIMS(path)
h = hash_dataset(D)

# another data-set
path = pathlib.Path('C:/Projects/bruker_data/F2020-09-14_18-43-30_M200914_01_69.d')
D = OpenTIMS(path)
h = hash_dataset(D)
# windows:
# b'\x8b\x93\xb9\x8b\xa0\xca\x0e\xbf1\xf6\x0cd\x9e\x8b1&\xbeZG\x94\xa9\xce(\xb9Gy\xa5YP\xc4\xd6G\x96\xee\xa2\x0f\x1b\x00\x99\xa0FE\x00\xb2q\xd9$\xf2\xcf\xad\x89\xec\xe2\ri\x9by\x9fG\t\xa1yW\x87'


