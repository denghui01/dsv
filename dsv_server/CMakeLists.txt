cmake_minimum_required(VERSION 3.10)
project(dsv_server)

# print make information
#include(../arm_info.cmake)

# add include path
include_directories(
		${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/../libdsv/inc
		)

link_directories(
		${CMAKE_CURRENT_SOURCE_DIR}/../build/libdsv
        )

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src DIR_SRCS)
add_executable(${PROJECT_NAME} ${DIR_SRCS})
#target_link_libraries(${PROJECT_NAME} PUBLIC dsv cjson czmq ${CMAKE_CURRENT_SOURCE_DIR}/../libzmq/src/.libs/libzmq.a unwind )
target_link_libraries(${PROJECT_NAME} PUBLIC dsv cjson czmq zmq )
