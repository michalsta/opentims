# OpenTIMS

`OpenTIMS` is a C++ library for accessing timsTOF Pro data format (TDF).
It replaces (to a large extent) Bruker's SDK for purposes of data access and provides convenient layer for integration into higher level computer languages.
It comes with bindings to `Python` (through `opentimspy`) and `R` languages (through `opentimsr`).
In `Python`, we extract data into `NumPy` arrays that are optimized for speed and come with a universe of useful methods for their quick manipulation.
In `R`, we extract data into the native `data.frame` object.

With `OpenTIMS` you can access data contained in the `analysis.tdf_raw` file hapilly produced by your mass spectrometer of choice (as long as it is timsTOF Pro).
It also parses some of the information out of the `SQLite` data base contained in the `analysis.raw` file.
You should have both of these files in one folder to start using our software.

We can also get your data faster in `C++` (and so to `Python` and `R`):
![](https://github.com/michalsta/opentims/blob/master/speed.png "TIC per frame")

Prefer userfriendliness over raw power?
We have you covered! Check out the children projects [`TimsR`](https://github.com/MatteoLacki/timsr) and [`TimsPy`](https://github.com/MatteoLacki/timspy).

# Requirements

The software was tested on Linux, Windows, and MacOS.
On Windows, install Microsoft Visual Studio from [here](https://visualstudio.microsoft.com/visual-cpp-build-tools/) to make use of C++ or Python code.
On Linux, have `clang++` or `g++` installed (`clang` produces slightly faster code).
Also, do make sure that a developper version of Python is installed.
For instance, on Ubuntu, install Python with 
```
sudo apt install python3.8-dev
```
i.e. with the `-dev` version.
This contains headers needed for pybind to work properly.
On macOS, [install x-tools command line tools](https://www.godo.dev/tutorials/xcode-command-line-tools-installation-faq/).

## Python
 
From terminal (assuming you have python and pip included in the system PATH) write
```bash
pip install opentimspy
```
We recommend also installing the opentims_bruker_bridge module, containing Bruker's proprietary conversion functions (Linux and Windows only). To do that, do:
```bash
pip install opentims_bruker_bridge
```

**On Windows**: we have noticed issues with the numpy==1.19.4 due to changes in Intel's fmod function, unrelated to our work. 
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
or using devtools
```bash
install.packages('devtools')
library(devtools)

install_github("michalsta/opentims", subdir="opentimsr")
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

# path = pathlib.Path('path_to_your_data.d')
path = "/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d"

# Do you want to have access only to 'frame', 'scan', 'time of flight', and 'intensity'?
accept_Bruker_EULA_and_on_Windows_or_Linux = TRUE

if(accept_Bruker_EULA_and_on_Windows_or_Linux){
    folder_to_stode_priopriatary_code = "/home/matteo"
    path_to_bruker_dll = download_bruker_proprietary_code(folder_to_stode_priopriatary_code)
    setup_bruker_so(path_to_bruker_dll)
    all_columns = c('frame','scan','tof','intensity','mz','inv_ion_mobility','retention_time')
} else {
    all_columns = c('frame','scan','tof','intensity','retention_time')
}

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



# Get a dict with each 10th frame, starting from frame 2, finishing on frame 1000.   
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
# ATTENTION: that's quite a lot of data!!! And R will first make a stupid copy, because it's bad. You might exceed your RAM.

# Getting subset of columns: simply specify 'columns':
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
# 
# this is also the only way to get data without accepting Bruker terms of service and on MacOS (for time being).


# The frame lasts a convenient time unit that well suits chromatography peak elution.
# What if you were interested instead in finding out which frames eluted in a given time 
# time of the experiment?
# For this reasone, we have prepared a retention time based query:
# suppose you are interested in all frames corresponding to all that eluted between 10 and 12
# second of the experiment.
pprint(rt_query(D, 10, 12))
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


# R has no proper in-built iterators :(

# All MS1 frames, but one at a time:
for(fr in MS1(D)){
    print(query(D, fr, columns=all_columns))
}


# Syntactic sugar: only the real bruker data can also be extracted this way:
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


# Simple access to 'analysis.bin'? Sure:
tables_names(D)
#  [1] "CalibrationInfo"          "DiaFrameMsMsInfo"        
#  [3] "DiaFrameMsMsWindowGroups" "DiaFrameMsMsWindows"     
#  [5] "ErrorLog"                 "FrameMsMsInfo"           
#  [7] "FrameProperties"          "Frames"                  
#  [9] "GlobalMetadata"           "GroupProperties"         
# [11] "MzCalibration"            "Properties"              
# [13] "PropertyDefinitions"      "PropertyGroups"          
# [15] "Segments"                 "TimsCalibration"         
 

# Just choose a table now:
table2df(D, 'TimsCalibration')
#   Id ModelType C0  C1       C2       C3 C4 C5           C6       C7       C8
# 1  1         2  1 917 213.5998 75.81729 33  1 -0.009065829 135.4364 13.32608
#         C9
# 1 1663.341
```

# C++

In C++ we offer several functions for the raw access to the data.
To check out how to use the `C++` API, check a basic usage example [/examples/get_data.cpp](https://raw.githubusercontent.com/michalsta/opentims/master/examples/get_data.cpp),
or the full documentation at [/docs/opentims++](https://michalsta.github.io/opentims/opentims++/html/)



# More options?

Consider [TimsPy](https://github.com/MatteoLacki/timspy) and [TimsR](https://github.com/MatteoLacki/timsr) for more user-friendly options.

# Development
We will be happy to accept any contributions.

# Current limitations
Due to patent restrictions, open source calibration functions used by Bruker cannot be revealed.
For this reason, we have to use the original Bruker TDF-SDK for time of flight to mass over charge and scan to inverse ion mobility transformations.
To make it as easy at can be, we have prepared a `Python` module called [opentims_bruker_bridge](https://github.com/MatteoLacki/opentims_bruker_bridge) that ships the necessary `dll` and `so` files.
Please visit the project and follow language-specific instructions for its installation.

# Plans for future

* Together with Bruker we are working on opening up the tof-mz and scan-dt conversions which is scheduled for the next release of the acquisition software.
* This way fully open source access will be available on all of the commonly used platforms.
* Adding bindings to other languages.

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

If the above license terms do not suit you, please contact us.
We are open to discussion about your particular licensing needs.

## Special thanks
We would like to thank Michael Krause, Sascha Winter, and Sven Brehmer, all from Bruker Daltonik GmbH, for their magnificent work in developing tfd-sdk.


## Knowns Issues:

### *pybind11* causes an error upon installation:

```bash
  Building wheel for opentimspy (setup.py) ... error
  ERROR: Command errored out with exit status 1:
   command: /home/matteo/Projects/opentims/timspy/timspyVE/bin/python -u -c 'import sys, setuptools, tokenize; sys.argv[0] = '"'"'/tmp/pip-req-build-06parpqo/setup.py'"'"'; __file__='"'"'/tmp/pip-req-build-06parpqo/setup.py'"'"';f=getattr(tokenize, '"'"'open'"'"', open)(__file__);code=f.read().replace('"'"'\r\n'"'"', '"'"'\n'"'"');f.close();exec(compile(code, __file__, '"'"'exec'"'"'))' bdist_wheel -d /tmp/pip-wheel-rq1ll2m4
       cwd: /tmp/pip-req-build-06parpqo/
  Complete output (11 lines):
  running bdist_wheel
  running build
  running build_py
  creating build
  creating build/lib.linux-x86_64-3.8
  creating build/lib.linux-x86_64-3.8/opentimspy
  copying opentimspy/opentims.py -> build/lib.linux-x86_64-3.8/opentimspy
  copying opentimspy/__init__.py -> build/lib.linux-x86_64-3.8/opentimspy
  running build_ext
  building 'opentimspy_support' extension
  pybind11 not found. Please either install it manually, or install via pip rather than through setuptools directly.
  ----------------------------------------
  ERROR: Failed building wheel for opentimspy
```

Ignore it: it is pip-related and does not influence the intallation.

### R function rt_query

If you see error:
`Error in col[1] : object of type 'closure' is not subsettable`
then interpret the following set of functions:
```R
get_left_frame = function(x,y) ifelse(x > y[length(y)], NA, findInterval(x, y, left.open=T) + 1)
get_right_frame = function(x,y) ifelse(x < y[1], NA, findInterval(x, y, left.open=F))

rt_query = function(opentims,
                    min_retention_time,
                    max_retention_time,
                    columns=c('frame','scan','tof','intensity','mz','inv_ion_mobility','retention_time')
){
  RTS = retention_times(opentims)
  
  min_frame = get_left_frame(min_retention_time, RTS)
  max_frame = get_right_frame(max_retention_time, RTS)
  
  if(is.na(min_frame) | is.na(max_frame))
    stop("The [min_retention_time,max_retention_time] interval does not hold any data.")
  
  query(opentims, min_frame:max_frame, columns=columns)
}
```

