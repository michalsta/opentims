import pathlib
from pprint import pprint

from opentims.opentims import OpenTIMS

path = pathlib.Path('path_to_your_data.d')
# path = pathlib.Path("/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d")
D = OpenTIMS(path) # get data handle

# Get a dict with data from frames 1, 5, and 67.
pprint(D.query(frames=[1,5,67]))

# Get a dict with each 10th frame, starting from frame 2, finishing on frame 1000.   
pprint(D.query(frames=slice(2,1000,10)))

# Get all MS1 frames 
# pprint(D.query(frames=D.ms1_frames))
# ATTENTION: that's quite a lot of data!!! You might exceed your RAM.

# If you want to extract not every possible columnt, but a subset, use the columns argument:
pprint(D.query(frames=slice(2,1000,10), columns=('tof','intensity',)))
# this will reduce your memory usage.

# Still too much memory used up? You can also iterate over frames:
it = D.query_iter(slice(10,100,10))
pprint(next(it))
pprint(next(it))

# All MS1 frames, but one at a time
iterator_over_MS1 = D.query_iter(D.ms1_frames)
pprint(next(it))
pprint(next(it))
# or in a loop, only getting intensities
for fr in D.query_iter(D.ms1_frames, columns=('intensity',)):
    print(fr['intensity'])

# Get numpy array with raw data in a given range 1:10
pprint(D[1:10])


