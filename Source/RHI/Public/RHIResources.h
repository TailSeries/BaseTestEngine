#pragma once
#include <span>
#include "RHI.h"
#include "RHIModule.h"
#include "RHIDefinitions.h"
#include <stdexcept>
#include <utility>
#include <array>
// UE: class FRHIResource（RHIResources.h）——所有 RHI 资源的基类
/*UE 的结构:
 *FRHIResource
	├─ FRHIViewableResource
	│    ├─ FRHIBuffer
	│    └─ FRHITexture
	│
	└─ FRHIView
		 ├─ FRHIShaderResourceView
		 └─ FRHIUnorderedAccessView
 * 其中，FRHIView 持有一个 TRefCountPtr<FRHIViewableResource>。这意味着同一个 View 接口可以引用 Buffer，也可以引用 Texture.两者的区别是：
 *  · 资源：实际存储的数据，例如一张纹理的像素。
	· View：如何访问这些数据，例如将纹理的某个 mip 范围作为 Shader 输入。
 */

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
		:Size(InSize), Stride(InStride), Usage(InUsage) {}
};

// Buffer 和 Texture 的共同资源基类。
// UE 还在这里保存访问状态、名称等信息，后续逐步补齐。
class RHIMODULE FRHIViewableResource :public FRHIResource
{
protected:
	explicit FRHIViewableResource(ERHIResourceType InResourceType)
		:FRHIResource(InResourceType)
	{}
};


/*
 * - RHI 层:FRHIBuffer : FRHIViewableResource : FRHIResource,持 FRHIBufferDesc { Size, Stride, Usage },构造收 FRHIBufferCreateDesc
 * - D3D12 层:FD3D12Buffer : FRHIBuffer + FD3D12BaseShaderResource,通过 FD3D12ResourceLocation 持有 FD3D12Resource(支持子分配/池化)
 * */
class RHIMODULE FRHIBuffer :public FRHIViewableResource
{
public:
	explicit FRHIBuffer(const FRHIBufferDesc& InDesc)
		:FRHIViewableResource(ERHIResourceType::RRT_Buffer)
		, Desc(InDesc)
	{}
	const FRHIBufferDesc& GetDesc()  const { return Desc; }
	uint32                GetSize()  const { return Desc.Size; }
	uint32                GetStride()const { return Desc.Stride; }
	EBufferUsageFlags     GetUsage() const { return Desc.Usage; }

private:
	FRHIBufferDesc Desc;
};


enum class EClearBinding
{
	ENoneBound,
	EColorBound,
	EDepthStencilBound,
};

//当前没有接入 FLinearColor，因此颜色暂用 std::array<float, 4>，构造器和返回类型与 UE 有这一处简化。
struct FClearValueBinding
{
public:
	// 对齐 UE：默认绑定透明黑。
	FClearValueBinding() = default;

	explicit FClearValueBinding(EClearBinding InBinding)
		: ColorBinding(InBinding)
	{
		if (InBinding != EClearBinding::ENoneBound)
		{
			throw std::invalid_argument(
				"Use a color or depth constructor for a bound clear value");
		}
	}

	explicit FClearValueBinding(
		const std::array<float, 4>& InClearColor)
		: ColorBinding(EClearBinding::EColorBound)
		, Color(InClearColor)
	{}

	explicit FClearValueBinding(
		float DepthClearValue,
		uint32 StencilClearValue = 0)
		: ColorBinding(EClearBinding::EDepthStencilBound)
		, Depth(DepthClearValue)
		, Stencil(StencilClearValue)
	{
		if (!(Depth >= 0.0f && Depth <= 1.0f) ||
			Stencil > 255)
		{
			throw std::invalid_argument(
				"Invalid depth/stencil clear value");
		}
	}

	std::array<float, 4> GetClearColor() const
	{
		if (ColorBinding != EClearBinding::EColorBound)
		{
			throw std::logic_error("No color clear value is bound");
		}

		return Color;
	}

	void GetDepthStencil(
		float& OutDepth,
		uint32& OutStencil) const
	{
		if (ColorBinding != EClearBinding::EDepthStencilBound)
		{
			throw std::logic_error(
				"No depth/stencil clear value is bound");
		}

		OutDepth = Depth;
		OutStencil = Stencil;
	}

private:
	EClearBinding ColorBinding = EClearBinding::EColorBound;

	// 暂不使用 UE 的 union 压缩存储。
	std::array<float, 4> Color{};
	float Depth = 0.0f;
	uint32 Stencil = 0;
};

