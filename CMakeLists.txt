
cmake_minimum_required (VERSION 3.5)

project (dsv)

set (CMAKE_CXX_STANDARD 17)
add_subdirectory(libdsv)
add_subdirectory(dsv_server)
add_subdirectory(dsv)
add_subdirectory(asv)
add_subdirectory(devman)
