all: zstddeclib.o sqlite3.o
	g++ test.cpp -Wall -Wextra -pedantic -std=c++14 -O3 -g zstddeclib.o sqlite3.o -lpthread -ldl
	#clang++ sqlite3.o tims2hdf5.cpp -lpthread -ldl -lzstd -lhdf5 -g -o tims2hdf5
check-gcc:
	g++ -fsyntax-only opentims.cpp scan2drift_converter.cpp tof2mz_converter.cpp -std=c++14
dbg: zstddeclib.o sqlite3.o
	g++ test.cpp -Wall -Wextra -pedantic -std=c++14 -Og -g zstddeclib.o sqlite3.o -lpthread -ldl
sqlite3.o: sqlite/*
	gcc -c -O2 sqlite/sqlite3.c -o sqlite3.o
zstddeclib.o: zstd/*
	gcc -c -O3 zstd/zstddeclib.c -o zstddeclib.o
check:
	clang++ opentims.cpp -Weverything -fsyntax-only -std=c++14 -Wno-c++98-compat
clean:
	rm -f *.o a.out *.gch
