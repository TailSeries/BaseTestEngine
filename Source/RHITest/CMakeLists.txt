cmake_minimum_required(VERSION 3.20.0)
project(RHITest)
FILE(GLOB_RECURSE source_list CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
add_executable(RHITest ${source_list})
target_link_options(RHITest PRIVATE /SUBSYSTEM:WINDOWS)
target_link_libraries(RHITest D3D12RHI ShaderFormatD3D)      # 传递带上 RHI / Core / d3d12 / dxgi
set_target_properties(RHITest PROPERTIES LINK_FLAGS
    "/NODEFAULTLIB:LIBCMT.lib /NODEFAULTLIB:LIBCMTD.lib /ignore:4099")

# 教学期直接从源码目录加载；打包/安装时需改为部署后的 Shader 目录。
target_compile_definitions(RHITest PRIVATE RHITEST_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Shaders")
