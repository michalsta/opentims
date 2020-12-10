import pathlib
from pprint import pprint

from opentimspy.opentims import OpenTIMS

path = pathlib.Path('path_to_your_data.d')
D = OpenTIMS(path) # get data handle
print(D)
# OpenTIMS(404183877 peaks)

print(len(D)) # The number of peaks.
# 404183877	

D.framesTIC() # Return combined intensity for each frame.
# array([ 95910, 579150, 906718, ..., 406317,   8093,   8629])


try:
	import opentims_bruker_bridge
	all_columns = ('frame','scan','tof','intensity','mz','inv_ion_mobility','retention_time')
except ModuleNotFoundError:
	print("Without Bruker proprietary code we cannot yet perform tof-mz and scan-dt transformations.")
	print("Download 'opentims_bruker_bridge' if you are on Linux or Windows.")
	print("Otherwise, you will be able to use only these columns:")
	all_columns = ('frame','scan','tof','intensity','retention_time')


# We consider the following columns:
print(all_columns)
# ('frame', 'scan', 'tof', 'intensity', 'mz', 'inv_ion_mobility', 'retention_time')


# Get a dict with data from frames 1, 5, and 67.
# pprint(D.query(frames=[1,5,67], columns=all_columns))
# {'frame': array([ 1,  1,  1, ..., 67, 67, 67], dtype=uint32),
#  'intensity': array([ 9,  9,  9, ..., 19, 57, 95], dtype=uint32),
#  'inv_ion_mobility': array([1.60114183, 1.6       , 1.6       , ..., 0.60077422, 0.60077422,
#        0.60077422]),
#  'mz': array([1174.65579059,  733.48094071,  916.95238879, ...,  672.00166969,
#         802.16055154, 1055.20374969]),
#  'retention_time': array([0.32649208, 0.32649208, 0.32649208, ..., 7.40565443, 7.40565443,
#        7.40565443]),
#  'scan': array([ 33,  34,  34, ..., 917, 917, 917], dtype=uint32),
#  'tof': array([312260, 220720, 261438, ..., 205954, 236501, 289480], dtype=uint32)}

# The outcome of the function is a dictionary of numpy arrays, which is the best one can have without 'Pandas' and stretching the use of numpy.
# If you like 'Pandas', consider 'TimsPy'.


# Get a dict with each 10th frame, starting from frame 2, finishing on frame 1000.   
pprint(D.query(frames=slice(2,1000,10), columns=all_columns))
# {'frame': array([  2,   2,   2, ..., 992, 992, 992], dtype=uint32),
#  'intensity': array([9, 9, 9, ..., 9, 9, 9], dtype=uint32),
#  'inv_ion_mobility': array([1.60114183, 1.60114183, 1.6       , ..., 0.60638211, 0.60301731,
#        0.60189576]),
#  'mz': array([ 302.3476711 , 1165.32728084,  391.98410024, ...,  440.96697448,
#        1158.92213271,  749.26470544]),
#  'retention_time': array([  0.43470634,   0.43470634,   0.43470634, ..., 106.71027856,
#        106.71027856, 106.71027856]),
#  'scan': array([ 33,  33,  34, ..., 912, 915, 916], dtype=uint32),
#  'tof': array([ 97298, 310524, 127985, ..., 143270, 309328, 224410], dtype=uint32)}



# Get all MS1 frames 
# pprint(D.query(frames=D.ms1_frames, columns=all_columns))
# ATTENTION: that's quite a lot of data!!! You might exceed your RAM.


# If you want to extract not every possible columnt, but a subset, use the columns argument:
pprint(D.query(frames=slice(2,1000,10), columns=('tof','intensity',)))
# {'intensity': array([9, 9, 9, ..., 9, 9, 9], dtype=uint32),
#  'tof': array([ 97298, 310524, 127985, ..., 143270, 309328, 224410], dtype=uint32)}
# 
# This will reduce your memory usage.


