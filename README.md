# Quick intro
**opentims** is a module that wraps Bruker's tdf-sdk into a module, for convenience of not having to copy it in all possible projects.

# Requirements
In general, the software should work on Linux, Windows, or MacOS.
Python3.6 or higher versions are tested.

## Python bindings

## Windows installation

* Install Microsoft Visual Studio from [here](https://visualstudio.microsoft.com/visual-cpp-build-tools/).
* From terminal (assuming you have python and pip included in the system PATH) write
```{python}
pip install opentims
```

## R bindings

* Install 


For fresher versions:
```{python}
pip install git+https://github.com/michalsta/opentims
```

For development:
```{bash}
github clone https://github.com/michalsta/opentims
cd opentims
pip3 install .
```

# Usage

## Python

```{python}
import pathlib
from pprint import pprint

from opentims.opentims import OpenTIMS

path = pathlib.Path('path_to_your_data.d')
D = OpenTIMS(path) # get data handle
print(D)
print(len(D)) # The number of peaks.

try:
	import opentims_bruker_bridge
	all_columns = ('frame','scan','tof','intensity','mz','dt','rt')
except ModuleNotFoundError:
	print("Without Bruker proprietary code we cannot yet perform tof-mz and scan-dt transformations.")
	print("Download 'opentims_bruker_bridge' if you are on Linux or Windows.")
	print("Otherwise, you will be able to use only these columns:")
	all_columns = ('frame','scan','tof','intensity','rt')
	print(all_columns)

# Get a dict with data from frames 1, 5, and 67.
pprint(D.query(frames=[1,5,67], columns=all_columns))

# Get a dict with each 10th frame, starting from frame 2, finishing on frame 1000.   
pprint(D.query(frames=slice(2,1000,10), columns=all_columns))

# Get all MS1 frames 
# pprint(D.query(frames=D.ms1_frames, columns=all_columns))
# ATTENTION: that's quite a lot of data!!! You might exceed your RAM.

# If you want to extract not every possible columnt, but a subset, use the columns argument:
pprint(D.query(frames=slice(2,1000,10), columns=('tof','intensity',)))
# this will reduce your memory usage.

# Still too much memory used up? You can also iterate over frames:
it = D.query_iter(slice(10,100,10), columns=all_columns)
pprint(next(it))
pprint(next(it))

# All MS1 frames, but one at a time
iterator_over_MS1 = D.query_iter(D.ms1_frames, columns=all_columns)
pprint(next(it))
pprint(next(it))
# or in a loop, only getting intensities
for fr in D.query_iter(D.ms1_frames, columns=('intensity',)):
    print(fr['intensity'])

# Get numpy array with raw data in a given range 1:10
pprint(D[1:10])
```

## R

```{R}
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

Do observe, that you must know which values: to put there.
If you don't, consider [TimsPy](https://github.com/MatteoLacki/timspy).

# Development

## R Installation 
Download with git.
Navigate into 'opentims'.
Open terminal there and run:
```{bash}
R CMD build R
R CMD INSTALL opentims_*
```

## Plans for future

Together with Bruker we are working on openning up the tof-mz and scan-dt conversions.


## Law
Please read THIRD-PARTY-LICENSE-README.txt for legal aspects of using the software.

## Special thanks
We would like to thank Michael Krause, Sascha Winter, and Sven Brehmer, all from Bruker Daltonik GmbH, for their magnificent work in developing tfd-sdk.

