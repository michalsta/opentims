# Quick intro
**opentims** is a module that wraps Bruker's tdf-sdk into a module, for convenience of not having to copy it in all possible projects.

# Requirements
In general, the software should work on Linux, Windows, or MacOS.
Python3.6 or higher versions are tested.

On Windows, install Microsoft Visual Studio from [here](https://visualstudio.microsoft.com/visual-cpp-build-tools/) to make use of C++ or Python code.
On Linux, have clang++ or g++ installed (clang is better).
On macOS, [install x-tools command line tools](https://www.godo.dev/tutorials/xcode-command-line-tools-installation-faq/).

## Python
 
From terminal (assuming you have python and pip included in the system PATH) write
```bash
pip install opentims
```
For a direct installation from github:
```bash
pip install git+https://github.com/michalsta/opentims
```

**OnWindows**: we have noticed issues with the numpy==1.19.4 due to changes in Intel's fmod function, unrelated to our work. 
If you keep on experiencing these issues, install numpy==1.19.3.
```bash
pip uninstall numpy
pip install numpy==1.19.3
```

## R

From R terminal (opened either in powershell or in RStudio and similar):
```bash
install.packages('opentimsr')
```
If that does not work, first clone the repository and then install manually with:
```bash
git clone https://github.com/michalsta/opentims
R CMD build opentimsr
R CDM INSTALL opentims_*.tar.gz
```
On windows, replace `R` with `R.exe`.
You can download git [from here](https://git-scm.com/book/en/v2/Getting-Started-Installing-Git).

# Usage

## Python

All the functions are documented with doc-strings.
The resulting automatic API documentation is available [here](https://michalsta.github.io/opentims/).

```python
import pathlib
from pprint import pprint

from opentimspy.opentims import OpenTIMS

path = pathlib.Path('path_to_your_data.d')
D = OpenTIMS(path) # get data handle
print(D)
# OpenTIMS(404183877 peaks)

print(len(D)) # The number of peaks.
# 404183877


try:
	import opentims_bruker_bridge
	all_columns = ('frame','scan','tof','intensity','mz','dt','rt')
except ModuleNotFoundError:
	print("Without Bruker proprietary code we cannot yet perform tof-mz and scan-dt transformations.")
	print("Download 'opentims_bruker_bridge' if you are on Linux or Windows.")
	print("Otherwise, you will be able to use only these columns:")
	all_columns = ('frame','scan','tof','intensity','rt')


# We consider the following columns:
print(all_columns)
# ('frame', 'scan', 'tof', 'intensity', 'mz', 'dt', 'rt')


# Get a dict with data from frames 1, 5, and 67.
pprint(D.query(frames=[1,5,67], columns=all_columns))
# {'dt': array([1.60114183, 1.6       , 1.6       , ..., 0.60077422, 0.60077422,
#        0.60077422]),
#  'frame': array([ 1,  1,  1, ..., 67, 67, 67], dtype=uint32),
#  'intensity': array([ 9,  9,  9, ..., 19, 57, 95], dtype=uint32),
#  'mz': array([1174.65579059,  733.48094071,  916.95238879, ...,  672.00166969,
#         802.16055154, 1055.20374969]),
#  'rt': array([0.32649208, 0.32649208, 0.32649208, ..., 7.40565443, 7.40565443,
#        7.40565443]),
#  'scan': array([ 33,  34,  34, ..., 917, 917, 917], dtype=uint32),
#  'tof': array([312260, 220720, 261438, ..., 205954, 236501, 289480], dtype=uint32)}
# 
# The outcome of the function is a dictionary of numpy arrays, which is the best one can have without 'Pandas' and stretching the use of numpy.
# If you like 'Pandas', consider 'TimsPy'.


# Get a dict with each 10th frame, starting from frame 2, finishing on frame 1000.   
pprint(D.query(frames=slice(2,1000,10), columns=all_columns))
# {'dt': array([1.60114183, 1.60114183, 1.6       , ..., 0.60638211, 0.60301731,
#        0.60189576]),
#  'frame': array([  2,   2,   2, ..., 992, 992, 992], dtype=uint32),
#  'intensity': array([9, 9, 9, ..., 9, 9, 9], dtype=uint32),
#  'mz': array([ 302.3476711 , 1165.32728084,  391.98410024, ...,  440.96697448,
#        1158.92213271,  749.26470544]),
#  'rt': array([  0.43470634,   0.43470634,   0.43470634, ..., 106.71027856,
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
# {'dt': array([1.6       , 1.5977164 , 1.5954329 , ..., 0.60526049, 0.60189576,
#        0.60189576]),
#  'frame': array([10, 10, 10, ..., 10, 10, 10], dtype=uint32),
#  'intensity': array([ 9,  9,  9, ...,  9, 13, 86], dtype=uint32),
#  'mz': array([538.22572833, 148.90442262, 414.28892487, ..., 677.99334299,
#        290.222999  , 298.18539969]),
#  'rt': array([1.29368159, 1.29368159, 1.29368159, ..., 1.29368159, 1.29368159,
#        1.29368159]),
#  'scan': array([ 34,  36,  38, ..., 913, 916, 916], dtype=uint32),
#  'tof': array([171284,  31282, 135057, ..., 207422,  92814,  95769], dtype=uint32)}
pprint(next(it))
# {'dt': array([1.60114183, 1.60114183, 1.6       , ..., 0.60301731, 0.60301731,
#        0.60189576]),
#  'frame': array([20, 20, 20, ..., 20, 20, 20], dtype=uint32),
#  'intensity': array([31, 10,  9, ..., 26,  9,  9], dtype=uint32),
#  'mz': array([1445.63777755, 1516.85130172,  536.01934412, ...,  421.57926311,
#         422.13747807,  300.13908112]),
#  'rt': array([2.36610302, 2.36610302, 2.36610302, ..., 2.36610302, 2.36610302,
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


# Get numpy array with raw data in a given range 1:10
pprint(D[1:10])
# array([[     1,     33, 312260,      9],
#        [     1,     34, 220720,      9],
#        [     1,     34, 261438,      9],
#        ...,
#        [     9,    913, 204042,     10],
#        [     9,    914, 358144,      9],
#        [     9,    915, 354086,      9]], dtype=uint32)
```

## R

```R
library(opentims)

# path = pathlib.Path('path_to_your_data.d')
path = "/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d"

# Do you want to have access only to 'frame', 'scan', 'time of flight', and 'intensity'?
accept_Bruker_EULA_and_on_Windows_or_Linux = TRUE

if(accept_Bruker_EULA_and_on_Windows_or_Linux){
    folder_to_stode_priopriatary_code = "/home/matteo"
    path_to_bruker_dll = download_bruker_proprietary_code(folder_to_stode_priopriatary_code)
    setup_bruker_so(path_to_bruker_dll)
    all_columns = c('frame','scan','tof','intensity','mz','dt','rt')
} else {
	all_columns = c('frame','scan','tof','intensity','rt')
}

D = OpenTIMS(path) # get data handle

print(D) 
print(length(D)) # The number of peaks.


pprint = function(x,...){ print(head(x,...)); print(tail(x,...)) }

# Get a data,frame with data from frames 1, 5, and 67.
pprint(query(D, frames=c(1,5,67), columns=all_columns))

# Get a dict with each 10th frame, starting from frame 2, finishing on frame 1000.   
pprint(query(D, frames=seq(2,1000,10), columns=all_columns))

# Get all MS1 frames 
# print(query(D, frames=MS1(D)))
# ATTENTION: that's quite a lot of data!!! And R will first make a stupid copy, because it's bad. You might exceed your RAM.

# Getting subset of columns: simply specify 'columns':
pprint(query(D, frames=c(1,5,67), columns=c('scan','intensity')))
# this is also the only way to get data without accepting Bruker terms of service and on MacOS (for time being).

# R has no proper in-built iterators :(

# All MS1 frames, but one at a time:
for(fr in MS1(D)){
    print(query(D, fr, columns=all_columns))
}


# Syntactic sugar: only the real bruker data can also be extracted this way:
pprint(head(D[100])) 
X = D[10:200]
pprint(X) 
```

# More options?

Consider [TimsPy](https://github.com/MatteoLacki/timspy) and [TimsR](https://github.com/MatteoLacki/timsr) for more user-friendly options.

# Development
We will be happy to accept any contributions.

## Plans for future
Together with Bruker we are working on openning up the tof-mz and scan-dt conversions.


## Licence

OpenTIMS is released under the terms of GNU GPL v3 licence, as
published by the Free Software Foundation. Full text below.
If you require other licensing terms please contact the authors.

OpenTIMS contains built-in versions of the following software:
- sqlite3, public domain
- ZSTD, BSD licence
- mio, MIT licence

See the respective files for details.
Consider [TimsPy](https://github.com/MatteoLacki/opentims_bruker_bridge) for Bruker proprietary time of flight to mass to charge ratio and scan to drift time transformations, which are shipped under separate license. 

## Special thanks
We would like to thank Michael Krause, Sascha Winter, and Sven Brehmer, all from Bruker Daltonik GmbH, for their magnificent work in developing tfd-sdk.

