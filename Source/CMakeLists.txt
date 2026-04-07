cmake_minimum_required(VERSION 3.20.0)
project(BaseTestEngine LANGUAGES CXX)
message("CMAKE_C_COMPILER = ${CMAKE_C_COMPILER}")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("CMAKE_CURRENT_LIST_DIR = ${CMAKE_CURRENT_LIST_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../out/bin") # 针对可执行文件，以及windows的动态库 pdb
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../out/bin") # 针对linux静态库，以及windows的dll的依赖lib文件

add_definitions(-DNOMINMAX)
#add_subdirectory(Transfer)
add_subdirectory(Algorithm)
add_subdirectory(Main)
message("CMAKE_BINARY_DIR = ${CMAKE_BINARY_DIR};  CMAKE_ARCHIVE_OUTPUT_DIRECTORY = ${}")

#target_link_libraries(Main Transfer)