// UE: struct FRHITextureDesc / FRHITextureCreateDesc（RHIResources.h）
// 当前仅支持 2D、单 mip、单层；ClearValue 记录附件清除值，单独保存它不会执行清除。
struct FRHITextureDesc
{
	uint32 Width = 1;
	uint32 Height = 1;
	EPixelFormat Format = PF_Unknown;
	FRHITextureDesc() = default;
	FRHITextureDesc(uint32 InW, uint32 InH, EPixelFormat InFmt)
		: Width(InW), Height(InH), Format(InFmt) {}

	FClearValueBinding ClearValue{ EClearBinding::ENoneBound };
	ETextureCreateFlags Flags = ETextureCreateFlags::None;
};

// UE: class FRHITexture : FRHIViewableResource : FRHIResource
class RHIMODULE FRHITexture :public FRHIViewableResource
{
public:
	explicit FRHITexture(const FRHITextureDesc& InDesc)
		:FRHIViewableResource(RRT_Texture), Desc(InDesc)
	{

	}

	const FRHITextureDesc& GetDesc() const { return Desc; }
private:
	FRHITextureDesc Desc;
};


// 当前只支持普通 Texture2D SRV。
// UE 的完整版本还包含 Buffer SRV/UAV、Texture UAV、数组和 Plane 等。
struct FRHIViewDesc
{
	/*
	 * 目前我们保留 Texture.SRV、Format、Dimension、MipRange 这些结构关系。当前资源只有单 mip，因此实际支持范围仍然是 First=0、Num=1。
	 */
	enum class EViewType :uint8
	{
		TextureSRV,
	};
	enum class EDimension :uint8
	{
		Texture2D,
	};

	struct FCommon
	{
		EViewType ViewType = EViewType::TextureSRV;
		EPixelFormat Format = PF_Unknown;
	};

	struct FTexture :public FCommon
	{
		EDimension Dimension = EDimension::Texture2D;
		struct FMipRange
		{
			uint8 First = 0;
			uint8 Num = 1;
		};

		FMipRange MipRange{};
	};

	struct FTextureSRV :public FTexture
	{
		struct FInitializer;
	};

	// 暂时只有一个 View 类型，不需要 UE 的 union 存储。
	struct FTextureViews
	{
		FTextureSRV SRV{};
	};

	FTextureViews Texture{};

	static FTextureSRV::FInitializer CreateTextureSRV();

	bool IsSRV() const
	{
		return Texture.SRV.ViewType == EViewType::TextureSRV;
	}

	bool IsTexture() const
	{
		return Texture.SRV.ViewType == EViewType::TextureSRV;
	}
};

struct FRHIViewDesc::FTextureSRV::FInitializer : private FRHIViewDesc
{
	friend struct FRHIViewDesc;

private:
	FInitializer() = default;

public:
	FInitializer& SetFormat(EPixelFormat InFormat)
	{
		Texture.SRV.Format = InFormat;
		return *this;
	}

	FInitializer& SetMipRange(uint8 InFirstMip, uint8 InNumMips)
	{
		Texture.SRV.MipRange.First = InFirstMip;
		Texture.SRV.MipRange.Num = InNumMips;
		return *this;
	}

	// 教学版显式转换入口。
	// UE 由友元 FRHICommandListBase 访问私有基类，
	// 我们尚未建立那一层，暂时通过 Build 返回描述副本。
	/*这里的 Build() 是明确的临时适配，不是 UE 原接口。使用形式是：
	 * const FRHIViewDesc ViewDesc =
		FRHIViewDesc::CreateTextureSRV()
		.SetFormat(PF_R8G8B8A8_UNORM)
		.SetMipRange(0, 1)
		.Build();
	 */
	FRHIViewDesc Build() const
	{
		return static_cast<const FRHIViewDesc&>(*this);
	}
};

inline FRHIViewDesc::FTextureSRV::FInitializer FRHIViewDesc::CreateTextureSRV()
{
	return FTextureSRV::FInitializer{};
}

//View 活着，就会保持它引用的资源存活；但 GPU 是否使用完毕，仍然需要 Fence 保证。 以后回收 SRV 时，既要考虑 View 引用的资源，也要考虑描述符槽位的使用期限
class RHIMODULE FRHIView :public FRHIResource
{
public:
	FRHIViewableResource* GetResource() const
	{
		return Resource.get();
	}

	const FRHIViewDesc& GetDesc() const
	{
		return ViewDesc;
	}
	bool IsTexture() const
	{
		return ViewDesc.IsTexture();
	}

