#pragma once
#include "D3D12RHIModule.h"
#include "D3D12RHIPrivate.h"
class FD3D12Device;
// UE: class FD3D12RootSignature : FD3D12AdapterChild（带量化绑定状态缓存，很复杂）
/*
Root Signature对象
绑定槽位映射
Shader反射结果
资源绑定状态缓存
Root Parameter量化
PSO共享 
*/

// 简化：直接从 D3D12_ROOT_SIGNATURE_DESC 序列化 + 创建
using Microsoft::WRL::ComPtr;
class D3D12RHIMODULE FD3D12RootSignature
{
public:
	FD3D12RootSignature(FD3D12Device* InDevice, const D3D12_ROOT_SIGNATURE_DESC& Desc);
	~FD3D12RootSignature();
	ID3D12RootSignature* GetRootSignature() const { return RootSignature.Get(); }
private:
	
	ComPtr<ID3D12RootSignature> RootSignature;
};

