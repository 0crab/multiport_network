cmake_minimum_required(VERSION 3.15)
project(multiport_network)

set(CMAKE_CXX_STANDARD 11)

link_libraries(pthread event event_core)

add_executable(multiport_network server.cpp)