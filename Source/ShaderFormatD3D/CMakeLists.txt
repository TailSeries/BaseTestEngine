add_library(ShaderFormatD3D STATIC Private/D3DShaderCompiler.cpp)
target_include_directories(ShaderFormatD3D PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Public")
target_link_libraries(ShaderFormatD3D PUBLIC RenderCore PRIVATE d3dcompiler)
