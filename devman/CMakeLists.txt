cmake_minimum_required(VERSION 3.10)
project(devman)

# print make information
#include(../arm_info.cmake)

# add include path
include_directories(
        ${CMAKE_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/../lib_dsv/inc
        /usr/local/include
		)

link_directories(
		${CMAKE_SOURCE_DIR}/../lib_dsv/build
        ${CMAKE_SOURCE_DIR}/../cjson/build
        /usr/local/lib
        )
set(CMAKE_BUILD_TYPE Debug)
aux_source_directory(${CMAKE_SOURCE_DIR}/src DIR_SRCS)
add_executable(${PROJECT_NAME} ${DIR_SRCS})
target_link_libraries(${PROJECT_NAME} PUBLIC dsv cjson czmq zmq)


