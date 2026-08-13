#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_6.h> //dxgi1_6.h 包含了 IDXGIFactory6（按性能枚举 GPU），向下兼容所有旧版本，用这一个就够
#include <d3dcompiler.h>

#include <wrl/client.h> //wrl/client.h 提供 ComPtr，这是 Windows 官方的智能指针，等价于 UE 的 TRefCountPtr<ID3D12xxx>

#include <cstdint>
#include <cassert>

#include "D3D12RHIModule.h"


#define VERIFY_D3D12(hr) assert(SUCCEEDED(hr)) //VERIFY_D3D12 对应 UE 的 VERIFYD3D12RESULT，现在先用 assert，后续可以改成打 log