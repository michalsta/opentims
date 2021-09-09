JH=${JAVA_HOME}

main: support
	$(CXX) -O3 -Wall -shared -std=c++2a sqlite3.o zstddeclib.o -I${JH}/include -I${JH}/include/linux timsj_TimsTOFExperimentReader.cpp -shared -fPIC -o libtimstofexperimentreader.so
support: sqlite3.o zstddeclib.o
