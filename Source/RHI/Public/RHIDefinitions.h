#pragma once
#include "GenericPlatform.h"
#include "EnumClassFlags.h"

// UE: enum ERHIResourceType : uint8（RHIDefinitions.h，RRT_ 前缀，非 enum class）
// 精简：只留当前用得到的类型，UE 大概有几十种吧，包括各种RayTracing相关的buffer，命名对齐 UE，后续按需扩充
enum ERHIResourceType :uint8
{
    RRT_None = 0,
    RRT_Buffer,
    RRT_Texture,
    RRT_VertexShader,
    RRT_PixelShader,
    RRT_GraphicsPipelineState,
    RRT_VertexDeclaration,
    RRT_RasterizerState,
    RRT_DepthStencilState,
    RRT_BlendState,
    RRT_ShaderResourceView,//用来标识SRV 对象的类型，与其引用的 RRT_Texture 是两个不同对象。
    RRT_Viewport,
};

//实心或线框
enum ERasterizerFillMode : uint8
{
    FM_Wireframe,
    FM_Solid,
};

//剔除哪种顶点绕序的三角形
enum ERasterizerCullMode : uint8
{
    CM_None,
    CM_CW,
    CM_CCW,
};

enum class ERasterizerDepthClipMode : uint8
{
    DepthClip,//越过远平面的部分被切掉，范围内的部分继续绘制。
    DepthClamp,//越过部分仍可生成像素，它们的深度被钳制到远端边界。能否显示仍取决于深度测试等条件。(depth = clamp(depth, 0.0f, 1.0f);)
};

enum EVertexElementType : uint8
{
    VET_None = 0,
    VET_Float1,
    VET_Float2,
    VET_Float3,
    VET_Float4,
    VET_UInt,
    VET_UByte4,
    VET_UByte4N,
    VET_Short2,
    VET_Short2N,
};

//
enum ECompareFunction : uint8
{
    CF_Less,
    CF_LessEqual,
    CF_Greater,
    CF_GreaterEqual,
    CF_Equal,
    CF_NotEqual,
    CF_Never,
    CF_Always,
};

// UE: enum class EBufferUsageFlags : uint32（RHIDefinitions.h，位标志）
// 精简：只留当前用得到的几个, 注意这个结构体比较过分，它不仅是堆类型的标志，甚至还是 哪种类型的标志
enum class EBufferUsageFlags : uint32
{
    None = 0,
    Static = 1 << 0,   // 内容基本不变，GPU 读为主（DEFAULT 堆）
    Dynamic = 1 << 1,   // CPU 频繁写（UPLOAD 堆，Map+memcpy）
    VertexBuffer = 1 << 2,
    IndexBuffer = 1 << 3,
    ConstantBuffer = 1 << 4,
};
// UE: enum EPixelFormat（RHIDefinitions.h，PF_ 前缀，几十种）
// 精简：暂时只保留一些现在用得到的
enum EPixelFormat :uint8
{
    PF_Unknown = 0,
    PF_R8G8B8A8_UNORM,   // 普通彩色贴图
    PF_D32_FLOAT,        // 深度（把 CreateDepthBuffer 的硬编码也能收编到这）
};

ENUM_CLASS_FLAGS(EBufferUsageFlags);



// Shader 所属的管线阶段；Frequency 在这里不是执行频率。
enum EShaderFrequency :uint8
{
    SF_Vertex = 0,
    SF_Pixel,
};



enum EBlendOperation : uint8
{
    BO_Add,
    BO_Subtract,
    BO_Min,
    BO_Max,
    BO_ReverseSubtract,
};

// 先实现常用子集，名字沿用 UE。
enum EBlendFactor : uint8
{
    BF_Zero,
    BF_One,
    BF_SourceAlpha,
    BF_InverseSourceAlpha,
};

enum EColorWriteMask : uint8
{
    CW_NONE = 0,
    CW_RED = 0x01,
    CW_GREEN = 0x02,
    CW_BLUE = 0x04,
    CW_ALPHA = 0x08,

    CW_RGB = CW_RED | CW_GREEN | CW_BLUE,
    CW_RGBA = CW_RGB | CW_ALPHA,
};

inline constexpr uint32 MaxSimultaneousRenderTargets = 8;

enum EPrimitiveType : uint8
{
    PT_TriangleList,
};


//Load 不是“从磁盘加载纹理”，而是保留这个附件进入 Pass 前已有的内容。
enum class ERenderTargetLoadAction :uint8
{
    // ENoAction 也不是清零,而是不要求保留旧内容。选择它之后，就不能依赖那些未被重新写入的内容，要认为未被本 Pass 写入的区域内容不确定。
    ENoAction,
    // 保留附件原有内容。
    ELoad,
    // 在 Pass 开始时清空附件。
    EClear,
    Num,
    NumBits = 2,
};

enum class ERenderTargetStoreAction : uint8
{
    // Pass 结束后，不要求保留附件内容。
    ENoAction,

    // 保留结果，供后续使用。
    EStore,

    // 将多重采样结果 Resolve 到目标。
    // 先保留 UE 的枚举，当前后端尚不支持。
    EMultisampleResolve,

    Num,
    NumBits = 2,
};
static_assert(
    static_cast<uint32>(ERenderTargetLoadAction::Num) <=
    (1u << static_cast<uint32>(ERenderTargetLoadAction::NumBits)));

static_assert(
    static_cast<uint32>(ERenderTargetStoreAction::Num) <=
    (1u << static_cast<uint32>(ERenderTargetStoreAction::NumBits)));


enum class ETextureCreateFlags : uint64
{
    None = 0,

    RenderTargetable = 1ull << 0, // RenderTargetable 本步用于标记已有 backbuffer，暂不支持创建普通离屏颜色附件
    DepthStencilTargetable = 1ull << 2,
    ShaderResource = 1ull << 3,
};

ENUM_CLASS_FLAGS(ETextureCreateFlags)
