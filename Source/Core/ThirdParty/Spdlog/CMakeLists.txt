add_library(CoreSpdlog INTERFACE)
add_library(CoreThirdParty::Spdlog ALIAS CoreSpdlog)
target_include_directories(CoreSpdlog SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
# Only Core's implementation includes spdlog; no extra DLL or public registry.
target_compile_features(CoreSpdlog INTERFACE cxx_std_20)
