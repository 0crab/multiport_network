cmake_minimum_required(VERSION 3.10)

link_libraries(pthread)

add_executable(benchmark_test benchmark_test.cpp)

add_executable(network_test network_test.cpp)