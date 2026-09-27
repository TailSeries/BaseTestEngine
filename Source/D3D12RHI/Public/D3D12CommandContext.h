#pragma once
#include "D3D12RHIModule.h"
#include "RHICommandList.h"
#include "D3D12Queue.h"
#include "GenericPlatform.h"
#include <memory>
#include "D3D12DeferredDeletionQueue.h"

class FD3D12Device;
class FD3D12Queue;
class FD3D12Texture;
class FD3D12DescriptorHeap;
class FD3D12CommandAllocator;
class FD3D12CommandList;
class FD3D12Buffer;
class FD3D12RootSignature;
class FD3D12GraphicsPipelineState;

// UE: class FD3D12CommandContext : public IRHICommandContext
// 精简：单线程即时执行；内部包 FD3D12CommandList + 多帧同步 + CB ring, 后续再填充多线程
class D3D12RHIMODULE FD3D12CommandContext:public IRHICommandContext
{
public:
	FD3D12CommandContext();
	virtual ~FD3D12CommandContext();

    // Device 在自身初始化时创建默认 Context；视口不是 Context 的初始化依赖。
    void Init(FD3D12Device* InDevice);
    bool IsFrameOpen() const { return bFrameOpen; }

	virtual void BeginFrame() override;
	virtual void BeginRenderPass(const FRHIRenderPassInfo& Info, const char* Name) override;
	virtual void SetGraphicsPipelineState(FRHIGraphicsPipelineState* PSO) override;
	//我们暂时保留现有两个教学接口，但调整参数语义：
	virtual void SetShaderConstants(uint32 BufferIndex, const void* Data, uint32 Size) override; //  → VS 的 b0 这里的 0 变成 Shader 资源索引，后端负责查对应的根参数位置。这两个接口仍是过渡接口，尚不是 UE 完整的 UniformBuffer/ShaderParameters API。
	virtual void SetShaderResourceViewParameter(uint32 ResourceIndex, FRHIShaderResourceView* View) override;//   → PS 的 t0 这里的 0 变成 Shader 资源索引，后端负责查对应的根参数位置。这两个接口仍是过渡接口，尚不是 UE 完整的 UniformBuffer/ShaderParameters API。
	virtual void SetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBuffer, UINT32 Offset) override;
	virtual void DrawIndexedPrimitive(FRHIBuffer* IndexBuffer, uint32 IndexCount) override;
	virtual void EndRenderPass() override;
	virtual void EndFrame() override;
	virtual void WaitForGPU() override;
	virtual void DeferredDelete(TRefCountPtr<FRHIResource> Resource) override;
private:
	static constexpr uint32 FrameCount = 2;

	FD3D12Device* Device = nullptr;
	FD3D12Queue* Queue = nullptr;
	FD3D12Texture* CurrentColorTarget = nullptr; // 当前 Pass 借用的附件。
	FD3D12DescriptorHeap* SRVHeap = nullptr;

	std::unique_ptr<FD3D12CommandAllocator> CmdAllocs[FrameCount];
	std::unique_ptr<FD3D12CommandList> CmdList;
	TRefCountPtr<FD3D12Buffer> CBs[FrameCount];// 每帧独立的常量 arena，按 draw 分配 256 字节对齐区域。
	uint64 FrameFenceValue[FrameCount] = {};
	uint32 FrameIndex = 0;
	uint32 Slot = 0; // 当前帧 slot（BeginFrame 更新）

	FD3D12DeferredDeletionQueue DeletionQueue;

	// 已申请释放，但还没有绑定本次提交的完成 Fence。
	std::vector<TRefCountPtr<FRHIResource>> PendingDeletes;

	const FD3D12RootSignature* CurrentRootSignature = nullptr;
	const FD3D12GraphicsPipelineState* CurrentGraphicsPipelineState = nullptr;


	bool bInsideRenderPass = false;
    bool bFrameOpen = false;
    uint32 ConstantOffset = 0;
    void RequireFrame() const;

};
