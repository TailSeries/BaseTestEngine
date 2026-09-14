#pragma once
#include "RHIModule.h"
#include "RHIDefinitions.h"

// UE: class FRHIResource（RHIResources.h）——所有 RHI 资源的基类
// 简化(方案A)：去掉侵入式 AddRef/Release + 延迟删除队列（多线程机制），
//              生命周期交给 TRefCountPtr(= std::shared_ptr)，故析构改 public 虚析构。
//              等实现自己的侵入式 TRefCountPtr 后再补 AddRef/Release 对齐 UE。
class RHIMODULE FRHIResource
{
public:
    explicit FRHIResource(ERHIResourceType InResourceType);
    virtual ~FRHIResource();
    ERHIResourceType GetType() const { return ResourceType; }
private:
    ERHIResourceType ResourceType;
};


// UE: struct FRHIBufferDesc（RHIResources.h）
struct FRHIBufferDesc
{
    uint32 Size = 0;// 字节数
    uint32 Stride = 0;// 单个元素字节数(顶点/索引 用)
    EBufferUsageFlags Usage = EBufferUsageFlags::None;
    FRHIBufferDesc() = default;
    FRHIBufferDesc(uint32 InSize, uint32 InStride, EBufferUsageFlags InUsage)
	    :Size(InSize), Stride(InStride), Usage(InUsage){}
};


/*
 * - RHI 层:FRHIBuffer : FRHIViewableResource : FRHIResource,持 FRHIBufferDesc { Size, Stride, Usage },构造收 FRHIBufferCreateDesc
 * - D3D12 层:FD3D12Buffer : FRHIBuffer + FD3D12BaseShaderResource,通过 FD3D12ResourceLocation 持有 FD3D12Resource(支持子分配/池化)
 * - 我们的简化:FRHIBuffer 直接继承 FRHIResource(跳过 FRHIViewableResource 的 view 跟踪);FD3D12Buffer 直接持一个 FD3D12Resource(committed resource,一 buffer 一资源,跳过 UE 的 FD3D12ResourceLocation 子分配/池化——那是显存分配器的进阶话题)。
 */
class RHIMODULE FRHIBuffer:public FRHIResource
{
public:
    explicit FRHIBuffer(const FRHIBufferDesc& InDesc)
        :FRHIResource(ERHIResourceType::RRT_Buffer)
        , Desc(InDesc)
	{}
    const FRHIBufferDesc& GetDesc()  const { return Desc; }
    uint32                GetSize()  const { return Desc.Size; }
    uint32                GetStride()const { return Desc.Stride; }
    EBufferUsageFlags     GetUsage() const { return Desc.Usage; }
	
private:
    FRHIBufferDesc Desc;
};

// UE: struct FRHITextureDesc / FRHITextureCreateDesc（RHIResources.h）
// 精简：只留 2D 单 mip 单层需要的字段
struct FRHITextureDesc
{
    uint32 Width = 1;
    uint32 Height = 1;
    EPixelFormat Format = PF_Unknown;
    FRHITextureDesc() = default;
    FRHITextureDesc(uint32 InW, uint32 InH, EPixelFormat InFmt)
        : Width(InW), Height(InH), Format(InFmt) {}
};

// UE: class FRHITexture : FRHIViewableResource : FRHIResource
// 精简：直接继承 FRHIResource（跳过 FRHIViewableResource 的 view 跟踪）
class RHIMODULE FRHITexture:public FRHIResource
{
public:
    explicit FRHITexture(const FRHITextureDesc& InDesc)
	    :FRHIResource(RRT_Texture),Desc(InDesc)
    {
	    
    }

    const FRHITextureDesc& GetDesc() const { return Desc; }
private:
    FRHITextureDesc Desc;
};



