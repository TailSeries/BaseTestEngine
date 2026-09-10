#pragma once
#include "D3D12RHIModule.h"
#include "D3D12RHIPrivate.h"
class FD3D12Device;
// UE: class FD3D12PipelineState（含 PSO 缓存/异步编译）
// 简化：直接从 D3D12_GRAPHICS_PIPELINE_STATE_DESC 创建
using Microsoft::WRL::ComPtr;
class D3D12RHIMODULE FD3D12PipelineState
{
public:
	FD3D12PipelineState(FD3D12Device* InDevice, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& Desc);
	~FD3D12PipelineState();
	ID3D12PipelineState* GetPipelineState() const { return PSO.Get(); }
private:

	ComPtr<ID3D12PipelineState> PSO;
	
};
