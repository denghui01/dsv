cmake_minimum_required(VERSION 3.10)
project(dsv_server)

# print make information
#include(../arm_info.cmake)

# add include path
include_directories(
		${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/../lib_dsv/inc
        /usr/local/include
		)

link_directories(
		${CMAKE_CURRENT_SOURCE_DIR}/../lib_dsv/build
        ${CMAKE_CURRENT_SOURCE_DIR}/../cjson/build
        /usr/local/lib
                )

#file(GLOB_RECURSE CFILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
#SET_SOURCE_FILES_PROPERTIES(${CFILES} PROPERTIES LANGUAGE CXX )

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src DIR_SRCS)
add_executable(${PROJECT_NAME} ${DIR_SRCS})
target_link_libraries(${PROJECT_NAME} PUBLIC dsv cjson czmq zmq )
