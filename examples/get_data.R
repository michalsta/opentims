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
