cmake_minimum_required(VERSION 3.20.0)
FILE(GLOB_RECURSE source_list
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)
message("Algorithm source_list = ${source_list}")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${source_list})
add_library(Algorithm SHARED ${source_list})
target_compile_definitions(Algorithm PRIVATE ALGORITHM_LIBRARY)
target_include_directories(Algorithm PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ModulePrivate")
target_include_directories(Algorithm PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/ModulePublic")