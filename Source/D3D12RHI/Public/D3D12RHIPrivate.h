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


#include <stdexcept>
#include <string>
// HRESULT 表达式在 Release 中也必须执行；assert 会在 NDEBUG 下消去表达式。
inline void VerifyD3D12Result(HRESULT Result, const char* Expression)
{
    if (FAILED(Result))
        throw std::runtime_error(std::string(Expression) + " failed, HRESULT=" +
            std::to_string(static_cast<unsigned long>(Result)));
}
#define VERIFY_D3D12(Expression) VerifyD3D12Result((Expression), #Expression)
