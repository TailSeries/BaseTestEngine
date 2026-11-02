#pragma once
#include "RHIModule.h"
#include "RHITransition.h"
#include "RHIResources.h"

// UE：IRHIComputeContext 是图形/计算共用命令的后端接口层。
// Compute 是能力分层，不意味着这些命令一定提交到异步计算队列。
class RHIMODULE IRHIComputeContext
{
public:
    virtual ~IRHIComputeContext() = default;
    virtual void RHIBeginTransitions(std::span<const FRHITransition* const> Transitions) = 0;
    virtual void RHIEndTransitions(std::span<const FRHITransition* const> Transitions) = 0;
};

// UE: class IRHICommandContext —— backend 命令接口（D3D12 实现）
// 精简：暂时单线程即时执行；方法参数全 FRHI 类型，后面再拓展对多线程的支持
class RHIMODULE IRHICommandContext : public IRHIComputeContext
{
	/*
	 * UE 里"RenderPass"是核心概念:一帧可以有多趟(GBuffer pass、光照pass、后处理pass…),每pass绑不同的 RT。阶段 B 包含离屏绘制和全屏合成两个绘制 Pass。
	 *
	 * draw loop 中的实际调用顺序如下：
	 *	BeginFrame            开始录
          Transition            声明附件即将使用的访问方式
		  BeginRenderPass     按附件描述绑定 RT，并按 LoadAction 决定是否清除
			SetGraphicsPipelineState   先设管线(rootsig/堆/PSO)——必须在绑 CBV/表之前
			SetShaderConstants         传矩阵(root CBV)
			SetShaderResourceViewParameter                 绑纹理(root 表)——必须在 SetDescriptorHeaps 之后
			SetStreamSource            绑顶点
			DrawIndexedPrimitive       画
		  EndRenderPass       结束附件使用，不转换状态
          Transition            为下个 Pass 或 Present 声明访问
		EndFrame              提交并记录 Fence
        EndDrawingViewport    独立呈现入口（ImmediateCommandList）
	 */
public:
	virtual ~IRHICommandContext() = default;
	//开始录这一帧:选 slot → 等这个 slot 上轮 GPU 干完 → Reset 当帧 allocator + 命令列表,(WaitCPU + CmdAlloc->Reset + CmdList->Reset)
	virtual void BeginFrame() = 0;

	// 当前支持一个 RGBA8 颜色附件（离屏或 backbuffer）和 D32 附件；Load_Store 保留内容，Clear_Store 使用纹理 ClearValue 清除。
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

	// 结束当前 Pass；下一次访问由调用方通过 Transition 显式声明。
	virtual void EndRenderPass() = 0;

	// 收尾：关闭并提交命令列表，记录 Fence，推进帧槽；Present 由独立入口负责。
	virtual void EndFrame() = 0;
	virtual void WaitForGPU() = 0;

	// 延迟删除：把资源交给后端保活，等 GPU 用完再真释放
	virtual void DeferredDelete(TRefCountPtr<FRHIResource> Resource) = 0;
    // 阶段 B：同尺寸同格式整纹理复制，状态须由调用方先声明。
    virtual void RHICopyTexture(FRHITexture* Source, FRHITexture* Dest) = 0;

};
