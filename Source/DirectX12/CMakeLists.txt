cmake_minimum_required(VERSION 3.20.0)
set(target_name DirectX12)
FILE(GLOB_RECURSE source_list
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)
message("DirectX12 source_list = ${source_list}")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${source_list})
add_library(DirectX12 SHARED ${source_list})
target_compile_definitions(DirectX12 PRIVATE DirectX12_LIBRARY)
target_include_directories(DirectX12 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(${target_name} PROPERTIES LINK_FLAGS
	"/NODEFAULTLIB:LIBCMT.lib /NODEFAULTLIB:LIBCMTD.lib /ignore:4099"
)
target_link_libraries(${target_name} Core winmm d3d12 dxgi d3dcompiler)