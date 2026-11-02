#pragma once
#include "RHIModule.h"
#include "RHIContext.h"
#include "RHIResources.h"  // FRHIBuffer / FRHITexture / FRHIGraphicsPipelineState / uint32

// UE：FRHICommandListBase。
// 命令列表的公共基础设施。
// 当前只保存借用的后端 Context；命令存储和延迟重放以后再接入。
class RHIMODULE FRHICommandListBase
{
protected:
	explicit FRHICommandListBase(IRHICommandContext* InContext)
		: Context(InContext)
	{}

	// 非拥有指针：Context 的生命周期目前仍由后端管理。
	IRHICommandContext* Context = nullptr;
};

// UE：FRHIComputeCommandList。
// 图形和计算共用的命令入口层；Transition 在此转发给后端。
// 当前构造参数沿用项目的单 Context 简化，并非 UE 完整构造接口。
class RHIMODULE FRHIComputeCommandList : public FRHICommandListBase
{
public:
    // 同一 Graphics 管线上的同步 Begin/End；异常时也释放 CPU 转换描述。
    void Transition(std::span<const FRHITransitionInfo> Infos);
    void Transition(const FRHITransitionInfo& Info)
    {
        Transition(std::span<const FRHITransitionInfo>(&Info, 1));
    }
    void CopyTexture(FRHITexture* Source, FRHITexture* Dest)
    {
        Context->RHICopyTexture(Source, Dest);
    }
	explicit FRHIComputeCommandList(IRHICommandContext* InContext)
		: FRHICommandListBase(InContext)
	{}
};


// UE：FRHICommandList。
// 在公共命令能力之上提供图形管线、RenderPass 和 Draw 接口。
// 当前仍直接转发到 Context，尚未实现延迟命令重放。
class RHIMODULE FRHICommandList : public FRHIComputeCommandList
{
public:
	explicit FRHICommandList(IRHICommandContext* InContext)
		: FRHIComputeCommandList(InContext)
	{}
	void BeginFrame()
	{
		Context->BeginFrame();
	}

	void BeginRenderPass(const FRHIRenderPassInfo& Info, const char* Name)
	{
		Context->BeginRenderPass(Info, Name);
	}

	void SetGraphicsPipelineState(FRHIGraphicsPipelineState* PSO)
	{
		Context->SetGraphicsPipelineState(PSO);
	}
	// 当前教学接口：向 VS 的 b0 写入常量。
	void SetShaderConstants(uint32 BufferIndex, const void* Data, uint32 Size)
	{
		Context->SetShaderConstants(BufferIndex, Data, Size);
	}
	// 当前教学接口：绑定 PS 的 t0。
	void SetShaderResourceViewParameter(uint32 ResourceIndex, FRHIShaderResourceView* View)
	{
		Context->SetShaderResourceViewParameter(ResourceIndex, View);
	}

	void SetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBuffer, uint32 Offset = 0)
	{
		Context->SetStreamSource(StreamIndex, VertexBuffer, Offset);
	}

	void DrawIndexedPrimitive(FRHIBuffer* IndexBuffer, uint32 IndexCount)
	{
		Context->DrawIndexedPrimitive(IndexBuffer, IndexCount);
	}

	void EndRenderPass()
	{
		Context->EndRenderPass();
	}
	void EndFrame()
	{
		Context->EndFrame();
	}
	void WaitForGPU()
	{
		Context->WaitForGPU();
	}
	void DeferredDelete(TRefCountPtr<FRHIResource> Resource)
	{
		Context->DeferredDelete(std::move(Resource));
	}

};

// UE 将呈现等即时操作放在 ImmediateCommandList；本阶段不实现延迟命令重放。
class RHIMODULE FRHICommandListImmediate : public FRHICommandList
{
public:
    explicit FRHICommandListImmediate(IRHICommandContext* InContext)
        : FRHICommandList(InContext) {}
    void EndDrawingViewport(FRHIViewport* Viewport, const FRHIPresentArgs& PresentArgs);
};

class RHIMODULE FRHICommandListExecutor
{
public:
    static FRHICommandListImmediate& GetImmediateCommandList();
};
