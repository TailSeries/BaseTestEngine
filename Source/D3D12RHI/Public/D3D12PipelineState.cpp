#include "D3D12PipelineState.h"
#include "D3D12Device.h"
#include "D3D12RootSignature.h"
#include "D3D12Shader.h"
#include "D3D12State.h"
#include <stdexcept>


FD3D12PipelineState::FD3D12PipelineState(FD3D12Device* InDevice, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& Desc)

{
	ID3D12Device* D3DDevice = InDevice->GetDevice();
	VERIFY_D3D12(D3DDevice->CreateGraphicsPipelineState(&Desc, IID_PPV_ARGS(&PipelineState)));
}

FD3D12PipelineState::~FD3D12PipelineState() = default;

FD3D12GraphicsPipelineState::FD3D12GraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer,
	const FD3D12RootSignature* InRootSignature, TRefCountPtr<FD3D12PipelineState> InPipelineState)
		:FD3D12PipelineStateCommonData(InRootSignature, std::move(InPipelineState)),
		PipelineStateInitializer(Initializer)
{
	const FBoundShaderStateInput& BoundShaderState = PipelineStateInitializer.BoundShaderState;
	if (BoundShaderState.VertexDeclarationRHI)
	{
		const FD3D12VertexDeclaration* VertexDeclaration = static_cast<FD3D12VertexDeclaration*>(BoundShaderState.VertexDeclarationRHI.get());
		StreamStrides = VertexDeclaration->StreamStrides; // 真正的输入布局
	}
}





// 当前仅支持项目已有的两个具体格式。
// UE 使用 GPixelFormats 和 View 格式转换工具，这里暂不引入整套格式表。
static DXGI_FORMAT TranslatePipelineFormat(EPixelFormat Format)
{
	switch (Format)
	{
	case PF_Unknown:
		return DXGI_FORMAT_UNKNOWN;
	case PF_R8G8B8A8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case PF_D32_FLOAT:
		return DXGI_FORMAT_D32_FLOAT;
	default:
		throw std::invalid_argument("Unsupported pipeline format");
	}
}


FD3D12LowLevelGraphicsPipelineStateDesc GetLowLevelGraphicsPipelineStateDesc(const FGraphicsPipelineStateInitializer& Initializer, const FD3D12RootSignature* RootSignature)
{
	const FBoundShaderStateInput& BoundShaderState = Initializer.BoundShaderState;
	// 当前实验使用传统 VS + PS 管线，并显式提供三种固定状态。
	if (!RootSignature ||
		!BoundShaderState.VertexDeclarationRHI ||
		!BoundShaderState.VertexShaderRHI ||
		!BoundShaderState.PixelShaderRHI ||
		!Initializer.RasterizerState ||
		!Initializer.DepthStencilState ||
		!Initializer.BlendState)
	{
		throw std::invalid_argument("Incomplete graphics pipeline initializer");
	}


	if (Initializer.RenderTargetsEnabled > MaxSimultaneousRenderTargets)
	{
		throw std::invalid_argument("Too many render targets");
	}

	if (Initializer.PrimitiveType != PT_TriangleList)
	{
		throw std::invalid_argument("Unsupported primitive type");
	}

	// 当前 SwapChain 和深度缓冲都是单采样。
	if (Initializer.NumSamples != 1)
	{
		throw std::invalid_argument("Only single-sample pipelines are supported");
	}


	FD3D12LowLevelGraphicsPipelineStateDesc Result{};
	Result.pRootSignature = RootSignature;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC& Desc = Result.Desc;
	Desc.pRootSignature = RootSignature->GetRootSignature();
	
	const auto* VertexShader = static_cast<const FD3D12VertexShader*>(BoundShaderState.GetVertexShader());
	const auto* PixelShader = static_cast<const FD3D12PixelShader*>(BoundShaderState.GetPixelShader());
	const auto* VertexDeclaration = static_cast<const FD3D12VertexDeclaration*>(BoundShaderState.VertexDeclarationRHI.get());
	Desc.VS = VertexShader->GetShaderByteCode();
	Desc.PS = PixelShader->GetShaderByteCode();

	Desc.InputLayout = {
	VertexDeclaration->VertexElements.data(),
	static_cast<UINT>(VertexDeclaration->VertexElements.size())
	};

	Desc.RasterizerState = static_cast<const FD3D12RasterizerState*>(Initializer.RasterizerState.get())->Desc;

	Desc.DepthStencilState = static_cast<const FD3D12DepthStencilState*>(Initializer.DepthStencilState.get())->Desc;

	Desc.BlendState = static_cast<const FD3D12BlendState*>(Initializer.BlendState.get())->Desc;

	Desc.SampleMask = UINT_MAX;
	Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	Desc.NumRenderTargets = Initializer.RenderTargetsEnabled;


	for (uint32 Index = 0; Index < Initializer.RenderTargetsEnabled; ++Index)
	{
		// 当前颜色附件只支持这个格式。
		if (Initializer.RenderTargetFormats[Index] != PF_R8G8B8A8_UNORM)
		{
			throw std::invalid_argument("Unsupported color attachment format");
		}

		Desc.RTVFormats[Index] =
			TranslatePipelineFormat(Initializer.RenderTargetFormats[Index]);
	}


	if (Initializer.DepthStencilTargetFormat != PF_Unknown &&
		Initializer.DepthStencilTargetFormat != PF_D32_FLOAT)
	{
		throw std::invalid_argument("Unsupported depth attachment format");
	}

	Desc.DSVFormat =
		TranslatePipelineFormat(Initializer.DepthStencilTargetFormat);

	Desc.SampleDesc.Count = Initializer.NumSamples;
	Desc.SampleDesc.Quality = 0;

	return Result;

}
