cmake_minimum_required(VERSION 3.20.0)
project(BaseTestEngine LANGUAGES CXX)
message("CMAKE_C_COMPILER = ${CMAKE_C_COMPILER}")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("CMAKE_CURRENT_LIST_DIR = ${CMAKE_CURRENT_LIST_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../out/bin") # 针对可执行文件，以及windows的动态库 pdb
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../out/bin") # 针对linux静态库，以及windows的dll的依赖lib文件

add_compile_definitions(NOMINMAX SYSTEM_WIN PLATFORM_WIN)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_subdirectory(Core)
add_subdirectory(RHI)
add_subdirectory(D3D12RHI)
add_subdirectory(RenderCore)
add_subdirectory(ShaderFormatD3D)
add_subdirectory(Engine)
add_subdirectory(Renderer)
add_subdirectory(RHITest)

# 编辑器是可选宿主；RHI / Renderer 不依赖 Qt。未安装 SDK 时照常构建原有目标。
option(BASETEST_BUILD_EDITOR "Build the Qt Quick editor (requires Qt 6.8+)" OFF)
if(BASETEST_BUILD_EDITOR)
    add_subdirectory(Editor)
endif()

#add_subdirectory(Transfer)
add_subdirectory(Algorithm)
add_subdirectory(DirectX12)
add_subdirectory(Main)
add_subdirectory(ConsoleMain)
message("CMAKE_BINARY_DIR = ${CMAKE_BINARY_DIR};  CMAKE_ARCHIVE_OUTPUT_DIRECTORY = ${}")

#target_link_libraries(Main Transfer)

