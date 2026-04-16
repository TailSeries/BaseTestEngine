cmake_minimum_required(VERSION 3.20.0)
project(ConsoleMain)
FILE(GLOB_RECURSE source_list
    ./*.c
    ./*.cpp
    ./*.h
    ./*.hpp)
message("ConsoleMain CMAKE_BINARY_DIR = ${CMAKE_BINARY_DIR}")
message("ConsoleMain CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}")

option(BUILD_SHARED_LIBS "Build shared library" OFF)

find_package(Threads)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${source_list})


if(MSVC)
    set(MSVC_COMPILE_FLAGS "/MP" CACHE STRING "Compiler flags to use with MSVC")
    set(CMAKE_DEBUG_POSTFIX "d")  
endif()


message("ConsoleMain source_list = ${source_list}")
add_executable(ConsoleMain ${source_list})
target_link_options(ConsoleMain PRIVATE /SUBSYSTEM:CONSOLE)

if(MSVC)
    target_compile_definitions(ConsoleMain PRIVATE -D_CRT_SECURE_NO_WARNINGS)    
endif()
target_link_libraries(ConsoleMain Threads::Threads)
target_include_directories(ConsoleMain PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(ConsoleMain PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)  # 支持 "Algorithm/ModulePublic/..." 写法
target_link_libraries(ConsoleMain Algorithm)