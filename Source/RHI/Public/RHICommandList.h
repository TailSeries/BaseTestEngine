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
		  BeginRenderPass     绑 RT + 清屏
		    SetGraphicsPipelineState   先设管线(rootsig/堆/PSO)——必须在绑 CBV/表之前
		    SetShaderConstants         传矩阵(root CBV)
		    SetTexture                 绑纹理(root 表)——必须在 SetDescriptorHeaps 之后
		    SetStreamSource            绑顶点
		    DrawIndexedPrimitive       画
		  EndRenderPass       RT→PRESENT
		EndFrame              提交 + 上屏
	 */
public:
	virtual ~IRHICommandContext() = default;
	//开始录这一帧:选 slot → 等这个 slot 上轮 GPU 干完 → Reset 当帧 allocator + 命令列表,(WaitCPU + CmdAlloc->Reset + CmdList->Reset)
	virtual void BeginFrame() = 0;

	//开一趟渲染:backbuffer 从 PRESENT→RENDER_TARGET,绑 RTV+DSV,清颜色+深度,设视口/裁剪 (barrier + OMSetRenderTargets + Clear* + RSSetViewports/Scissor)
	virtual void BeginRenderPass(const float ClearColor[4]) = 0;

	//设整条管线(shader+光栅+混合+深度+输入布局 打包成一个对象)+ 它的根签名 + 描述符堆 (SetGraphicsRootSignature + SetDescriptorHeaps + SetPipelineState。这就是 D3D12 的招牌 PSO)
	virtual void SetGraphicsPipelineState(FRHIGraphicsPipelineState* PSO) = 0;
	
	//给 shader 传常量(你的 WVP 矩阵):写进当前 slot 的 CB,绑成 root CBV (memcpy CB + SetGraphicsRootConstantBufferView)
	virtual void SetShaderConstants(uint32 RootParam, const void* Data, uint32 Size) = 0;

	//给 shader 绑纹理:找到纹理的 SRV 槽,绑成描述符表 (SetGraphicsRootDescriptorTable)
	virtual void SetTexture(uint32 RootParam, FRHITexture* Texture) = 0;
	// IA 阶段数据设置
	virtual void SetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBuffer) = 0;

	// 绑索引 buffer + 图元拓扑,发出按索引绘制 (建 IBV + IASetIndexBuffer + IASetPrimitiveTopology + DrawIndexedInstanced)
	virtual void DrawIndexedPrimitive(FRHIBuffer* IndexBuffer, uint32 IndexCount) = 0;

	//收一趟:backbuffer 从 RENDER_TARGET→PRESENT(准备上屏)(barrier)
	virtual void EndRenderPass() = 0;

	// 收尾:关命令列表 → 提交给队列 → Present 上屏 → 记 fence 值 → 帧号 +1(Close + ExecuteCommandLists + Present + Signal)
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

	void BeginRenderPass(const float ClearColor[4])
	{
		Context->BeginRenderPass(ClearColor);
	}

	void SetGraphicsPipelineState(FRHIGraphicsPipelineState* PSO)
	{
		Context->SetGraphicsPipelineState(PSO);
	}

	void SetShaderConstants(uint32 RootParam, const void* Data, uint32 Size)
	{
		Context->SetShaderConstants(RootParam, Data, Size);
	}

	void SetTexture(uint32 RootParam, FRHITexture* Texture)
	{
		Context->SetTexture(RootParam, Texture);
	}

	void SetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBuffer)
	{
		Context->SetStreamSource(StreamIndex, VertexBuffer);
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
