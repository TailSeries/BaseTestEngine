#pragma once
#include <span>
#include "RHI.h"
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

/*
 * UE 实际的pipelinestate的关系：
 * FRHIGraphicsPipelineState                 RHI 接口层对象
          ▲
          │ 继承
FD3D12GraphicsPipelineState               D3D12 图形管线对象
          │
          ├─ RootSignature ───────持有─────→ FD3D12RootSignature
          │
          └─ PipelineState ───────持有─────→ FD3D12PipelineState
                                             │
                                             └─ ID3D12PipelineState
 */
class RHIMODULE FRHIGraphicsPipelineState:public FRHIResource
{
public:
    FRHIGraphicsPipelineState()
	    :FRHIResource(RRT_GraphicsPipelineState)
    {
	    
    }
};


/*
 *UE 的 shader 数据分层
 *  FRHIResource       FRHIShaderData
      ▲                  ▲
      └──── FRHIShader ───┘
                 ▲
         FRHIGraphicsShader
                 ▲
          FRHIVertexShader        FD3D12ShaderData
                 ▲                      ▲
                 └── FD3D12VertexShader─┘
 * 
 */

// 对应 UE 的资源绑定元数据层，后续补资源表与静态槽位。
class RHIMODULE FRHIShaderData
{
	
};

class RHIMODULE FRHIShader:public FRHIResource, public FRHIShaderData
{
public:
    FRHIShader(ERHIResourceType InResourceType, EShaderFrequency InFrequency)
	    :FRHIResource(InResourceType),Frequency(InFrequency)
    {
	    
    }

    EShaderFrequency GetFrequency() const { return Frequency; }

private:
    EShaderFrequency Frequency;
};


class RHIMODULE FRHIGraphicsShader:public FRHIShader
{
public:
    FRHIGraphicsShader(ERHIResourceType InResourceType, EShaderFrequency InFrequency)
	    :FRHIShader(InResourceType, InFrequency)
	{}
};

class RHIMODULE FRHIVertexShader :public FRHIGraphicsShader
{
public:
    FRHIVertexShader()
        : FRHIGraphicsShader(RRT_VertexShader, SF_Vertex)
    {}
};

class RHIMODULE FRHIPixelShader:public FRHIGraphicsShader
{
public:
    FRHIPixelShader()
	    :FRHIGraphicsShader(RRT_PixelShader, SF_Pixel)
    {
	    
    }
};

//ResourceType 表示“这是哪种 RHI 资源”，Frequency 表示“这是哪个 Shader 阶段”。看起来有重复，但它们分别服务于资源系统和 Shader 系统，UE 也保留这两个信息。


// UE 同名结构位于 RenderCore/ShaderCore.h。
// 当前尚未建立对应的 Shader 编译产物模块，暂放这里。
// 暂不包含 UsageFlags 和序列化信息。
struct FShaderCodePackedResourceCounts
{
    uint8 NumSamplers = 0;
    uint8 NumSRVs = 0;
    uint8 NumCBs = 0;
    uint8 NumUAVs = 0;
};


// 对应 UE 的 TConstArrayView<uint8>：只查看，不拥有数据。
struct FRHICreateShaderDesc
{
    std::span<const uint8> Code;

    // 教学阶段的显式元数据入口。
    // UE 从 Code 携带的附加数据中解包，不是直接加这个成员。
    FShaderCodePackedResourceCounts ResourceCounts{};

    explicit  FRHICreateShaderDesc(std::span<const uint8> InCode)
	    :Code(InCode)
	{}
};


class RHIMODULE FRHIVertexDeclaration :public FRHIResource
{
public:
    FRHIVertexDeclaration():FRHIResource(RRT_VertexDeclaration)
	{}

    virtual bool GetInitializer(FVertexDeclarationElementList& Init)
    {
        return false;
    }
};


/*UE这里描述的是管线所使用的声明与 Shader 组合
 * FGraphicsPipelineStateInitializer
    └─ BoundShaderState
         ├─ VertexDeclarationRHI
         ├─ VertexShaderRHI
         └─ PixelShaderRHI
 */

struct FBoundShaderStateInput
{
    TRefCountPtr<FRHIVertexDeclaration> VertexDeclarationRHI;
    TRefCountPtr<FRHIVertexShader> VertexShaderRHI;
    TRefCountPtr<FRHIPixelShader> PixelShaderRHI;

    FBoundShaderStateInput() = default;
    FBoundShaderStateInput(
    		const TRefCountPtr<FRHIVertexDeclaration>& InVertexDeclarationRHI,
        const TRefCountPtr<FRHIVertexShader>& InVertexShaderRHI,
        const TRefCountPtr<FRHIPixelShader>& InPixelShaderRHI
    ):VertexDeclarationRHI(InVertexDeclarationRHI),VertexShaderRHI(InVertexShaderRHI),PixelShaderRHI(InPixelShaderRHI){};

    FRHIVertexShader* GetVertexShader() const
    {
        return VertexShaderRHI.get();
    }

    FRHIPixelShader* GetPixelShader() const
    {
        return PixelShaderRHI.get();
    }
    // 和ue不同，我们已经直接使用std::shared_ptr了，所以这里就这里不用再写 AddRefResources()、ReleaseResources()：智能指针复制和析构已经处理 CPU 所有权
};



class RHIMODULE FRHIRasterizerState:public FRHIResource
{
public:
    FRHIRasterizerState()
        : FRHIResource(RRT_RasterizerState)
    {}

    virtual bool GetInitializer(FRasterizerStateInitializerRHI& Init)
    {
        return false;
    }

};

class RHIMODULE FRHIDepthStencilState:public  FRHIResource
{
public:
    FRHIDepthStencilState()
	    :FRHIResource(RRT_DepthStencilState)
	{}

    virtual bool GetInitializer(FDepthStencilStateInitializerRHI& Init)
    {
	    return false;
    }
};

class RHIMODULE FRHIBlendState:public FRHIResource
{
public:
    FRHIBlendState()
	    :FRHIResource(RRT_BlendState)
	{}

    virtual bool GetInitializer(FBlendStateInitializerRHI& Init)
    {
	    return false;
    }
};


//这个类就是 UE 的同名 PSO 创建描述。当前只加入已经具备的部分；
class RHIMODULE FGraphicsPipelineStateInitializer
{
public:
    using TRenderTargetFormats = std::array<EPixelFormat, MaxSimultaneousRenderTargets>;

    FBoundShaderStateInput BoundShaderState;
    TRefCountPtr<FRHIRasterizerState> RasterizerState;
    TRefCountPtr<FRHIDepthStencilState> DepthStencilState;
    TRefCountPtr<FRHIBlendState> BlendState;
    EPrimitiveType PrimitiveType = PT_TriangleList;
    uint32 RenderTargetsEnabled = 0;// 当前使用的颜色附件数量。
    TRenderTargetFormats RenderTargetFormats{};// 数组中的格式默认是值为 0 的 PF_Unknown。
    EPixelFormat DepthStencilTargetFormat = PF_Unknown; // 数组中的格式默认是值为 0 的 PF_Unknown。
    uint16 NumSamples = 1;//NumSamples = 1 表示单采样，不是“无采样”。
};

