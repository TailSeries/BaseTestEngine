cmake_minimum_required(VERSION 3.20.0)
set(target_name D3D12RHI)
FILE(GLOB_RECURSE source_list CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${source_list})
add_library(D3D12RHI SHARED ${source_list})
target_compile_definitions(D3D12RHI PRIVATE D3D12RHI_LIBRARY)
target_include_directories(D3D12RHI PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/Public"
)
# 注：RHI 现为空壳（无导出符号 → 不生成 RHI.lib），暂不链接；第2章 RHI 有资源类后再加回 RHI
target_link_libraries(D3D12RHI Core d3d12 dxgi d3dcompiler)
set_target_properties(${target_name} PROPERTIES LINK_FLAGS
    "/NODEFAULTLIB:LIBCMT.lib /NODEFAULTLIB:LIBCMTD.lib /ignore:4099"
)