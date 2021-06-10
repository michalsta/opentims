#!/bin/bash
echo "starting build..."
g++ -O3 -std=c++1z -I/usr/include -I$JAVA_HOME/include -I$JAVA_HOME/include/linux timsj_TimsTOFExperimentReader.cpp -lsqlite3 -lzstd -shared -fPIC -o libtimstofexperimentreader.so
echo "done."
