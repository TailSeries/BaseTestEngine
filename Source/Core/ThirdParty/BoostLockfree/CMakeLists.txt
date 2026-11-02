# Vendored headers: no package installation, downloads, or Boost binaries needed.
add_library(CoreBoostLockfree INTERFACE)
add_library(CoreThirdParty::BoostLockfree ALIAS CoreBoostLockfree)
target_include_directories(CoreBoostLockfree SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
