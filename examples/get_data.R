library(opentims)

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
