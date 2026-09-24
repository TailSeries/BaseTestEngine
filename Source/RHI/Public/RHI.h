#pragma once
#include "RHIModule.h"
#include "RHIDefinitions.h"
#include <vector>
#include <array>

/*
 * FVertexElement 到 D3D12_INPUT_ELEMNT_DESC的转换关系
 * 
 * FVertexElement                  D3D12_INPUT_ELEMENT_DESC
 * ──────────────────────────────────────────────────────
 * StreamIndex                  → InputSlot
 * Offset                       → AlignedByteOffset
 * Type                         → Format
 * AttributeIndex               → SemanticIndex
 * bUseInstanceIndex            → InputSlotClass / InstanceDataStepRate
 * 
 * 为什么元素还需要记录Stride？1.不能直接根据属性大小推算？因为顶点中可能包含 padding，或者当前声明只使用其中部分属性。2.UE 还会检查：同一条流上的各元素，其 Stride 必须一致。
 *  */

struct FVertexElement
{
	uint8 StreamIndex = 0; // 输入槽位
	uint8 Offset = 0; //结构体内偏移
	EVertexElementType Type = VET_None;// 数据类型
	uint8 AttributeIndex = 0; // 语义索引（默认要求必须是ATTRIBUTE0~开始）
	uint16 Stride = 0;// 步长
	uint16 bUseInstanceIndex = 0; // 用于instance渲染

	FVertexElement() = default;
    FVertexElement(
        uint8 InStreamIndex,
        uint8 InOffset,
        EVertexElementType InType,
        uint8 InAttributeIndex,
        uint16 InStride,
        bool bInUseInstanceIndex = false)
        : StreamIndex(InStreamIndex)
        , Offset(InOffset)
        , Type(InType)
        , AttributeIndex(InAttributeIndex)
        , Stride(InStride)
        , bUseInstanceIndex(bInUseInstanceIndex)
    {}
};
using FVertexDeclarationElementList = std::vector<FVertexElement>;


/*
 * 光栅状态抽象
 * 本项目默认填充模式选 FM_Solid，与读到的 UE 默认 FM_Point 不同.
 */
struct FRasterizerStateInitializerRHI
{
    ERasterizerFillMode FillMode = FM_Solid;
    ERasterizerCullMode CullMode = CM_None;
    /*
     * 总偏移的公式大概是： 
     *  Bias = 固定偏移项
            SlopeScaledDepthBias * max(abs(dz/dx), abs(dz/dy));
			depth += Bias;
     */
    float DepthBias = 0.0f;//深度偏移参数 给光栅化得到的深度添加偏移，常用于缓解共面闪烁（Z-fighting）和阴影痤疮（shadow acne）。注意：DepthBias = 1 不代表深度直接加 1.0。实际偏移与深度缓冲区格式、精度有关。
    float SlopeScaleDepthBias = 0.0f;//随多边形深度斜率变化的偏移  深度在屏幕上变化越快，产生的偏移越大，用于处理倾斜表面。
    ERasterizerDepthClipMode DepthClipMode = ERasterizerDepthClipMode::DepthClip;
    bool bAllowMSAA = false;//后端光栅描述中的多重采样开关，不等于设置渲染目标采样数

    FRasterizerStateInitializerRHI() = default;
    FRasterizerStateInitializerRHI(
        ERasterizerFillMode InFillMode,
        ERasterizerCullMode InCullMode,
        bool bInAllowMSAA
    ):FillMode(InFillMode), CullMode(InCullMode), bAllowMSAA(bInAllowMSAA){};

};


struct FDepthStencilStateInitializerRHI
{
    bool bEnableDepthWrite;// 开启深度写入
    ECompareFunction DepthTest;//深度比较函数

    FDepthStencilStateInitializerRHI(bool bInEnableDepthWrite = true, ECompareFunction InDepthTest = CF_LessEqual)
	    :bEnableDepthWrite(bInEnableDepthWrite),DepthTest(InDepthTest)
    {}

};


/*
 * 个 Draw 可以同时输出到多个颜色附件，各个附件可能需要不同的混合方式
 * UE 的 [RHICreateBlendState (line 445)](/F:/workspace/UnrealEngine58/Engine/Source/Runtime/D3D12RHI/Private/D3D12State.cpp:445) 会识别这种“直接覆盖”的配置，将原生 BlendEnable 设为 false。
 * 所以 RHI 描述中没有另外要求你填写 BlendEnable，而是由混合公式推导。
 * 
 * 
 * 示例：		
		FBlendStateInitializerRHI AlphaBlendInitializer(
			FBlendStateInitializerRHI::FRenderTarget(
				BO_Add,
				BF_SourceAlpha,
				BF_InverseSourceAlpha,
				BO_Add,
				BF_One,
				BF_InverseSourceAlpha,
				CW_RGBA));

		RGB = SourceRGB × SourceAlpha
			+ DestinationRGB ×(1 - SourceAlpha)

			Alpha = SourceAlpha
			+ DestinationAlpha ×(1 - SourceAlpha)
			
 */
class FBlendStateInitializerRHI
{
public:

    struct FRenderTarget
    {
        EBlendOperation ColorBlendOp;
        EBlendFactor ColorSrcBlend;
        EBlendFactor ColorDestBlend;

        EBlendOperation AlphaBlendOp;
        EBlendFactor AlphaSrcBlend;
        EBlendFactor AlphaDestBlend;

        EColorWriteMask ColorWriteMask;

        /*
         * 默认就是 RGB：Source × 1 + Destination × 0
         * Alpha：SourceAlpha × 1 + DestinationAlpha × 0
         * 写入：RGBA 全部
         */
        FRenderTarget(
            EBlendOperation InColorBlendOp = BO_Add,
            EBlendFactor InColorSrcBlend = BF_One,
            EBlendFactor InColorDestBlend = BF_Zero,
            EBlendOperation InAlphaBlendOp = BO_Add,
            EBlendFactor InAlphaSrcBlend = BF_One,
            EBlendFactor InAlphaDestBlend = BF_Zero,
            EColorWriteMask InColorWriteMask = CW_RGBA)
            : ColorBlendOp(InColorBlendOp)
            , ColorSrcBlend(InColorSrcBlend)
            , ColorDestBlend(InColorDestBlend)
            , AlphaBlendOp(InAlphaBlendOp)
            , AlphaSrcBlend(InAlphaSrcBlend)
            , AlphaDestBlend(InAlphaDestBlend)
            , ColorWriteMask(InColorWriteMask)
        {}
    };

    std::array<FRenderTarget, MaxSimultaneousRenderTargets> RenderTargets{};
    //bUseIndependentRenderTargetBlendStates == false 时，D3D12 使用第 0 项作为各颜色目标的混合配置；设为 true 时，才分别使用每个目标的配置。
    bool bUseIndependentRenderTargetBlendStates = false;
    bool bUseAlphaToCoverage = false;
    FBlendStateInitializerRHI() = default;
    FBlendStateInitializerRHI(const FRenderTarget& InRenderTargetBlendState, bool bInUseAlphaCoverage = false)
	    :bUseIndependentRenderTargetBlendStates(false)
		,bUseAlphaToCoverage(bInUseAlphaCoverage)
    {
        RenderTargets[0] = InRenderTargetBlendState;
    }

};
