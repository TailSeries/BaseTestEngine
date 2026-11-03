# 只建立本节需要的 Engine 渲染资源接口，不引入 UObject/场景系统。
add_library(Engine STATIC Private/LocalVertexFactory.cpp)
target_include_directories(Engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Public")
target_link_libraries(Engine PUBLIC RenderCore)
