cmake_minimum_required(VERSION 3.20.0)
project(Main)
FILE(GLOB_RECURSE source_list
    ./*.c
    ./*.cpp
    ./*.h
    ./*.hpp)
message("main CMAKE_BINARY_DIR = ${CMAKE_BINARY_DIR}")
message("main CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}")
# cmake��Ŀ�����һ���ֵ�As��Ŀ�ı�Ҫ����
option(BUILD_SHARED_LIBS "Build shared library" OFF)
option(AS_NO_EXCEPTIONS "Disable exception handling in script context" OFF)
file(READ ./AngelScript/include/angelscript.h ANGELSCRIPT_H)
string(REGEX MATCH "#define ANGELSCRIPT_VERSION_STRING \"([0-9]*).([0-9]*).([0-9]*)" ANGELSCRIPT_VERSION_REGEX ${ANGELSCRIPT_H})
set(ANGELSCRIPT_VERSION_MAJOR ${CMAKE_MATCH_1})
set(ANGELSCRIPT_VERSION_MINOR ${CMAKE_MATCH_2})
set(ANGELSCRIPT_VERSION_PATCH ${CMAKE_MATCH_3})
set(PROJECT_VERSION ${ANGELSCRIPT_VERSION_MAJOR}.${ANGELSCRIPT_VERSION_MINOR}.${ANGELSCRIPT_VERSION_PATCH})

message(STATUS "Configuring angelscript ${PROJECT_VERSION}")
find_package(Threads)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${source_list})
if(MSVC AND CMAKE_CL_64)
    enable_language(ASM_MASM)
    if(CMAKE_ASM_MASM_COMPILER_WORKS)
        set(source_list ${source_list} ${CMAKE_CURRENT_SOURCE_DIR}/AngelScript/source/as_callfunc_x64_msvc_asm.asm)
        message("MSVC x86_64 set asm success ANGELSCRIPT_SOURCE = ${ANGELSCRIPT_SOURCE}")
    else()
        message(FATAL ERROR "MSVC x86_64 target requires a working assembler")
    endif()
endif()

if(MSVC)
    set(MSVC_COMPILE_FLAGS "/MP" CACHE STRING "Compiler flags to use with MSVC")
    set(CMAKE_DEBUG_POSTFIX "d")  
    set(ANGELSCRIPT_LIBRARY_NAME angelscript)
endif()


message("main source_list = ${source_list}")
add_executable(Main ${source_list})
target_link_options(Main PRIVATE /SUBSYSTEM:WINDOWS)

if(MSVC)
    target_compile_definitions(Main PRIVATE -D_CRT_SECURE_NO_WARNINGS)    
endif()
target_link_libraries(Main Threads::Threads)
target_include_directories(Main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(Main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/AngelScript/include)
target_include_directories(Main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)  # 支持 "Algorithm/ModulePublic/..." 写法

#���ӿ�
target_link_libraries(Main Algorithm DirectX12)