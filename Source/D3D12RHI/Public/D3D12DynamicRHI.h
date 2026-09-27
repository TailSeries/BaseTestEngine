#pragma once
#include "D3D12RHIModule.h"
#include "DynamicRHI.h"     // 基类 FDynamicRHI（在 RHI 模块）
#include <memory>

class FD3D12Adapter;
class FD3D12Device;



// UE: class FD3D12DynamicRHI : public FDynamicRHI（D3D12RHI 模块）
// 精简：持有 Adapter，Init 里建三层；资源创建转发到 Device

class D3D12RHIMODULE FD3D12DynamicRHI :public FDynamicRHI
{
public:
	FD3D12DynamicRHI();
	~FD3D12DynamicRHI();
	virtual void Init() override;
	virtual void Shutdown() override;
    virtual IRHICommandContext* RHIGetDefaultContext() override;
    virtual void RHIEndDrawingViewport(FRHICommandListImmediate& RHICmdList,
        FRHIViewport* Viewport, const FRHIPresentArgs& PresentArgs) override;
	virtual const char* GetName() override { return "D3D12"; };
	virtual TRefCountPtr<FRHIBuffer> RHICreateBuffer(const FRHIBufferDesc& Desc, const void* InitialData) override;
	virtual TRefCountPtr<FRHITexture> RHICreateTexture(const FRHITextureDesc& Desc, const void* InitialData) override;
	virtual TRefCountPtr<FRHIVertexShader> RHICreateVertexShader(const FRHICreateShaderDesc& CreateShaderDesc) override;
	virtual TRefCountPtr<FRHIPixelShader> RHICreatePixelShader(const FRHICreateShaderDesc& CreateShaderDesc) override;
	virtual TRefCountPtr<FRHIVertexDeclaration> RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements) override;
	virtual TRefCountPtr<FRHIRasterizerState> RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer) override;
	virtual TRefCountPtr<FRHIDepthStencilState> RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer) override;
	virtual TRefCountPtr<FRHIBlendState> RHICreateBlendState(const FBlendStateInitializerRHI& Initializer) override;
	virtual TRefCountPtr<FRHIGraphicsPipelineState> RHICreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer) override;
	virtual TRefCountPtr<FRHIShaderResourceView> RHICreateShaderResourceView(TRefCountPtr<FRHIViewableResource> Resource, const FRHIViewDesc& ViewDesc) override;
	virtual TRefCountPtr<FRHITexture> RHIGetViewportBackBuffer(FRHIViewport* Viewport) override;

	virtual TRefCountPtr<FRHIViewport> RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PixelFormat) override;
	// 后端内部及专用验证代码使用；普通绘制代码不获取具体 Device。
	FD3D12Adapter* GetAdapter() const { return Adapter.get(); }
	FD3D12Device* GetDevice()  const { return Device; }

private:
	std::unique_ptr<FD3D12Adapter> Adapter;
	FD3D12Device* Device = nullptr;   // 非拥有，指向 Adapter 内部
};
