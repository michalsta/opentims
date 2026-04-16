# OpenTIMS

`OpenTIMS` is a C++ library for accessing timsTOF Pro data format (TDF).
It replaces (to a large extent) Bruker's SDK for purposes of data access and provides convenient layer for integration into higher level computer languages.
It comes with bindings to `Python` (through `opentimspy`) and `R` languages (through `opentimsr`).
In `Python`, we extract data into `NumPy` arrays that are optimized for speed and come with a universe of useful methods for their quick manipulation.
In `R`, we extract data into the native `data.frame` object.

With `OpenTIMS` you can access data contained in the `analysis.tdf_bin` file produced by your mass spectrometer of choice (as long as it is a timsTOF instrument).
It also parses some of the information out of the `SQLite` data base contained in the `analysis.tdf` file.
You should have both of these files in one folder to start using our software.

We can also get your data faster in `C++` (and so to `Python` and `R`):
![](https://github.com/michalsta/opentims/blob/master/speed.png "TIC per frame")

Prefer userfriendliness over raw power?
We have you covered! Check out the children projects [`TimsR`](https://github.com/MatteoLacki/timsr) and [`TimsPy`](https://github.com/MatteoLacki/timspy).

# Requirements

The software was tested on Linux, Windows, and MacOS.
On Windows, install Microsoft Visual Studio from [here](https://visualstudio.microsoft.com/visual-cpp-build-tools/) to make use of C++ or Python code.
On Linux, have `clang++` or `g++` installed (`clang` produces slightly faster code).
Also, make sure a developer version of Python is installed.
For instance, on Ubuntu:
```
sudo apt install python3-dev
```
The `-dev` package contains headers needed for pybind11 to work properly.
On macOS, [install Xcode Command Line Tools](https://www.junian.net/dev/xcode-command-line-tools-installation-faq/).

## Python
 
From terminal (assuming you have python and pip included in the system PATH) write
```bash
pip install opentimspy
```
This gives you full access to all columns including `mz` and `inv_ion_mobility` via built-in open-source converters.
If you prefer Bruker's proprietary conversion functions (Linux and Windows only), also install:
```bash
pip install opentims_bruker_bridge
```
When `opentims_bruker_bridge` is installed it is used automatically; otherwise the built-in open-source converters are used after calling `opentimspy.setup_opensource()`.

## R

From R terminal (opened either in powershell or in RStudio and similar):
```bash
install.packages('opentimsr')
```
or using devtools
```bash
install.packages('devtools')
library(devtools)

install_github("michalsta/opentims", subdir="src/opentimsr")
```
On Windows the last command might give you a warning about tar stopping with non-zero exit code. It's safe to ignore.


If that does not work, first clone the repository and then install manually with:
```bash
git clone https://github.com/michalsta/opentims
cd opentims
R CMD build src/opentimsr
R CMD INSTALL opentimsr_*.tar.gz
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

import opentimspy
from opentimspy import OpenTIMS

# If opentims_bruker_bridge is installed, Bruker's conversion functions are used
# automatically. Otherwise, activate the built-in open-source converters:
if not opentimspy.bruker_bridge_present:
    opentimspy.setup_opensource()

all_columns = ('frame', 'scan', 'tof', 'intensity', 'mz', 'inv_ion_mobility', 'retention_time')

path = pathlib.Path('path_to_your_data.d')
with OpenTIMS(path) as D:  # use as a context manager to ensure the handle is closed
    pass

D = OpenTIMS(path) # or open without context manager
print(D)
# OpenTIMS(404_183_877 peaks)

print(len(D)) # The number of peaks.
# 404183877

D.framesTIC() # Return combined intensity for each frame.
# array([ 95910, 579150, 906718, ..., 406317,   8093,   8629])


# We consider the following columns:
print(all_columns)
# ('frame', 'scan', 'tof', 'intensity', 'mz', 'inv_ion_mobility', 'retention_time')


# Get a dict with data from frames 1, 5, and 67.
pprint(D.query(frames=[1,5,67], columns=all_columns))
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
```

## R

For a detailed documentation of the `R` package, consult the [CRAN webpage of the project](https://cran.r-project.org/web/packages/opentimsr/index.html) (especially the reference manual linked there).

```R
library(opentimsr)

path = "/path/to/your/data.d"

# Activate the built-in open-source converters to get mz and inv_ion_mobility.
# Alternatively, call setup_bruker_so() with a path to Bruker's timsdata library
# (Linux/Windows only) if you have it and accept Bruker's license terms.
setup_opensource()
all_columns = c('frame','scan','tof','intensity','mz','inv_ion_mobility','retention_time')

D = OpenTIMS(path) # get data handle
D@all_columns

print(D) 
print(length(D)) # The number of peaks.
# 404183877


pprint = function(x,...){ print(head(x,...)); print(tail(x,...)) }

# Get a data,frame with data from frames 1, 5, and 67.
pprint(query(D, frames=c(1,5,67), columns=all_columns))
#   frame scan    tof intensity        mz inv_ion_mobility retention_time
# 1     1   33 312260         9 1174.6558         1.601142      0.3264921
# 2     1   34 220720         9  733.4809         1.600000      0.3264921
# 3     1   34 261438         9  916.9524         1.600000      0.3264921
# 4     1   36  33072         9  152.3557         1.597716      0.3264921
# 5     1   36 242110         9  827.3114         1.597716      0.3264921
# 6     1   38 204868        62  667.5863         1.595433      0.3264921
# 
#        frame scan    tof intensity        mz inv_ion_mobility retention_time
# 224732    67  917 135191       189  414.7175        0.6007742       7.405654
# 224733    67  917 192745        51  619.2850        0.6007742       7.405654
# 224734    67  917 201838        54  655.3439        0.6007742       7.405654
# 224735    67  917 205954        19  672.0017        0.6007742       7.405654
# 224736    67  917 236501        57  802.1606        0.6007742       7.405654
# 224737    67  917 289480        95 1055.2037        0.6007742       7.405654



# Get a data.frame with each 10th frame, starting from frame 2, finishing on frame 1000.
pprint(query(D, frames=seq(2,1000,10), columns=all_columns))
#   frame scan    tof intensity        mz inv_ion_mobility retention_time
# 1     2   33  97298         9  302.3477         1.601142      0.4347063
# 2     2   33 310524         9 1165.3273         1.601142      0.4347063
# 3     2   34 127985         9  391.9841         1.600000      0.4347063
# 4     2   35 280460         9 1009.6751         1.598858      0.4347063
# 5     2   37 329377        72 1268.6262         1.596575      0.4347063
# 6     2   38 204900         9  667.7161         1.595433      0.4347063
#        frame scan    tof intensity        mz inv_ion_mobility retention_time
# 669552   992  904 291346         9 1064.7478        0.6153559       106.7103
# 669553   992  909 198994         9  643.9562        0.6097471       106.7103
# 669554   992  909 282616         9 1020.4663        0.6097471       106.7103
# 669555   992  912 143270         9  440.9670        0.6063821       106.7103
# 669556   992  915 309328         9 1158.9221        0.6030173       106.7103
# 669557   992  916 224410         9  749.2647        0.6018958       106.7103



# Get all MS1 frames
# print(query(D, frames=MS1(D)))
# ATTENTION: that's quite a lot of data - you might exceed your RAM.

# Getting a subset of columns is easy - just specify 'columns':
pprint(query(D, frames=c(1,5,67), columns=c('scan','intensity')))
#   scan intensity
# 1   33         9
# 2   34         9
# 3   34         9
# 4   36         9
# 5   36         9
# 6   38        62
#        scan intensity
# 224732  917       189
# 224733  917        51
# 224734  917        54
# 224735  917        19
# 224736  917        57
# 224737  917        95


# Retention time based query (time in seconds):
pprint(rt_query(D, 10, 12))  # seconds
#   frame scan    tof intensity        mz inv_ion_mobility retention_time
# 1    92   33 361758         9 1456.2835         1.601142        10.0869
# 2    92   36  65738         9  222.2822         1.597716        10.0869
# 3    92   41 308330         9 1153.5909         1.592008        10.0869
# 4    92   43 123618         9  378.5190         1.589725        10.0869
# 5    92   48  65346         9  221.3651         1.584017        10.0869
# 6    92   53 183172         9  582.4251         1.578310        10.0869
#        frame scan    tof intensity        mz inv_ion_mobility retention_time
# 128129   109  913  38170         9  162.4016        0.6052605       11.91001
# 128130   109  914 138760        65  426.2142        0.6041389       11.91001
# 128131   109  914 142129        69  437.2109        0.6041389       11.91001
# 128132   109  914 144566        58  445.2528        0.6041389       11.91001
# 128133   109  916 138933        91  426.7755        0.6018958       11.91001
# 128134   109  917 373182         9 1525.5765        0.6007742       11.91001


# All MS1 frames, but one at a time:
for(fr in MS1(D)){
    print(query(D, fr, columns=all_columns))
}


# Bracket indexing extracts raw data (frame, scan, tof, intensity):
pprint(head(D[100]))
#   frame scan    tof intensity
# 1   100   35 389679         9
# 2   100   35 394578         9
# 3   100   37  78036         9
# 4   100   37 210934         9
# 5   100   37 211498         9
# 6   100   37 351984         9
#   frame scan    tof intensity
# 1   100   35 389679         9
# 2   100   35 394578         9
# 3   100   37  78036         9
# 4   100   37 210934         9
# 5   100   37 211498         9
# 6   100   37 351984         9


X = D[10:200]
pprint(X)
#   frame scan    tof intensity
# 1    10   34 171284         9
# 2    10   36  31282         9
# 3    10   38 135057         9
# 4    10   39 135446         9
# 5    10   41 188048         9
# 6    10   42 288608         9
#         frame scan    tof intensity
# 3331314   200  895 318550         9
# 3331315   200  899  57824       126
# 3331316   200  902 314562         9
# 3331317   200  903 375375         9
# 3331318   200  905 358594         9
# 3331319   200  911 146843         9


# Simple access to 'analysis.tdf'? Sure:
tables_names(D)
#  [1] "CalibrationInfo"          "DiaFrameMsMsInfo"        
#  [3] "DiaFrameMsMsWindowGroups" "DiaFrameMsMsWindows"     
#  [5] "ErrorLog"                 "FrameMsMsInfo"           
#  [7] "FrameProperties"          "Frames"                  
#  [9] "GlobalMetadata"           "GroupProperties"         
# [11] "MzCalibration"            "Properties"              
# [13] "PropertyDefinitions"      "PropertyGroups"          
# [15] "Segments"                 "TimsCalibration"         
 

# Just choose a table now (returns a named list of data.frames):
table2df(D, 'TimsCalibration')$TimsCalibration
#   Id ModelType C0  C1       C2       C3 C4 C5           C6       C7       C8
# 1  1         2  1 917 213.5998 75.81729 33  1 -0.009065829 135.4364 13.32608
#         C9
# 1 1663.341
```

# C++

In C++ we offer several functions for the raw access to the data.
To check out how to use the `C++` API, check a basic usage example [/examples/get_data.cpp](https://raw.githubusercontent.com/michalsta/opentims/master/examples/get_data.cpp),
or the full documentation at [/docs/opentims++](https://michalsta.github.io/opentims/opentims++/html/)

## Installing the C++ library

The library is built with CMake and installs a shared or static library, headers, a CMake package config, and a pkg-config file.

**Prerequisites:** CMake ≥ 3.15, a C++20-capable compiler (GCC ≥ 10, Clang ≥ 12, MSVC 2019+), and SQLite3 development headers (`libsqlite3-dev` on Debian/Ubuntu, `sqlite-devel` on Fedora/RHEL, `sqlite` on Homebrew).

### System-wide install (shared library)

Builds and installs to `/usr/local` (or the platform default). Requires `sudo` on Linux/macOS.

```bash
git clone https://github.com/michalsta/opentims
cd opentims
cmake -B build -DBUILD_SHARED_LIBS=ON
cmake --build build -j$(nproc)
sudo cmake --install build
```

The shared library links sqlite3 and zstd at build time, so consumers have no extra runtime dependencies.

### Install into `~/.local` (no root required)

```bash
git clone https://github.com/michalsta/opentims
cd opentims
cmake -B build -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=~/.local
cmake --build build -j$(nproc)
cmake --install build
```

You may need to tell the linker where to find the library:

```bash
export LD_LIBRARY_PATH="$HOME/.local/lib:$LD_LIBRARY_PATH"   # Linux
export DYLD_LIBRARY_PATH="$HOME/.local/lib:$DYLD_LIBRARY_PATH" # macOS
```

And tell pkg-config:

```bash
export PKG_CONFIG_PATH="$HOME/.local/lib/pkgconfig:$PKG_CONFIG_PATH"
```

### Static library variant

Omit `-DBUILD_SHARED_LIBS=ON` to build a static library. If you want sqlite3 linked into the archive (so consumers need no sqlite3 at link time), add `-DOPENTIMS_LINK_SQLITE_STATICALLY=ON`:

```bash
cmake -B build -DOPENTIMS_LINK_SQLITE_STATICALLY=ON -DCMAKE_INSTALL_PREFIX=~/.local
cmake --build build -j$(nproc)
cmake --install build
```

Without `-DOPENTIMS_LINK_SQLITE_STATICALLY=ON`, consumers must supply their own sqlite3 at link time (useful when embedding into a project that already bundles sqlite3, such as OpenMS).

### Using the installed library in your CMake project

After installation, link against `opentims::opentims_cpp` via `find_package`:

```cmake
find_package(opentims REQUIRED)
target_link_libraries(my_target PRIVATE opentims::opentims_cpp)
```

If you installed to a non-standard prefix (e.g. `~/.local`), pass it to CMake:

```bash
cmake -B build -DCMAKE_PREFIX_PATH=~/.local
```

### Using pkg-config

```bash
pkg-config --cflags --libs opentims
```



# More options?

Consider [TimsPy](https://github.com/MatteoLacki/timspy) and [TimsR](https://github.com/MatteoLacki/timsr) for more user-friendly options.

# Development
We will be happy to accept any contributions.

# Notes on conversion accuracy

OpenTIMS ships built-in open-source converters for tof→m/z and scan→inverse ion mobility, enabled via `setup_opensource()` in both Python and R.
These are derived from the acquisition metadata and are suitable for most use cases.
Bruker's proprietary conversion functions (available via [opentims_bruker_bridge](https://github.com/MatteoLacki/opentims_bruker_bridge) on Linux and Windows) may give slightly more accurate results in some edge cases; they are used automatically when available.

## Licence

OpenTIMS is released under the terms of MIT licence. Full text below in LICENCE file.
If you require other licensing terms please contact the authors.

OpenTIMS contains built-in versions of the following software:
- sqlite3, public domain
- ZSTD, BSD licence
- mio, MIT licence

See the respective files for details.
The [opentims_bruker_bridge](https://github.com/MatteoLacki/opentims_bruker_bridge) module ships Bruker's proprietary tof→m/z and scan→drift time conversion binaries under a separate license.

If the above license terms do not suit you, please contact us.
We are open to discussion about your particular licensing needs.

## Special thanks
We would like to thank Michael Krause, Sascha Winter, and Sven Brehmer, all from Bruker Daltonik GmbH, for their magnificent work in developing tfd-sdk.

