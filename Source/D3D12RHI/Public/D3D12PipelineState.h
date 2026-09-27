#pragma once
#include <memory>
#include <utility>
#include "D3D12RHIModule.h"
#include "D3D12RHIPrivate.h"
#include "RHIResources.h"          // FRHIGraphicsPipelineState
#include <array>
class FD3D12Device;
class FD3D12RootSignature;
using Microsoft::WRL::ComPtr;

struct FD3D12LowLevelGraphicsPipelineStateDesc
{
	const FD3D12RootSignature* pRootSignature = nullptr;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc{};
};

D3D12RHIMODULE FD3D12LowLevelGraphicsPipelineStateDesc GetLowLevelGraphicsPipelineStateDesc(const FGraphicsPipelineStateInitializer& Initializer, const FD3D12RootSignature* RootSignature);


// 底层原生 PSO，不再继承 FRHIGraphicsPipelineState
class D3D12RHIMODULE FD3D12PipelineState
{
public:
	FD3D12PipelineState(FD3D12Device* InDevice, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& Desc);
	~FD3D12PipelineState();
	ID3D12PipelineState* GetPipelineState() const { return PipelineState.Get(); }
private:

	ComPtr<ID3D12PipelineState> PipelineState;
};


// 对应 UE 的公共关联数据。
// 暂无底层 PSO 缓存：用 TRefCountPtr 表达底层 PSO 所有权，根签名由 Adapter 缓存持有。
struct D3D12RHIMODULE FD3D12PipelineStateCommonData
{
	FD3D12PipelineStateCommonData(const FD3D12RootSignature* InRootSignature, TRefCountPtr<FD3D12PipelineState> InPipelineState)
		:RootSignature(InRootSignature),
		PipelineState(std::move(InPipelineState))
	{}

	const FD3D12RootSignature* const RootSignature;
	TRefCountPtr<FD3D12PipelineState> PipelineState;
};


// RHI 图形管线对象的 D3D12 实现
class D3D12RHIMODULE FD3D12GraphicsPipelineState : public FRHIGraphicsPipelineState, public FD3D12PipelineStateCommonData
{
public:
	FD3D12GraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer, const FD3D12RootSignature* InRootSignature, TRefCountPtr<FD3D12PipelineState> InPipelineState);


	FGraphicsPipelineStateInitializer PipelineStateInitializer;
	std::array<uint16, D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> StreamStrides{};

};
