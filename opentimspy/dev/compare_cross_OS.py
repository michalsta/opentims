import numpy as np
import pathlib
import itertools
from collections import Counter
import hashlib

import opentimspy
from opentimspy.opentims import OpenTIMS

path = pathlib.Path("/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d")

def hash_frame(X, columns=('frame','scan','tof','intensity')):
    hashes = []
    for c in columns:
        h = hashlib.blake2b()
        h.update(X[c])
        hashes.append(h.digest())
    return hashes

def hashes_dataset(D):
    return [hash_frame(X) 
            for X in D.query_iter(frames=slice(D.min_frame,
                                               D.max_frame+1))]

def hash_dataset(D, columns=('frame','scan','tof','intensity')):
    h = hashlib.blake2b()
    for X in D.query_iter(frames=slice(D.min_frame,
                                       D.max_frame+1),
                          columns=columns):
        for c in columns:
            h = hashlib.blake2b()
            h.update(X[c])
    return h.digest()

D = OpenTIMS(path)
data_set_hash = hash_dataset(D)
# b'-3?\x8e\x12&\xcbYd\xb0\xd6\x82MV)\xe7\xfc\x07Z\xa3\xf8\xe8\x0e\x9d\xda;\x13=\tt\x0fY\xea\x92e\x9e\x14\x90\xfb\x17{\x1e\xb3J\xd8\xf2&\x05I\x8c\xfbM: \x13\xbddJ\xf7\x9b\xc4x\\4'

data_set_hash_all_cols = hash_dataset(D,('frame','scan','tof','intensity','mz','dt','rt'))
# b'\xf2\x04\x07K\xdd\x97"c>\x94xQ\xdb\xb4\x9b\xb9\xd1\xaf/\x98x\x7fc\x07\x7f\x94\xcb\xdf;\xba\xac\xa5\xbdv\xbf\xf9\xc1\xaf\xe4\xde\xcc\xd4NiW\x8b\x8fl\x1b\x81\xa3\xa49v\x90\xac\x81{\xc8\xab\xd4\x90S\n'


def diff_hashes(H0, H1):
    assert H0.shape == H1.shape, "The elements dimensions differ."
    for i, (r0, r1) in enumerate(zip(H0, H1)):
        for j, (a0, a1) in enumerate(zip(r0, r1)):
            if a0 != a1:
                yield i,j

