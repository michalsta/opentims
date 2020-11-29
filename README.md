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
```

Do observe, that you must know which values: to put there.
If you don't, consider [TimsPy](https://github.com/MatteoLacki/timspy).


## Plans for future

We will gradually introduce cppyy to the project and fill up numpy arrays in C++.


## Law
Please read THIRD-PARTY-LICENSE-README.txt for legal aspects of using the software.

## Special thanks
We would like to thank Michael Krause, Sascha Winter, and Sven Brehmer, all from Bruker Daltonik GmbH, for their magnificent work in developing tfd-sdk.

