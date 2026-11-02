cmake_minimum_required(VERSION 3.20.0)
set(target_name Core)
add_subdirectory(ThirdParty/BoostLockfree)
add_subdirectory(ThirdParty/Spdlog)
FILE(GLOB_RECURSE source_list CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)
list(FILTER source_list EXCLUDE REGEX "/(ThirdParty|Tests)/")
message("Core source_list = ${source_list}")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${source_list})
add_library(Core SHARED ${source_list})
target_link_libraries(Core PUBLIC CoreThirdParty::BoostLockfree)
target_link_libraries(Core PRIVATE CoreThirdParty::Spdlog)
target_compile_features(Core PUBLIC cxx_std_20)
target_compile_definitions(Core PRIVATE Core_LIBRARY)
target_include_directories(Core PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/Base"
    "${CMAKE_CURRENT_SOURCE_DIR}/Common" 
    "${CMAKE_CURRENT_SOURCE_DIR}/Task"
    "${CMAKE_CURRENT_SOURCE_DIR}/Threads"
)
set_target_properties(${target_name} PROPERTIES LINK_FLAGS
	"/NODEFAULTLIB:LIBCMT.lib /NODEFAULTLIB:LIBCMTD.lib /ignore:4099"
)


