# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**BaseTestEngine** is a Windows C++20 learning/experimental project for engine development practice. It consists of three CMake modules built with MSVC and Ninja.

## Build Commands

```bash
# Debug build
cmake -B "../out/build/x64-Debug" -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build "../out/build/x64-Debug"

# Release build (RelWithDebInfo)
cmake -B "../out/build/x64-Release" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "../out/build/x64-Release"
```

Output goes to `../out/bin/` (executables and DLLs).

No automated test framework — tests are exercised by running `Main.exe` directly.

## Architecture

Three CMake targets, all output to `../out/bin/`:

### Main (Executable)
Entry point at `Main/main.cpp`. Serves as a test harness that exercises:
- **AngelScript integration**: Registers C++ functions/types, loads `.as` scripts, and executes them. The full AngelScript library is embedded in `Main/AngelScript/`.
- **C++ feature exploration**: Templates, SFINAE, member function pointers, structured bindings (`Main/CPPFeature/`).
- **Dynamic library loading**: Loads `Transfer.dll` at runtime via `LoadLibraryA`/`GetProcAddress` (Windows API).
- **Algorithm challenges**: Jump game, recursion patterns (`Main/Test/`).

### Algorithm (Shared DLL)
`Algorithm/` — A shared library for algorithm implementations. Public API is exposed through `Algorithm/ModulePublic/` headers. Use `ALGOMODULE` macro (expands to `__declspec(dllexport/dllimport)`).

### Transfer (Shared DLL)
`Transfer/src/` — Implements a traits-based type-safe dispatch system. Key design:
- `TransferBase` — abstract base providing virtual dispatch
- `ChildTransfer<T>` — template specialization for concrete types
- `TraitsType<T>` — template trait for safe downcasting
- `BaseObject`/`ChildObject` — objects that participate in transfer dispatch

**Transfer dispatch flow:**
1. `object->redirectTransfer(transfer)` 
2. Object calls `Transfer()` template method on transfer
3. Object calls `TransferDispatch()` using `TraitsType` for safe downcast

Use `TRANSFERMODULE` macro for DLL export/import.

## Key Conventions

- **DLL macros**: Each module defines its own export macro (`ALGOMODULE`, `TRANSFERMODULE`) in its `Module.h`. When building the DLL, the macro expands to `__declspec(dllexport)`; when consuming, `__declspec(dllimport)`.
- **Public vs. Private headers**: Algorithm and Transfer separate public API (`ModulePublic/`) from private implementation (`ModulePrivate/`). Only `ModulePublic/` is added to include paths for consumers.
- **C++20 required**: The root `CMakeLists.txt` sets `CMAKE_CXX_STANDARD 20` globally.
- **Windows-only**: Uses `Windows.h`, MSVC-specific pragmas, and `__declspec`. `NOMINMAX` is defined to avoid min/max macro conflicts.
- **Comments in Chinese**: Much of the codebase is commented in Chinese.
