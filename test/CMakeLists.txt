cmake_minimum_required(VERSION 3.10)

link_libraries(pthread)

add_executable(benchmark_test benchmark_test.cpp)

#add_executable(test_ntol test_ntol.cpp)

#add_executable(client  clien_test.cpp client_tool.cpp)

add_executable(network_test network_test.cpp)

#add_executable(micro_test micro_test.cpp)
