cmake_minimum_required(VERSION 3.10)
project(multiport_network)

set(CMAKE_CXX_STANDARD 11)

add_subdirectory(test)

add_subdirectory(micro_test)

link_libraries(pthread event event_core)

add_executable(multiport_network server.cpp kvobj.cpp assoc.cpp)

add_executable(simple_test simple_test.cpp kvobj.cpp)