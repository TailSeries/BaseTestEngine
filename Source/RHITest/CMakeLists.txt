cmake_minimum_required(VERSION 3.20.0)
project(RHITest)
FILE(GLOB_RECURSE source_list CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
# Qt 编辑器和独立 Win32 示例共享同一个 RHITestPeriod1，不复制渲染代码。
list(REMOVE_ITEM source_list "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
add_library(RHITestSupport STATIC ${source_list})
target_include_directories(RHITestSupport PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(RHITestSupport PUBLIC D3D12RHI ShaderFormatD3D Engine Renderer)
add_executable(RHITest main.cpp)
target_link_options(RHITest PRIVATE /SUBSYSTEM:WINDOWS)
target_link_libraries(RHITest PRIVATE RHITestSupport)
set_target_properties(RHITest PROPERTIES LINK_FLAGS
    "/NODEFAULTLIB:LIBCMT.lib /NODEFAULTLIB:LIBCMTD.lib /ignore:4099")

# 教学期直接从源码目录加载；打包/安装时需改为部署后的 Shader 目录。
target_compile_definitions(RHITestSupport PRIVATE RHITEST_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Shaders")
