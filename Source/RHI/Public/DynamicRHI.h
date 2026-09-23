#pragma once
#include "RHIModule.h"
#include "RHIResources.h"// FRHIBuffer / FRHITexture / 各 Desc / TRefCountPtr
// UE: class FDynamicRHI（DynamicRHI.h）—— 纯虚接口，签名里只有 FRHI 基类型，无任何 D3D12 类型
// 总之：接口(RHI)/实现(D3D12RHI)分离 + 全局分发 + 只返回 FRHI 基类型。
// 精简：先放资源创建（Buffer/Texture）+ 生命周期；命令列表抽象留 A2-b

class RHIMODULE FDynamicRHI
{
public:
	virtual ~FDynamicRHI() = default;
	virtual void Init() = 0;
	virtual void Shutdown() = 0;
	virtual const char* GetName() = 0;
	virtual TRefCountPtr<FRHIBuffer> RHICreateBuffer(const FRHIBufferDesc& Desc, const void* InitialData = nullptr) = 0;
	virtual TRefCountPtr<FRHITexture> RHICreateTexture(const FRHITextureDesc& Desc, const void* InitialData = nullptr) = 0;
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



