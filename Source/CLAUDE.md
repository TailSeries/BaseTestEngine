# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**BaseTestEngine** is a Windows C++20 learning/experimental project for engine development practice. It consists of five active CMake modules (Transfer is commented out) built with MSVC and Ninja.

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

No automated test framework — tests are exercised by running `Main.exe` or `ConsoleMain.exe` directly.

## Architecture

Six CMake targets (Transfer currently commented out in root `CMakeLists.txt`), all output to `../out/bin/`:

### Core (Shared DLL)
Foundation library. Public header is `Core/Core.h`. Key systems:
- **Threading**: `FRunnableThread` / `FRunnable` — create OS threads; `FFakeThread` for single-threaded simulation. Windows implementation in `Threads/Win/`.
- **TaskGraph**: UE-style async task system. `FTaskGraphInterface` (singleton, call `Startup(n)`), `FBaseGraphTask`, `TGraphTask<TTask>`, `FGraphEvent`. Tasks specify `GetDesiredThread()` (an `ENamedThreads::Type` bitmask encoding thread + queue + priority) and implement `DoTask()`. Named threads: `GameThread`, `ActualRenderingThread`, `RHIThread`, `AudioThread`; worker threads via `AnyThread`.
- **Synchronization**: `FRWLock`, `FScopeLock`, `FEvent` / `FEventPool`.
- Use `COREMODULE` macro (controlled by `Core_LIBRARY` define).

### Algorithm (Shared DLL)
Algorithm implementations. Public API in `Algorithm/ModulePublic/`. Use `ALGOMODULE` macro (controlled by `ALGORITHM_LIBRARY`).

### DirectX12 (Shared DLL)
DirectX 12 rendering foundation. Links `Core`, `d3d12`, `dxgi`, `d3dcompiler`, `winmm`. Key class:
- `D3DApp` — singleton base class for DX12 applications. Manages device, swap chain, command queue/allocator/list, RTV/DSV heaps, fence. Subclass and override `Update()` / `Draw()` / `OnResize()` / mouse handlers.
- Use `DXMODULE` macro (controlled by `DirectX12_LIBRARY`).

### Main (Executable, `/SUBSYSTEM:WINDOWS`)
Primary test harness. Links `Algorithm` and `DirectX12`. Entry point at `Main/main.cpp`. Exercises:
- **AngelScript integration**: Full library embedded in `Main/AngelScript/`. Registers C++ functions/types, loads `.as` scripts.
- **C++ feature exploration**: Templates, SFINAE, member function pointers, structured bindings (`Main/CPPFeature/`).
- **Dynamic library loading**: Loads `Transfer.dll` at runtime via `LoadLibraryA`/`GetProcAddress`.
- **Algorithm challenges**: Jump game, recursion patterns (`Main/Test/`).
- Include path `..` allows `#include "Algorithm/ModulePublic/..."` style.

### ConsoleMain (Executable, `/SUBSYSTEM:CONSOLE`)
Secondary console test harness. Links `Algorithm`. Entry point at `ConsoleMain/main.cpp`. Currently exercises graph/BST algorithms. Same `..` include path convention as Main.

### Transfer (Shared DLL — currently disabled)
Traits-based type-safe dispatch system. Re-enable by uncommenting in root `CMakeLists.txt`.
- `TransferBase` — abstract base; `ChildTransfer<T>` — template specialization; `TraitsType<T>` — safe downcast trait; `BaseObject`/`ChildObject` — dispatch participants.
- Dispatch flow: `object->redirectTransfer(transfer)` → `Transfer()` → `TransferDispatch()` via `TraitsType`.
- Use `TRANSFERMODULE` macro.

## Key Conventions

- **DLL macros**: Each module has a `Module.h` (or `CoreModule.h`) defining its export macro. Building the DLL defines the `_LIBRARY` symbol; consumers get `dllimport` automatically.
- **Public vs. Private headers**: Algorithm and Transfer separate `ModulePublic/` (added to consumer include paths) from `ModulePrivate/` (internal only). Core and DirectX12 expose their full source directory as public.
- **Global defines**: `NOMINMAX`, `SYSTEM_WIN`, `PLATFORM_WIN` set project-wide. `_CRT_SECURE_NO_WARNINGS` set per-executable.
- **`NONCOPYABLE(T)` macro**: Defined in `Core/Base/BaseDefines.h` — deletes copy/move constructors and assignment operators.
- **`verify(expr)`**: Debug-aborting assertion macro in `Core/Base/BaseDefines.h`; logs to stderr in both Debug and Release (Release only logs, does not abort).
- **Comments in Chinese**: Most inline documentation is written in Chinese.
