# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**BaseTestEngine** is a personal **engine-rendering research** project (Windows C++20). The author works in an Unreal Engine team; this codebase is the practice ground for learning DirectX 12 and real-time rendering — each feature is mapped back against UE's engine source to understand "why" behind the "how". The `DirectX12` and `Main` modules are the primary vehicles for this; `Plan.md` (below) is the roadmap the code tracks.

It consists of five active CMake modules (Transfer is commented out) built with MSVC and Ninja.

**`Source/Plan.md` is the project's learning roadmap** (in Chinese): DX12 basics → Shadow/PBR/Deferred → TAA/AO/post-processing → Mini-renderer integration, running in parallel with UE module comparison and UE material/particle/animation skills. Read it for context on what the rendering code is building toward.

## Build Commands

From the `Source/` directory (paths are relative to it). Ninja builds require running inside a **Visual Studio Developer environment** so `cl.exe` and the MSVC toolchain are on `PATH` (e.g. `VsDevCmd.bat -arch=x64` or the VS terminal).

```bash
# Debug build
cmake -B "../out/build/x64-Debug" -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build "../out/build/x64-Debug"

# Release build (RelWithDebInfo)
cmake -B "../out/build/x64-Release" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "../out/build/x64-Release"
```

Alternative: `Source/CMakeSettings.json` configures the same two configurations (`x64-Debug`, `x64-Release`) for the **Visual Studio 18 2026 Win64** generator — open `Source/` in VS and build from the Configuration Manager. The `out/` directory at repo root holds all build output; the `out/build/x64-Debug/` tree is already generated from a prior configure.

**Output** goes to `../out/bin/` (executables and DLLs). The CMake binary dir is set at repo level (`../out/build/...`), so always configure with `-B` pointing there rather than in-tree.

**Running tests**: There is no automated test framework and no `ctest`/GoogleTest — the docs and `verify()` asserts are exercised by launching `Main.exe` (GUI, `/SUBSYSTEM:WINDOWS`) or `ConsoleMain.exe` (console) from `out/bin/`, and reading their stdout/logs. Any file under `Source/*/Test/` or `Source/*/CPPFeature/` is exercised this way.

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

## Repo-root tooling (outside `Source/`)

- `out/` — build artifacts (`out/build/<config>/` generated trees, `out/bin/` final outputs). Not source; regenerate rather than hand-edit.
- `check-source.ps1` + `_create_task.bat` — a scheduled-task (Windows Task Scheduler) setup that auto-commits `Source/` changes on an interval and warns when the source tree is idle. Not part of the build; treat as dev-environment plumbing.

## Key Conventions

- **DLL macros**: Each module has a `Module.h` (or `CoreModule.h`) defining its export macro. Building the DLL defines the `_LIBRARY` symbol; consumers get `dllimport` automatically. The macros are `COREMODULE`, `ALGOMODULE`, `DXMODULE`, `TRANSFERMODULE`.
- **Public vs. Private headers**: Algorithm and Transfer separate `ModulePublic/` (added to consumer include paths) from `ModulePrivate/` (internal only). Core and DirectX12 expose their full source directory as public.
- **Include path convention**: Executables add `..` to their include path, enabling `#include "Algorithm/ModulePublic/..."` style includes.
- **`Core.h` is the Core umbrella header**: `Core/Core.h` aggregates `CoreModule.h`, `GenericPlatform.h`, `NumericLimit.h`, `GenericPlatformProcess.h`, `BaseDefines.h`.
- **Global defines**: `NOMINMAX`, `SYSTEM_WIN`, `PLATFORM_WIN` set project-wide. `_CRT_SECURE_NO_WARNINGS` set per-executable.
- **`NONCOPYABLE(T)` macro**: Defined in `Core/Base/BaseDefines.h` — deletes copy/move constructors and assignment operators.
- **`verify(expr)`**: Debug-aborting assertion macro in `Core/Base/BaseDefines.h`; logs to stderr in both Debug and Release (Release only logs, does not abort).
- **Comments in Chinese**: Most inline documentation is written in Chinese. Respond/match in kind when editing those files.
