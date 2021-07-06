get_data_cpp: support
	$(CXX) -std=c++17 sqlite3.o zstddeclib.o get_data.cpp -o get_data -ldl -lpthread -g -Og
support: sqlite3.o zstddeclib.o
zstddeclib.o: ../opentims++/zstd/zstddeclib.c
	$(CC) ../opentims++/zstd/zstddeclib.c -c -o zstddeclib.o
sqlite3.o: ../opentims++/sqlite/sqlite3.c
	$(CC) ../opentims++/sqlite/sqlite3.c -c -o sqlite3.o
clean:
	rm -f get_data *.o
