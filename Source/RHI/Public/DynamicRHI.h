#pragma once
#include "AnyTaskThread.h"
#include "RHIModule.h"
#include "RHIResources.h"// FRHIBuffer / FRHITexture / 各 Desc / TRefCountPtr
// UE: class FDynamicRHI（DynamicRHI.h）—— 纯虚接口，签名里只有 FRHI 基类型，无任何 D3D12 类型
// 总之：接口(RHI)/实现(D3D12RHI)分离 + 全局分发 + 只返回 FRHI 基类型。
// 阶段 A：资源/状态/视口创建及即时命令入口；平台启动时注入后端。

class IRHICommandContext;
class FRHICommandListImmediate;

class RHIMODULE FDynamicRHI
{
public:
	virtual ~FDynamicRHI() = default;
	virtual void Init() = 0;
	virtual void Shutdown() = 0;
    // 供 RHI 内部初始化命令列表；上层通过 Executor 获取 ImmediateCommandList。
    virtual IRHICommandContext* RHIGetDefaultContext() = 0;
    virtual void RHIEndDrawingViewport(FRHICommandListImmediate& RHICmdList,
        FRHIViewport* Viewport, const FRHIPresentArgs& PresentArgs) = 0;
	virtual const char* GetName() = 0;
	virtual TRefCountPtr<FRHIBuffer> RHICreateBuffer(const FRHIBufferDesc& Desc, const void* InitialData = nullptr) = 0;
	/*
	 * RHICreateTexture
	    → FD3D12DynamicRHI
	    → FD3D12Device::CreateTexture
	        ├─ ShaderResource：现有 RGBA8 上传路径
	        └─ DepthStencilTargetable：D32 深度创建路径
	 */
	virtual TRefCountPtr<FRHITexture> RHICreateTexture(const FRHITextureDesc& Desc, const void* InitialData = nullptr) = 0;
	virtual TRefCountPtr<FRHIVertexShader> RHICreateVertexShader(const FRHICreateShaderDesc& CreateShaderDesc) = 0;
	virtual TRefCountPtr<FRHIPixelShader> RHICreatePixelShader(const FRHICreateShaderDesc& CreateShaderDesc) = 0;
	virtual TRefCountPtr<FRHIVertexDeclaration> RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements) = 0;
	virtual TRefCountPtr<FRHIRasterizerState> RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer) = 0;
	virtual TRefCountPtr<FRHIDepthStencilState> RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer) = 0;
	virtual TRefCountPtr<FRHIBlendState> RHICreateBlendState(const FBlendStateInitializerRHI& Initializer) = 0;
	virtual TRefCountPtr<FRHIGraphicsPipelineState> RHICreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer) = 0;
	virtual TRefCountPtr<FRHIShaderResourceView> RHICreateShaderResourceView(TRefCountPtr<FRHIViewableResource> Resource, const FRHIViewDesc& ViewDesc) = 0;
	virtual TRefCountPtr<FRHITexture> RHIGetViewportBackBuffer(FRHIViewport* Viewport) = 0;
	//我们沿用 UE 的入口参数，暂时只支持窗口模式、RGBA8、两个 backbuffer。
	virtual TRefCountPtr<FRHIViewport> RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PixelFormat) = 0;
};
// UE: extern RHI_API FDynamicRHI* GDynamicRHI; —— 全局分发入口，上层只认它

extern RHIMODULE FDynamicRHI* GDynamicRHI;

// UE 同名自由函数：上层调这个，内部转发给 GDynamicRHI
inline TRefCountPtr<FRHIBuffer> RHICreateBuffer(const FRHIBufferDesc& Desc, const void* InitialData = nullptr)
{
	return GDynamicRHI->RHICreateBuffer(Desc, InitialData);
}

inline TRefCountPtr<FRHITexture> RHICreateTexture(const FRHITextureDesc& Desc, const void* InitialData = nullptr)
{
	return GDynamicRHI->RHICreateTexture(Desc, InitialData);
}

inline TRefCountPtr<FRHIVertexShader> RHICreateVertexShader(const FRHICreateShaderDesc& CreateShaderDesc)
{
	return GDynamicRHI->RHICreateVertexShader(CreateShaderDesc);
}

inline TRefCountPtr<FRHIPixelShader> RHICreatePixelShader(const FRHICreateShaderDesc& CreateShaderDesc)
{
	return GDynamicRHI->RHICreatePixelShader(CreateShaderDesc);
}


inline TRefCountPtr<FRHIVertexDeclaration> RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	return GDynamicRHI->RHICreateVertexDeclaration(Elements);
}


inline TRefCountPtr<FRHIRasterizerState> RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
	return GDynamicRHI->RHICreateRasterizerState(Initializer);
}

inline TRefCountPtr<FRHIDepthStencilState> RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
{
	return GDynamicRHI->RHICreateDepthStencilState(Initializer);
};


inline TRefCountPtr<FRHIBlendState> RHICreateBlendState(const FBlendStateInitializerRHI& Initializer)
{
	return GDynamicRHI->RHICreateBlendState(Initializer);
}


inline TRefCountPtr<FRHIGraphicsPipelineState> RHICreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer)
{
	return GDynamicRHI->RHICreateGraphicsPipelineState(Initializer);
}
inline TRefCountPtr<FRHIShaderResourceView> RHICreateShaderResourceView(TRefCountPtr<FRHIViewableResource> Resource, const FRHIViewDesc& ViewDesc)
{
	return GDynamicRHI->RHICreateShaderResourceView(std::move(Resource), ViewDesc);
}

inline TRefCountPtr<FRHITexture> RHIGetViewportBackBuffer(FRHIViewport* Viewport)
{
	return GDynamicRHI->RHIGetViewportBackBuffer(Viewport);
}

inline TRefCountPtr<FRHIViewport> RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PixelFormat)
{
	return GDynamicRHI->RHICreateViewport(
		WindowHandle,
		SizeX,
		SizeY,
		bIsFullscreen,
		PixelFormat);
}

// 教学版启动注入：平台引导层选择后端，RHI 本身不包含 D3D12 头文件。
RHIMODULE void RHIInit(std::unique_ptr<FDynamicRHI> Backend);
// 前提：上层资源已经释放；内部先等待队列完成再销毁后端。
RHIMODULE void RHIExit();
