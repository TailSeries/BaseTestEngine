add_library(Renderer STATIC Private/MeshPassProcessor.cpp)
target_include_directories(Renderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Public")
target_link_libraries(Renderer PUBLIC Engine RenderCore)
