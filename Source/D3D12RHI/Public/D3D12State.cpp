#include "D3D12State.h"
#include "D3D12DynamicRHI.h"
#include "D3D12Adapter.h"
#include "D3D12PipelineState.h"
#include <memory>
#include <utility>
#include <cmath>
#include <stdexcept>
static D3D12_FILL_MODE  TranslateFillMode(ERasterizerFillMode FillMode)
{
    switch (FillMode)
    {
    case FM_Wireframe:
        return D3D12_FILL_MODE_WIREFRAME;
    default:
        return D3D12_FILL_MODE_SOLID;
    }
}


static ERasterizerFillMode ReverseTranslateFillMode(D3D12_FILL_MODE FillMode)
{
    return FillMode == D3D12_FILL_MODE_WIREFRAME
        ? FM_Wireframe
        : FM_Solid;
}

static D3D12_CULL_MODE TranslateCullMode(ERasterizerCullMode CullMode)
{
    switch (CullMode)
    {
    case CM_CW:
        return D3D12_CULL_MODE_BACK;
    case CM_CCW:
        return D3D12_CULL_MODE_FRONT;
    default:
        return D3D12_CULL_MODE_NONE;
    }
}


static ERasterizerCullMode ReverseTranslateCullMode(D3D12_CULL_MODE CullMode)
{
    switch (CullMode)
    {
    case D3D12_CULL_MODE_BACK:
        return CM_CW;
    case D3D12_CULL_MODE_FRONT:
        return CM_CCW;
    default:
        return CM_None;
    }
}

TRefCountPtr<FRHIRasterizerState> FD3D12DynamicRHI::RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
    TRefCountPtr<FD3D12RasterizerState> State = std::make_shared<FD3D12RasterizerState>();
    D3D12_RASTERIZER_DESC& Desc = State->Desc;
    Desc.FillMode = TranslateFillMode(Initializer.FillMode);
    Desc.CullMode = TranslateCullMode(Initializer.CullMode);


    // 与 UE 的 D3D12 后端约定一致。
    Desc.FrontCounterClockwise = true;//逆时针是正面
    Desc.DepthBias = static_cast<INT>(std::floor(Initializer.DepthBias * static_cast<float>(1<<24)));

    Desc.SlopeScaledDepthBias = Initializer.SlopeScaleDepthBias;//随斜率变化的深度偏移系数
    Desc.DepthClipEnable = Initializer.DepthClipMode == ERasterizerDepthClipMode::DepthClip;
    Desc.MultisampleEnable = Initializer.bAllowMSAA;
    return State;
}


bool FD3D12RasterizerState::GetInitializer(FRasterizerStateInitializerRHI& Init)
{
    Init.FillMode = ReverseTranslateFillMode(Desc.FillMode);
    Init.CullMode = ReverseTranslateCullMode(Desc.CullMode);
    Init.DepthBias = Desc.DepthBias / static_cast<float>(1 << 24);
    Init.SlopeScaleDepthBias = Desc.SlopeScaledDepthBias;
    Init.DepthClipMode = Desc.DepthClipEnable
        ? ERasterizerDepthClipMode::DepthClip
        : ERasterizerDepthClipMode::DepthClamp;
    Init.bAllowMSAA = Desc.MultisampleEnable != FALSE;
    return true;
}



static D3D12_COMPARISON_FUNC TranslateCompareFunction(ECompareFunction CompareFunction)
{
    switch (CompareFunction)
    {
    case CF_Less:         return D3D12_COMPARISON_FUNC_LESS;
    case CF_LessEqual:    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case CF_Greater:      return D3D12_COMPARISON_FUNC_GREATER;
    case CF_GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case CF_Equal:        return D3D12_COMPARISON_FUNC_EQUAL;
    case CF_NotEqual:     return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case CF_Never:        return D3D12_COMPARISON_FUNC_NEVER;
    default:             return D3D12_COMPARISON_FUNC_ALWAYS;
    }
}
static ECompareFunction ReverseTranslateCompareFunction(D3D12_COMPARISON_FUNC CompareFunction)
{
    switch (CompareFunction)
    {
    case D3D12_COMPARISON_FUNC_LESS:          return CF_Less;
    case D3D12_COMPARISON_FUNC_LESS_EQUAL:    return CF_LessEqual;
    case D3D12_COMPARISON_FUNC_GREATER:       return CF_Greater;
    case D3D12_COMPARISON_FUNC_GREATER_EQUAL: return CF_GreaterEqual;
    case D3D12_COMPARISON_FUNC_EQUAL:         return CF_Equal;
    case D3D12_COMPARISON_FUNC_NOT_EQUAL:     return CF_NotEqual;
    case D3D12_COMPARISON_FUNC_NEVER:         return CF_Never;
    default:                                return CF_Always;
    }
}

