# 当前 RenderCore 只有与平台无关的编译数据类型；后续加入 FShader 等实现。
add_library(RenderCore INTERFACE)
target_include_directories(RenderCore INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/Public")
target_link_libraries(RenderCore INTERFACE RHI)