# Still too much memory used up? You can also iterate over frames:
it = D.query_iter(slice(10,100,10), columns=all_columns)
pprint(next(it))
# {'frame': array([10, 10, 10, ..., 10, 10, 10], dtype=uint32),
#  'intensity': array([ 9,  9,  9, ...,  9, 13, 86], dtype=uint32),
#  'inv_ion_mobility': array([1.6       , 1.5977164 , 1.5954329 , ..., 0.60526049, 0.60189576,
#        0.60189576]),
#  'mz': array([538.22572833, 148.90442262, 414.28892487, ..., 677.99334299,
#        290.222999  , 298.18539969]),
#  'retention_time': array([1.29368159, 1.29368159, 1.29368159, ..., 1.29368159, 1.29368159,
#        1.29368159]),
#  'scan': array([ 34,  36,  38, ..., 913, 916, 916], dtype=uint32),
#  'tof': array([171284,  31282, 135057, ..., 207422,  92814,  95769], dtype=uint32)}

pprint(next(it))
# {'frame': array([20, 20, 20, ..., 20, 20, 20], dtype=uint32),
#  'intensity': array([31, 10,  9, ..., 26,  9,  9], dtype=uint32),
#  'inv_ion_mobility': array([1.60114183, 1.60114183, 1.6       , ..., 0.60301731, 0.60301731,
#        0.60189576]),
#  'mz': array([1445.63777755, 1516.85130172,  536.01934412, ...,  421.57926311,
#         422.13747807,  300.13908112]),
#  'retention_time': array([2.36610302, 2.36610302, 2.36610302, ..., 2.36610302, 2.36610302,
#        2.36610302]),
#  'scan': array([ 33,  33,  34, ..., 915, 915, 916], dtype=uint32),
#  'tof': array([359979, 371758, 170678, ..., 137327, 137500,  96488], dtype=uint32)}


# All MS1 frames, but one at a time
iterator_over_MS1 = D.query_iter(D.ms1_frames, columns=all_columns)
pprint(next(it))
pprint(next(it))
# or in a loop, only getting intensities
for fr in D.query_iter(D.ms1_frames, columns=('intensity',)):
    print(fr['intensity'])
# ...
# [ 9  9  9 ... 83 72 82]
# [ 9  9  9 ... 59 86 61]
# [ 9  9 55 ...  9 32  9]
# [ 9  9  9 ... 93  9 80]
# [ 9  9 60 ...  9  9 60]
# [ 9  9  9 ... 46 10  9]
# [ 9  9  9 ... 30 61  9]
# [  9   9   9 ... 117   9  64]
# [ 20 147  69 ...  58   9   9]
# [ 9  9  9 ...  9 91  9]


# The frame lasts a convenient time unit that well suits chromatography peak elution.
# What if you were interested instead in finding out which frames eluted in a given time 
# time of the experiment?
# For this reasone, we have prepared a retention time based query:
# suppose you are interested in all frames corresponding to all that eluted between 10 and 12
# second of the experiment.
D.rt_query(10,12)
# {'frame': array([ 92,  92,  92, ..., 109, 109, 109], dtype=uint32),
#  'scan': array([ 33,  36,  41, ..., 914, 916, 917], dtype=uint32),
#  'tof': array([361758,  65738, 308330, ..., 144566, 138933, 373182], dtype=uint32),
#  'intensity': array([ 9,  9,  9, ..., 58, 91,  9], dtype=uint32),
#  'mz': array([1456.28349866,  222.28224757, 1153.59087822, ...,  445.25277042,
#          426.77550441, 1525.57652881]),
#  'inv_ion_mobility': array([1.60114183, 1.5977164 , 1.59200782, ..., 0.60413889, 0.60189576,
#         0.60077422]),
#  'retention_time': array([10.08689891, 10.08689891, 10.08689891, ..., 11.91001388,
#         11.91001388, 11.91001388])}


# Get numpy array with raw data in a given range 1:10
pprint(D[1:10])
# array([[     1,     33, 312260,      9],
#        [     1,     34, 220720,      9],
#        [     1,     34, 261438,      9],
#        ...,
#        [     9,    913, 204042,     10],
#        [     9,    914, 358144,      9],
#        [     9,    915, 354086,      9]], dtype=uint32)