TRefCountPtr<FRHIDepthStencilState> FD3D12DynamicRHI::RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
{
    TRefCountPtr<FD3D12DepthStencilState> State = std::make_unique<FD3D12DepthStencilState>();
    D3D12_DEPTH_STENCIL_DESC& Desc = State->Desc;

    Desc.DepthEnable = Initializer.DepthTest != CF_Always || Initializer.bEnableDepthWrite;

    Desc.DepthWriteMask = Initializer.bEnableDepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL: D3D12_DEPTH_WRITE_MASK_ZERO;
    Desc.DepthFunc = TranslateCompareFunction(Initializer.DepthTest);

    // 当前阶段不启用 Stencil，但仍填写有效的默认操作。
    Desc.StencilEnable = false;
    Desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    Desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

    Desc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    Desc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    Desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    Desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    Desc.BackFace = Desc.FrontFace;

    return State;

}

bool FD3D12DepthStencilState::GetInitializer(FDepthStencilStateInitializerRHI& Init)
{
    Init.bEnableDepthWrite =
        Desc.DepthWriteMask == D3D12_DEPTH_WRITE_MASK_ALL;

    Init.DepthTest = ReverseTranslateCompareFunction(Desc.DepthFunc);

    return true;
}


static D3D12_BLEND_OP TranslateBlendOp(EBlendOperation Op)
{
    switch (Op)
    {
    case BO_Add:             return D3D12_BLEND_OP_ADD;
    case BO_Subtract:        return D3D12_BLEND_OP_SUBTRACT;
    case BO_Min:             return D3D12_BLEND_OP_MIN;
    case BO_Max:             return D3D12_BLEND_OP_MAX;
    case BO_ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
    default:
        throw std::invalid_argument("Unsupported blend operation");
    }
}

static EBlendOperation ReverseTranslateBlendOp(D3D12_BLEND_OP Op)
{
    switch (Op)
    {
    case D3D12_BLEND_OP_ADD:          return BO_Add;
    case D3D12_BLEND_OP_SUBTRACT:     return BO_Subtract;
    case D3D12_BLEND_OP_MIN:          return BO_Min;
    case D3D12_BLEND_OP_MAX:          return BO_Max;
    case D3D12_BLEND_OP_REV_SUBTRACT: return BO_ReverseSubtract;
    default:
        throw std::invalid_argument("Unsupported D3D12 blend operation");
    }
}

static D3D12_BLEND TranslateBlendFactor(EBlendFactor Factor)
{
    switch (Factor)
    {
    case BF_Zero:               return D3D12_BLEND_ZERO;
    case BF_One:                return D3D12_BLEND_ONE;
    case BF_SourceAlpha:        return D3D12_BLEND_SRC_ALPHA;
    case BF_InverseSourceAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
    default:
        throw std::invalid_argument("Unsupported blend factor");
    }
}

static EBlendFactor ReverseTranslateBlendFactor(D3D12_BLEND Factor)
{
    switch (Factor)
    {
    case D3D12_BLEND_ZERO:          return BF_Zero;
    case D3D12_BLEND_ONE:           return BF_One;
    case D3D12_BLEND_SRC_ALPHA:     return BF_SourceAlpha;
    case D3D12_BLEND_INV_SRC_ALPHA: return BF_InverseSourceAlpha;
    default:
        throw std::invalid_argument("Unsupported D3D12 blend factor");
    }
}


