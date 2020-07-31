cmake_minimum_required(VERSION 3.10)
project(multiport_network)

set(CMAKE_CXX_STANDARD 11)

add_subdirectory(test)

add_subdirectory(micro_test)

add_subdirectory(cuckoo_local_test)

add_subdirectory(get_batch)

add_subdirectory(double_buf_get)

link_libraries(pthread event event_core)

add_executable(multiport_network server.cpp kvobj.cpp assoc.cpp)

add_executable(simple_test simple_test.cpp kvobj.cpp)