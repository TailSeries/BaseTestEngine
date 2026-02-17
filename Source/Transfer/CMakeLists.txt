cmake_minimum_required(VERSION 3.20.0)
set(TRANSER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
FILE(GLOB_RECURSE source_list
    ${TRANSER_SRC_DIR}/*.c
    ${TRANSER_SRC_DIR}/*.cpp
    ${TRANSER_SRC_DIR}/*.h
    ${TRANSER_SRC_DIR}/*.hpp)
message("Transfer source_list = ${source_list}")
message("Transfer CMAKE_RUNTIME_OUTPUT_DIRECTORY = ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
source_group(TREE "${TRANSER_SRC_DIR}" PREFIX "Source Files" FILES ${source_list})
add_library(Transfer SHARED ${source_list})
target_compile_definitions(Transfer PRIVATE TRANSFER_LIBRARY)
target_include_directories(Transfer PRIVATE TRANSER_SRC_DIR)