	FRHITexture* GetTexture() const
	{
		if (!IsTexture() || Resource->GetType() != RRT_Texture)
		{
			throw std::logic_error("View does not reference a texture");
		}

		return static_cast<FRHITexture*>(Resource.get());
	}

protected:
	FRHIView(ERHIResourceType InResourceType, TRefCountPtr<FRHIViewableResource> InResource, const FRHIViewDesc& InViewDesc)
		: FRHIResource(InResourceType)
		, Resource(std::move(InResource))
		, ViewDesc(InViewDesc)
	{
		if (!Resource)
		{
			throw std::invalid_argument("View resource must not be null");
		}
	}

private:
	TRefCountPtr<FRHIViewableResource> Resource;

protected:
	const FRHIViewDesc ViewDesc;
};


class RHIMODULE FRHIShaderResourceView :public FRHIView
{
public:
	FRHIShaderResourceView(TRefCountPtr<FRHIViewableResource> InResource, const FRHIViewDesc& InViewDesc)
		:FRHIView(RRT_ShaderResourceView, std::move(InResource), InViewDesc)
	{
		if (!InViewDesc.IsSRV())
		{
			throw std::invalid_argument("Expected an SRV description");
		}

		// 本阶段仅支持 Texture SRV。
		if (!InViewDesc.IsTexture() || GetResource()->GetType() != RRT_Texture)
		{
			throw std::invalid_argument(
				"Only texture shader resource views are supported");
		}


	}

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
class RHIMODULE FRHIGraphicsPipelineState :public FRHIResource
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

class RHIMODULE FRHIShader :public FRHIResource, public FRHIShaderData
{
public:
	FRHIShader(ERHIResourceType InResourceType, EShaderFrequency InFrequency)
		:FRHIResource(InResourceType), Frequency(InFrequency)
	{

	}

	EShaderFrequency GetFrequency() const { return Frequency; }

private:
	EShaderFrequency Frequency;
};


class RHIMODULE FRHIGraphicsShader :public FRHIShader
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

class RHIMODULE FRHIPixelShader :public FRHIGraphicsShader
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
	FRHIVertexDeclaration() :FRHIResource(RRT_VertexDeclaration)
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
	) :VertexDeclarationRHI(InVertexDeclarationRHI), VertexShaderRHI(InVertexShaderRHI), PixelShaderRHI(InPixelShaderRHI) {};

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



class RHIMODULE FRHIRasterizerState :public FRHIResource
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

class RHIMODULE FRHIDepthStencilState :public  FRHIResource
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

class RHIMODULE FRHIBlendState :public FRHIResource
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


enum class ERenderTargetActions : uint8
{
	// 与 UE 保持一致：这里的 2 实际表示 Load 的移位数。
	/*
	 * 低 2 位：Store
	 * 再高 2 位：Load
	 */
	LoadOpMask = 2,

#define RTACTION_MAKE_MASK(Load, Store) \
    ((static_cast<uint8>(ERenderTargetLoadAction::Load) << 2) | \
     static_cast<uint8>(ERenderTargetStoreAction::Store))

	DontLoad_DontStore =
	RTACTION_MAKE_MASK(ENoAction, ENoAction),

	DontLoad_Store =
	RTACTION_MAKE_MASK(ENoAction, EStore),

	Clear_Store =
	RTACTION_MAKE_MASK(EClear, EStore),

	Load_Store =
	RTACTION_MAKE_MASK(ELoad, EStore),

	Clear_DontStore =
	RTACTION_MAKE_MASK(EClear, ENoAction),

	Load_DontStore =
	RTACTION_MAKE_MASK(ELoad, ENoAction),

	Clear_Resolve =
	RTACTION_MAKE_MASK(EClear, EMultisampleResolve),

	Load_Resolve =
	RTACTION_MAKE_MASK(ELoad, EMultisampleResolve),

#undef RTACTION_MAKE_MASK
};

inline constexpr ERenderTargetActions MakeRenderTargetActions(ERenderTargetLoadAction Load, ERenderTargetStoreAction Store)
{
	return static_cast<ERenderTargetActions>(
		(static_cast<uint8>(Load) <<
			static_cast<uint8>(ERenderTargetActions::LoadOpMask)) |
		static_cast<uint8>(Store));
}

inline constexpr ERenderTargetLoadAction GetLoadAction(ERenderTargetActions Action)
{
	return static_cast<ERenderTargetLoadAction>(
		static_cast<uint8>(Action) >>
		static_cast<uint8>(ERenderTargetActions::LoadOpMask));
}

inline constexpr ERenderTargetStoreAction GetStoreAction(ERenderTargetActions Action)
{
	constexpr uint8 StoreMask =
		(1u << static_cast<uint8>(
			ERenderTargetActions::LoadOpMask)) - 1u;

	return static_cast<ERenderTargetStoreAction>(
		static_cast<uint8>(Action) & StoreMask);
}


