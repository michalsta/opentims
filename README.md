# Quick intro
**opentims** is a module that wraps Bruker's tdf-sdk into a module, for convenience of not having to copy it in all possible projects.

# Requirements
Linux or Windows only for now.
Python3.6 or higher.


# Installation
From terminal (assuming you have python and pip included in the system PATH:

```{python}
pip3 install opentims
```

For fresher versions:
```{python}
pip3 install git+https://github.com/michalsta/opentims
```

For development:
```{bash}
github clone https://github.com/michalsta/opentims
cd opentims
pip3 install .
```

## Usage
```{python}
from opentims.opentims import TimsData

D = TimsData('path_to_your_data')

# choose some frame and scan limits
frame_no, min_scan, max_scan = 100, 0, 918

# lists of numpy arrays
D.readScans(frame_no, min_scan, max_scan)

## THIS RESULTS IN ARRAYS WITH TIME OF FLIGHTS AND INTENSITIES
## EACH ONE CORRESPONDS TO ONE SCAN, PER ONE FRAME (ONE RETENTION-TIME UNIT)
# [...
# (array([], dtype=uint32), array([], dtype=uint32)),
# (array([], dtype=uint32), array([], dtype=uint32)),
# (array([], dtype=uint32), array([], dtype=uint32)),
# (array([389679, 394578], dtype=uint32), array([9, 9], dtype=uint32)),
# (array([], dtype=uint32), array([], dtype=uint32)),
# (array([ 78036, 210934, 211498, 351984], dtype=uint32),
#  array([9, 9, 9, 9], dtype=uint32)),
#  ...]


D.frame_array(frame_no, min_scan, max_scan)
# one 4-dimensional numpy array
# columns corresponds to:
# frame_number, scan_number, mass_index (time of flight), intensity
# array([[   100,     35, 389679,      9],
#        [   100,     35, 394578,      9],
#        [   100,     37,  78036,      9],
#        ...,
#        [   100,    911, 294204,      9],
#        [   100,    913, 248788,      9],
#        [   100,    915, 100120,    120]])
```

Do observe, that you must know which values: to put there.
If you don't, consider [TimsPy](https://github.com/MatteoLacki/timspy).


## Plans for future

We will gradually introduce cppyy to the project and fill up numpy arrays in C++.


## Law
Please read THIRD-PARTY-LICENSE-README.txt for legal aspects of using the software.

## Special thanks
We would like to thank Michael Krause, Sascha Winter, and Sven Brehmer, all from Bruker Daltonik GmbH, for their magnificent work in developing tfd-sdk.

