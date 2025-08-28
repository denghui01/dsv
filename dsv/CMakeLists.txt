cmake_minimum_required(VERSION 3.10)
project(sv)

# print make information
#include(../arm_info.cmake)

# add include path
include_directories(
        #${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../libdsv/inc
		)

link_directories(
		${CMAKE_CURRENT_SOURCE_DIR}/../build/libdsv
        )
set(CMAKE_BUILD_TYPE Debug)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src DIR_SRCS)
add_executable(${PROJECT_NAME} ${DIR_SRCS})

if(CMAKE_CROSSCOMPILING)
    target_link_libraries(${PROJECT_NAME} PUBLIC dsv cjson czmq ${CMAKE_CURRENT_SOURCE_DIR}/../libzmq/src/.libs/libzmq.a unwind )
else()
    target_link_libraries(${PROJECT_NAME} PUBLIC dsv cjson czmq zmq )
endif()