enum class EDepthStencilTargetActions : uint8
{
	DepthMask = 4,

#define RTACTION_MAKE_MASK(Depth, Stencil) \
    ((static_cast<uint8>(ERenderTargetActions::Depth) << 4) | \
     static_cast<uint8>(ERenderTargetActions::Stencil))

	DontLoad_DontStore =
	RTACTION_MAKE_MASK(
		DontLoad_DontStore,
		DontLoad_DontStore),

	ClearDepthStencil_StoreDepthStencil =
	RTACTION_MAKE_MASK(
		Clear_Store,
		Clear_Store),

	LoadDepthStencil_StoreDepthStencil =
	RTACTION_MAKE_MASK(
		Load_Store,
		Load_Store),

	LoadDepthNotStencil_StoreDepthNotStencil =
	RTACTION_MAKE_MASK(
		Load_Store,
		DontLoad_DontStore),

	ClearDepthStencil_StoreDepthNotStencil =
	RTACTION_MAKE_MASK(
		Clear_Store,
		Clear_DontStore),

#undef RTACTION_MAKE_MASK
};


/*
 * 一张深度模板附件可能同时包含 Depth 和 Stencil，两部分可以采用不同动作。因此 UE 再把两个 ERenderTargetActions 打包：
 * 一个 ERenderTargetActions：4 位
    高 2 位 Load，低 2 位 Store
 * 一个 EDepthStencilTargetActions：8 位
    高 4 位 Depth，低 4 位 Stencil
 */
//这里先保留 UE 的部分常用命名，其他组合可以通过 MakeDepthStencilTargetActions() 表达
inline constexpr EDepthStencilTargetActions MakeDepthStencilTargetActions(ERenderTargetActions Depth, ERenderTargetActions Stencil)
{
	return static_cast<EDepthStencilTargetActions>(
		(static_cast<uint8>(Depth) <<
			static_cast<uint8>(EDepthStencilTargetActions::DepthMask)) |
		static_cast<uint8>(Stencil));
}

inline constexpr ERenderTargetActions GetDepthActions(EDepthStencilTargetActions Action)
{
	return static_cast<ERenderTargetActions>(
		static_cast<uint8>(Action) >>
		static_cast<uint8>(EDepthStencilTargetActions::DepthMask));
}

inline constexpr ERenderTargetActions GetStencilActions(EDepthStencilTargetActions Action)
{
	constexpr uint8 StencilMask =
		(1u << static_cast<uint8>(
			EDepthStencilTargetActions::DepthMask)) - 1u;

	return static_cast<ERenderTargetActions>(
		static_cast<uint8>(Action) & StencilMask);
}



/*
 *理解FRHIRenderPassInfo与 PSO 的区别:
	FGraphicsPipelineStateInitializer	用哪些 Shader、顶点布局和固定状态？要求什么附件格式？
	FRHIRenderPassInfo	这次实际绑定哪几张纹理？开始时清空还是保留？结束后是否保留？
 *
 */
struct FRHIRenderPassInfo
{

	struct FColorEntry
	{
		FRHITexture* RenderTarget = nullptr;
		FRHITexture* ResolveTarget = nullptr;
		int32 ArraySlice = -1;
		uint8 MipIndex = 0;

		ERenderTargetActions Action = ERenderTargetActions::DontLoad_DontStore;
	};
	std::array<FColorEntry, MaxSimultaneousRenderTargets> ColorRenderTargets{};



	struct FDepthStencilEntry
	{
		FRHITexture* DepthStencilTarget = nullptr;
		FRHITexture* ResolveTarget = nullptr;

		EDepthStencilTargetActions Action = EDepthStencilTargetActions::DontLoad_DontStore;

		// 后续补 UE 的 FExclusiveDepthStencil，
		// 表达 Depth / Stencil 的读写访问模式。
	};

	FDepthStencilEntry DepthStencilRenderTarget{};
	FRHIRenderPassInfo() = default;
};

//UE 还有获取原生窗口、交换链等接口，我们当前不需要，先不添加。
class RHIMODULE FRHIViewport : public FRHIResource
{
public:
	FRHIViewport()
		: FRHIResource(RRT_Viewport)
	{}
};

// UE 同名呈现参数；当前只解释 Present 和 VSync，帧号留作诊断。
struct FRHIPresentArgs
{
    FRHIPresentArgs(uint64 InFrameCounter, bool bInPresent, bool bInLockToVsync)
        : FrameCounter(InFrameCounter), bPresent(bInPresent), bLockToVsync(bInLockToVsync) {}
    uint64 FrameCounter;
    bool bPresent;
    bool bLockToVsync;
};
