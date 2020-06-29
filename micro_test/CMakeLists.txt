cmake_minimum_required(VERSION 3.10)

link_libraries(pthread)

add_executable(test_hash test_hash.cpp)

add_executable(micro_test  micro_test.cpp)



