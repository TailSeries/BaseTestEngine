#pragma once
#include "D3D12RHIModule.h"
#include "D3D12RHIPrivate.h"

// 运行时编译 HLSL → 字节码 blob。
// UE 是离线编译系统（ShaderCompilerWorker/DXC），我们学习阶段用 D3DCompile 直接运行时编译。
// Target 形如 "vs_5_0" / "ps_5_0"；失败返回 nullptr 并打印错误。
using Microsoft::WRL::ComPtr;

D3D12RHIMODULE ComPtr<ID3DBlob> CompileShader(const char* source, const char* EntryPoint, const char* Target);
