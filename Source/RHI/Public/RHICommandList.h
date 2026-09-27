#pragma once
#include "RHIModule.h"
#include "RHIResources.h"  // FRHIBuffer / FRHITexture / FRHIGraphicsPipelineState / uint32

// UE: class IRHICommandContext —— backend 命令接口（D3D12 实现）
// 精简：暂时单线程即时执行；方法参数全 FRHI 类型，后面再拓展对多线程的支持
class RHIMODULE IRHICommandContext
{
	/*
	 * UE 里"RenderPass"是核心概念:一帧可以有多趟(GBuffer pass、光照pass、后处理pass…),每pass绑不同的 RT。我们现在就一pass(画到 backbuffer)。
	 *
	 * draw loop 中的实际调用顺序如下：
	 *	BeginFrame            开始录
		  BeginRenderPass     按附件描述绑定 RT，并按 LoadAction 决定是否清除
			SetGraphicsPipelineState   先设管线(rootsig/堆/PSO)——必须在绑 CBV/表之前
			SetShaderConstants         传矩阵(root CBV)
			SetShaderResourceViewParameter                 绑纹理(root 表)——必须在 SetDescriptorHeaps 之后
			SetStreamSource            绑顶点
			DrawIndexedPrimitive       画
		  EndRenderPass       RT→PRESENT
		EndFrame              提交并记录 Fence
        EndDrawingViewport    独立呈现入口（ImmediateCommandList）
	 */
public:
	virtual ~IRHICommandContext() = default;
	//开始录这一帧:选 slot → 等这个 slot 上轮 GPU 干完 → Reset 当帧 allocator + 命令列表,(WaitCPU + CmdAlloc->Reset + CmdList->Reset)
	virtual void BeginFrame() = 0;

	// 当前支持一个 backbuffer 和 D32 附件；Load_Store 保留内容，Clear_Store 使用纹理 ClearValue 清除。
	//Name 对应 UE 的 Pass 名称；暂时使用 const char*，也暂不实现 GPU 调试标记。
	virtual void BeginRenderPass(const FRHIRenderPassInfo& Info, const char* Name) = 0;

	//设整条管线(shader+光栅+混合+深度+输入布局 打包成一个对象)+ 它的根签名 + 描述符堆 (SetGraphicsRootSignature + SetDescriptorHeaps + SetPipelineState。这就是 D3D12 的招牌 PSO)
	virtual void SetGraphicsPipelineState(FRHIGraphicsPipelineState* PSO) = 0;

	// 当前教学接口：向 VS 的 b0 写入常量。
	virtual void SetShaderConstants(uint32 BufferIndex, const void* Data, uint32 Size) = 0;

	// 当前教学接口：绑定 PS 的 t0。
	virtual void SetShaderResourceViewParameter(uint32 ResourceIndex, FRHIShaderResourceView* View) = 0;
	// IA 阶段数据设置
	virtual void SetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBuffer, uint32 Offset) = 0;

	// 绑索引 buffer + 图元拓扑,发出按索引绘制 (建 IBV + IASetIndexBuffer + IASetPrimitiveTopology + DrawIndexedInstanced)
	virtual void DrawIndexedPrimitive(FRHIBuffer* IndexBuffer, uint32 IndexCount) = 0;

	//收一趟:backbuffer 从 RENDER_TARGET→PRESENT(准备上屏)(barrier)
	virtual void EndRenderPass() = 0;

	// 收尾：关闭并提交命令列表，记录 Fence，推进帧槽；Present 由独立入口负责。
	virtual void EndFrame() = 0;
	virtual void WaitForGPU() = 0;

	// 延迟删除：把资源交给后端保活，等 GPU 用完再真释放
	virtual void DeferredDelete(TRefCountPtr<FRHIResource> Resource) = 0;

};

// UE: class FRHICommandList —— 上层录制 API；单线程下直接转发给 context，
//  将来在 FRHICommandList→context 这条缝里插入"命令缓存 + RHI 线程重放"（deferred）
class RHIMODULE FRHICommandList
{
public:
	explicit FRHICommandList(IRHICommandContext* InContext)
		:Context(InContext)
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


private:
	IRHICommandContext* Context = nullptr;    // 非拥有 context 不由commandlist管理释放

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
