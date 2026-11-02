# spdlog 1.17.0

Source: https://github.com/gabime/spdlog/releases/tag/v1.17.0

Archive: https://codeload.github.com/gabime/spdlog/zip/refs/tags/v1.17.0

Archive SHA256: `b11912a82d149792fef33fabd0503b13d54aeac25c1464755461d4108ea71fc2`

The complete upstream `include/` directory and `LICENSE` are vendored unchanged.
The bundled fmt headers retain their upstream license notices. No network access,
package manager, or separate fmt installation is needed when building.
`MANIFEST.sha256` records the hashes of all 106 upstream files (105 headers plus
LICENSE), verified against the downloaded archive.

Core privately uses the header-only target, compiling the logging backend into
Core.dll. Consumers use `Logging/Log.h` (C++20 standard formatting) and do not
instantiate a separate spdlog registry in each engine DLL.
