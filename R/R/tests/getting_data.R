library(devtools)
library(opentims)
library(data.table)
library(microbenchmark)
library(hexbin)

load_all()
path_dll = '/home/matteo/Projects/opentims/opentims_bruker_bridge/opentims_bruker_bridge/libtimsdata.so'
path = '/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d'

setup_bruker_so(path_dll)

D = OpenTIMS(path)
x = tdf_extract_frames(D@handle, c(1,4,40), T,T,T,T,F,F,F)
y = tdf_extract_frames(D@handle, c(4,40,60), T,T,T,T,T,T,T)

as.data.table(query(D, frames=1:10, columns=c('frame', 'scan', 'tof', 'intensity')))
as.data.table(y)

y = tdf_extract_frames_slice(D@handle, 1,100,10, T,T,T,T,T,T,T)

as.data.table(query(D, start=1, end=40, columns=c('frame', 'scan', 'tof', 'intensity')))

x <- 2:18
v <- c(5, 10, 15) # create two bins [5,10) and [10,15)
cbind(x, findInterval(x, v))
?findInterval

table(y$frame)

microbenchmark(as.data.frame(x), data.frame(x), as.data.table(x))


document()
document()
build()
install()
load_all()


x <- 2:18
v <- c(5, 10, 15) # create two bins [5,10) and [10,15)
cbind(x, findInterval(x, v))


library(Rcpp)

cppFunction('void Test(LogicalVector LV){
	int n = LV.size();
	for(int i=0; i<n; ++i){
		Rcout << LV[i] << " ";
	}
	Rcout << "\\n";
	for(int i=0; i<n; ++i){
		LV[i] = true;
	}
	for(int i=0; i<n; ++i){
		Rcout << LV[i] << " ";
	}
	Rcout << "\\n";
}')

cppFunction('void Test(IntegerVector LV){
	Rcout << LV[0];
}')
Test(c(10,4))


x = c(F,T,T)
Test(x)

cppFunction('void Test(bool test){
	if(test){ Rcout << "Dupa.\\n"; } else { Rcout << "Cipa\\n";}
}')

Test(F)