TRefCountPtr<FRHIBlendState> FD3D12DynamicRHI::RHICreateBlendState(const FBlendStateInitializerRHI& Initializer)
{
    TRefCountPtr<FD3D12BlendState> State = std::make_shared<FD3D12BlendState>();
    D3D12_BLEND_DESC& Desc = State->Desc;

    Desc.AlphaToCoverageEnable = Initializer.bUseAlphaToCoverage;
    Desc.IndependentBlendEnable = Initializer.bUseIndependentRenderTargetBlendStates;

    static_assert(MaxSimultaneousRenderTargets <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

    for (uint32 Index = 0; Index < MaxSimultaneousRenderTargets; Index++)
    {
        const FBlendStateInitializerRHI::FRenderTarget& Src = Initializer.RenderTargets[Index];
        D3D12_RENDER_TARGET_BLEND_DESC& Dst = Desc.RenderTarget[Index];

        Dst.BlendEnable = Src.ColorBlendOp != BO_Add ||
            Src.ColorSrcBlend != BF_One ||
            Src.ColorDestBlend != BF_Zero ||
            Src.AlphaBlendOp != BO_Add ||
            Src.AlphaSrcBlend != BF_One ||
            Src.AlphaDestBlend != BF_Zero;


        Dst.BlendOp = TranslateBlendOp(Src.ColorBlendOp);
        Dst.SrcBlend = TranslateBlendFactor(Src.ColorSrcBlend);
        Dst.DestBlend = TranslateBlendFactor(Src.ColorDestBlend);

        Dst.BlendOpAlpha = TranslateBlendOp(Src.AlphaBlendOp);
        Dst.SrcBlendAlpha = TranslateBlendFactor(Src.AlphaSrcBlend);
        Dst.DestBlendAlpha = TranslateBlendFactor(Src.AlphaDestBlend);

        Dst.RenderTargetWriteMask = static_cast<UINT8>(
            ((Src.ColorWriteMask & CW_RED)
                ? D3D12_COLOR_WRITE_ENABLE_RED : 0) |
            ((Src.ColorWriteMask & CW_GREEN)
                ? D3D12_COLOR_WRITE_ENABLE_GREEN : 0) |
            ((Src.ColorWriteMask & CW_BLUE)
                ? D3D12_COLOR_WRITE_ENABLE_BLUE : 0) |
            ((Src.ColorWriteMask & CW_ALPHA)
                ? D3D12_COLOR_WRITE_ENABLE_ALPHA : 0));

        // 本节不使用逻辑运算。
        Dst.LogicOpEnable = false;
        Dst.LogicOp = D3D12_LOGIC_OP_NOOP;

    }
    return State;
}

//这个反向转换针对通过我们创建接口生成、且未被外部修改的状态对象。不需要恢复独立的 BlendEnable 字段，因为 RHI 描述会通过公式重新推导它。
bool FD3D12BlendState::GetInitializer(FBlendStateInitializerRHI& Init)
{
    Init.bUseAlphaToCoverage = Desc.AlphaToCoverageEnable != FALSE;
    Init.bUseIndependentRenderTargetBlendStates = Desc.IndependentBlendEnable != FALSE;
    for (uint32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
    {
        const D3D12_RENDER_TARGET_BLEND_DESC& Src =
            Desc.RenderTarget[Index];

        FBlendStateInitializerRHI::FRenderTarget& Dst =
            Init.RenderTargets[Index];

        Dst.ColorBlendOp = ReverseTranslateBlendOp(Src.BlendOp);
        Dst.ColorSrcBlend = ReverseTranslateBlendFactor(Src.SrcBlend);
        Dst.ColorDestBlend = ReverseTranslateBlendFactor(Src.DestBlend);

        Dst.AlphaBlendOp = ReverseTranslateBlendOp(Src.BlendOpAlpha);
        Dst.AlphaSrcBlend = ReverseTranslateBlendFactor(Src.SrcBlendAlpha);
        Dst.AlphaDestBlend = ReverseTranslateBlendFactor(Src.DestBlendAlpha);

        Dst.ColorWriteMask = static_cast<EColorWriteMask>(
            ((Src.RenderTargetWriteMask & D3D12_COLOR_WRITE_ENABLE_RED)
                ? CW_RED : 0) |
            ((Src.RenderTargetWriteMask & D3D12_COLOR_WRITE_ENABLE_GREEN)
                ? CW_GREEN : 0) |
            ((Src.RenderTargetWriteMask & D3D12_COLOR_WRITE_ENABLE_BLUE)
                ? CW_BLUE : 0) |
            ((Src.RenderTargetWriteMask & D3D12_COLOR_WRITE_ENABLE_ALPHA)
                ? CW_ALPHA : 0));
    }

    return true;
}

//UE 的这个入口也在 D3D12State.cpp，但会进一步经过 PSO 缓存。我们这步只完成根签名缓存，底层 PSO 暂时仍然每次创建。
TRefCountPtr<FRHIGraphicsPipelineState> FD3D12DynamicRHI::RHICreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer)
{
    const FD3D12RootSignature* RootSignature = GetAdapter()->GetRootSignature(Initializer.BoundShaderState);

    const FD3D12LowLevelGraphicsPipelineStateDesc LowLevelDesc =
        GetLowLevelGraphicsPipelineStateDesc(
            Initializer, RootSignature);

    TRefCountPtr<FD3D12PipelineState> LowLevelPSO =
        std::make_shared<FD3D12PipelineState>(
            GetDevice(), LowLevelDesc.Desc);

    return std::make_shared<FD3D12GraphicsPipelineState>(
        Initializer,
        RootSignature,
        std::move(LowLevelPSO));
}
