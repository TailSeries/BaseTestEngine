#pragma once
#include "D3D12RHIModule.h"
#include "DynamicRHI.h"     // 基类 FDynamicRHI（在 RHI 模块）
#include <memory>

class FD3D12Adapter;
class FD3D12Device;



// UE: class FD3D12DynamicRHI : public FDynamicRHI（D3D12RHI 模块）
// 精简：持有 Adapter，Init 里建三层；资源创建转发到 Device

class D3D12RHIMODULE FD3D12DynamicRHI:public FDynamicRHI
{
public:
	FD3D12DynamicRHI();
	~FD3D12DynamicRHI();
	virtual void Init() override;
	virtual void Shutdown() override;
	virtual const char* GetName() override { return "D3D12"; };
	virtual TRefCountPtr<FRHIBuffer> RHICreateBuffer(const FRHIBufferDesc& Desc, const void* InitialData) override;
	virtual TRefCountPtr<FRHITexture> RHICreateTexture(const FRHITextureDesc& Desc, const void* InitialData) override;

	// 过渡期：Viewport / Queue / CommandList / SRV 仍是具体调用，需要拿 Adapter/Device
	FD3D12Adapter* GetAdapter() const { return Adapter.get(); }
	FD3D12Device* GetDevice()  const { return Device; }

private:
	std::unique_ptr<FD3D12Adapter> Adapter;
	FD3D12Device* Device = nullptr;   // 非拥有，指向 Adapter 内部
};
