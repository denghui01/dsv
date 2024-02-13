cmake_minimum_required(VERSION 3.10)
project(dsv)

set(PROJ_OBJ ${PROJECT_NAME}-objs)
set(PROJ_SHARED ${PROJECT_NAME}-shared)
set(PROJ_STATIC ${PROJECT_NAME}-static)

# print make information
#include(../arm_info.cmake)

# add include path
include_directories(
		${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/..
		)

link_directories(
		#${CMAKE_CURRENT_SOURCE_DIR}/../libcfu/build
                )

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src DIR_SRCS)

add_library(${PROJ_OBJ}
    OBJECT
        ${DIR_SRCS}
)
add_library(${PROJ_SHARED}
    SHARED
        $<TARGET_OBJECTS:${PROJ_OBJ}>
)
set_target_properties(${PROJ_SHARED}
	PROPERTIES
		OUTPUT_NAME ${PROJECT_NAME}
)
add_library(${PROJ_STATIC}
    STATIC
        $<TARGET_OBJECTS:${PROJ_OBJ}>
)
set_target_properties(${PROJ_STATIC}
	PROPERTIES
		OUTPUT_NAME ${PROJECT_NAME}
)