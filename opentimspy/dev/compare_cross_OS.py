import numpy as np
import pathlib
import itertools
from collections import Counter

from opentimspy.opentims import OpenTIMS

path = pathlib.Path("/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d")
path = pathlib.Path("C:/projects/opentims/data.d")
path = pathlib.Path("/Users/matteo/Projects/bruker/BrukerMIDIA/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d")

D = OpenTIMS(path)
integer_based_hash = D.get_hash()
# linux_hash = b'\xad\xb3j\x88E\xd5c\xb5B\x93CZ\x0bJn\t\xfa\x90\x8dy\xe0?\x82\xb7\xbeQ\xfceH\xd9X\xd9\xf1&\x8d\x84-\\\x9f\x9a0\xcc6\xee\xe3M\xe5\xdb\xb5\x03A\xba\xb7\xea\xa2n>\x112y\x11\xb8[\x87'
# win_hash = b'\xad\xb3j\x88E\xd5c\xb5B\x93CZ\x0bJn\t\xfa\x90\x8dy\xe0?\x82\xb7\xbeQ\xfceH\xd9X\xd9\xf1&\x8d\x84-\\\x9f\x9a0\xcc6\xee\xe3M\xe5\xdb\xb5\x03A\xba\xb7\xea\xa2n>\x112y\x11\xb8[\x87'
# mac_hash = b'\xad\xb3j\x88E\xd5c\xb5B\x93CZ\x0bJn\t\xfa\x90\x8dy\xe0?\x82\xb7\xbeQ\xfceH\xd9X\xd9\xf1&\x8d\x84-\\\x9f\x9a0\xcc6\xee\xe3M\xe5\xdb\xb5\x03A\xba\xb7\xea\xa2n>\x112y\x11\xb8[\x87'
# linux_hash == win_hash # True!!!
# linux_hash == mac_hash # True!!!

data_set_hash_all_cols = D.get_hash(columns=('frame','scan','tof','intensity','mz','dt','rt'))
# linux_hash_all_cols = b'\xd7\xb4\xcc\x10|\x06\x84y\xcdW\x83x\x98\xafS\xe9B?i\x17r\xea\xe9\x961\x9e\x94A$1\xc6z\x97]\xec\x1d\xb7D\x9b\xba\xe5\xe0\xcdr\x98\xbe\x85#(\t\xd6\x90&.o\xa9\xd5\x98$\xec\xaa\xa8\xad\x9d'
# win_hash_all_cols = differnet ! :D

# def hashes_dataset(D, **kwds):
#     return [hash_frame(X, **kwds) 
#             for X in D.query_iter(frames=slice(D.min_frame,
#                                                D.max_frame+1))]

# def diff_hashes(H0, H1):
#     assert H0.shape == H1.shape, "The elements dimensions differ."
#     for i, (r0, r1) in enumerate(zip(H0, H1)):
#         for j, (a0, a1) in enumerate(zip(r0, r1)):
#             if a0 != a1:
#                 yield i,j

