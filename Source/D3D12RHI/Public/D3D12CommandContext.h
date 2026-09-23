#pragma once
#include "D3D12RHIModule.h"
#include "RHICommandList.h"
#include "D3D12Queue.h"
#include "GenericPlatform.h"
#include <memory>
#include "D3D12DeferredDeletionQueue.h"

class FD3D12Device;
class FD3D12Queue;
class FD3D12Viewport;
class FD3D12DescriptorHeap;
class FD3D12CommandAllocator;
class FD3D12CommandList;
class FD3D12Buffer;

// UE: class FD3D12CommandContext : public IRHICommandContext
// 精简：单线程即时执行；内部包 FD3D12CommandList + 多帧同步 + CB ring, 后续再填充多线程
class D3D12RHIMODULE FD3D12CommandContext:public IRHICommandContext
{
public:
	FD3D12CommandContext();
	virtual ~FD3D12CommandContext();

	// 一次性接入帧缓冲/描述符基础设施（过渡期具体类型集中在这一处）
	void Init(FD3D12Device* InDevice,
	FD3D12Queue* InQueue,
	FD3D12Viewport* InViewPort,
	FD3D12DescriptorHeap* InDSVHeap,
	FD3D12DescriptorHeap* InSRVHeap
	);

	virtual void BeginFrame() override;
	virtual void BeginRenderPass(const float ClearColor[4]) override;
	virtual void SetGraphicsPipelineState(FRHIGraphicsPipelineState* PSO) override;
	virtual void SetShaderConstants(uint32 RootParam, const void* Data, uint32 Size) override;
	virtual void SetTexture(uint32 RootParam, FRHITexture* Texture) override;
	virtual void SetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBuffer) override;
	virtual void DrawIndexedPrimitive(FRHIBuffer* IndexBuffer, uint32 IndexCount) override;
	virtual void EndRenderPass() override;
	virtual void EndFrame() override;
	virtual void WaitForGPU() override;
	virtual void DeferredDelete(TRefCountPtr<FRHIResource> Resource) override;
private:
	static constexpr uint32 FrameCount = 2;

	FD3D12Device* Device = nullptr;
	FD3D12Queue* Queue = nullptr;
	FD3D12Viewport* Viewport = nullptr;
	FD3D12DescriptorHeap* DSVHeap = nullptr;
	FD3D12DescriptorHeap* SRVHeap = nullptr;

	std::unique_ptr<FD3D12CommandAllocator> CmdAllocs[FrameCount];
	std::unique_ptr<FD3D12CommandList> CmdList;
	TRefCountPtr<FD3D12Buffer> CBs[FrameCount];// // CB ring（context 是 D3D12 层，目前先直接持具体类型）
	uint64 FrameFenceValue[FrameCount] = {};
	uint32 FrameIndex = 0;
	uint32 Slot = 0; // 当前帧 slot（BeginFrame 更新）

	FD3D12DeferredDeletionQueue DeletionQueue;
};
