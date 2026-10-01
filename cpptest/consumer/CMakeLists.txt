# Builds against an installed opentims C++ library, as a downstream project
# would. Used by CI to check the installed CMake package and headers:
#   cmake -S cpptest/consumer -B build-consumer -DCMAKE_PREFIX_PATH=<install prefix>
cmake_minimum_required(VERSION 3.15)
project(opentims_consumer LANGUAGES CXX)

find_package(opentims REQUIRED)
add_executable(opentims_consumer consumer.cpp)
target_compile_features(opentims_consumer PRIVATE cxx_std_20)
target_link_libraries(opentims_consumer PRIVATE opentims::opentims_cpp)
