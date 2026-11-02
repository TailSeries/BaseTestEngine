# RenderCore 只依赖 RHI，不依赖 D3D 编译器或后端。
add_library(RenderCore STATIC Private/Shader.cpp Private/ShaderParameters.cpp Private/GlobalShader.cpp Private/RenderResource.cpp)
target_include_directories(RenderCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Public")
target_link_libraries(RenderCore PUBLIC RHI)
