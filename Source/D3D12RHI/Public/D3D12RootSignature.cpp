#include "D3D12RootSignature.h"
#include "D3D12Adapter.h"

#include <stdexcept>
#include <utility>

/*
 * UE 的真实实现是：按固定的生成规则，遍历各 Shader 阶段的资源需求，生成数量和组成可变的根参数.
 * 真正的动态布局的基础，需要能够获取 Shader 的资源绑定元信息的能力，根签名生成器根据这些信息决定需要哪些绑定入口。
 * 我们目前只是教学阶段，先像这样填死假装一下就行
 */
FD3D12RootSignatureDesc::FD3D12RootSignatureDesc(const FD3D12QuantizedBoundShaderState& QBSS)
{
	RootParameterSlots.fill(-1);

	const FShaderRegisterCounts& VS = QBSS.RegisterCounts[SV_Vertex];
	const FShaderRegisterCounts& PS = QBSS.RegisterCounts[SV_Pixel];

	// 当前支持：
   // VS: 可选 b0
   // PS: 可选 t0、s0
   // 暂时不支持其他寄存器、资源数组、UAV、其他 register space，以后需要再补
	if (VS.ConstantBufferCount > 1 ||
		VS.ShaderResourceCount != 0 ||
		VS.SamplerCount != 0 ||
		VS.UnorderedAccessCount != 0 ||
		PS.ConstantBufferCount != 0 ||
		PS.ShaderResourceCount > 1 ||
		PS.SamplerCount > 1 ||
		PS.UnorderedAccessCount != 0)
	{
		throw std::invalid_argument(
			"Unsupported shader resource layout");
	}

	uint32 ParameterCount = 0;
	if (VS.ConstantBufferCount != 0)
	{
		// 根描述符
		RootParameterSlots[VS_RootCBVs] = static_cast<int32>(ParameterCount);
		D3D12_ROOT_PARAMETER& Parameter = TableSlots[ParameterCount++];
		Parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		Parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		Parameter.Descriptor.ShaderRegister = 0;
		Parameter.Descriptor.RegisterSpace = 0;
	}

	if (PS.ShaderResourceCount != 0)
	{
		// 描述符表
		D3D12_DESCRIPTOR_RANGE& Range = DescriptorRanges[0];
		Range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		Range.NumDescriptors = 1;
		Range.BaseShaderRegister = 0;
		Range.RegisterSpace = 0;
		Range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		RootParameterSlots[PS_SRVs] = static_cast<int32>(ParameterCount);
		D3D12_ROOT_PARAMETER& Parameter = TableSlots[ParameterCount++];
		Parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		Parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		Parameter.DescriptorTable.NumDescriptorRanges = 1;
		Parameter.DescriptorTable.pDescriptorRanges = &Range;
	}

	if (PS.SamplerCount)
	{
		// 静态采样器
		D3D12_STATIC_SAMPLER_DESC& Sampler = StaticSamplers[0];

		Sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		Sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		Sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		Sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		Sampler.MipLODBias = 0.0f;
		Sampler.MaxAnisotropy = 1;
		Sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		Sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		Sampler.MinLOD = 0.0f;
		Sampler.MaxLOD = D3D12_FLOAT32_MAX;
		Sampler.ShaderRegister = 0;
		Sampler.RegisterSpace = 0;
		Sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		RootDesc.NumStaticSamplers = 1;
		RootDesc.pStaticSamplers = StaticSamplers.data();
	}

	RootDesc.NumParameters = ParameterCount;
	RootDesc.pParameters = ParameterCount != 0 ? TableSlots.data() : nullptr;
	RootDesc.Flags = QBSS.bAllowIAInputLayout ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT : D3D12_ROOT_SIGNATURE_FLAG_NONE;

}

FD3D12RootSignature::FD3D12RootSignature(FD3D12Adapter* InParent, const FD3D12QuantizedBoundShaderState& InQBSS)
	:FD3D12AdapterChild(InParent)
{
	FD3D12RootSignatureDesc Desc(InQBSS);
	for (uint32 Index = 0; Index < RPK_RootParameterKeyCount; ++Index)
	{
		RootParameterSlots[Index] = Desc.GetRootParameterSlot(static_cast<ERootParameterKeys>(Index));
	}

	ComPtr<ID3DBlob> Serialized;
	ComPtr<ID3DBlob> Error;
	const HRESULT SerializeResult = D3D12SerializeRootSignature(
		&Desc.GetDesc(),
		D3D_ROOT_SIGNATURE_VERSION_1,
		Serialized.GetAddressOf(),
		Error.GetAddressOf());

	if (FAILED(SerializeResult))
	{
		if (Error)
		{
			OutputDebugStringA(
				static_cast<const char*>(Error->GetBufferPointer()));
		}

		throw std::runtime_error("Serialize root signature failed");
	}

	const HRESULT CreateResult =
		GetParentAdapter()->GetD3DDevice()->CreateRootSignature(
			0,
			Serialized->GetBufferPointer(),
			Serialized->GetBufferSize(),
			IID_PPV_ARGS(RootSignature.GetAddressOf()));

	if (FAILED(CreateResult))
	{
		throw std::runtime_error("Create root signature failed");
	}
}

FD3D12RootSignature::~FD3D12RootSignature() = default;


FD3D12RootSignature* FD3D12RootSignatureManager::GetRootSignature(const FD3D12QuantizedBoundShaderState& QBSS)
{
	const auto It = RootSignatureMap.find(QBSS);

	if (It != RootSignatureMap.end())
	{
		return It->second.get();
	}

	return CreateRootSignature(QBSS);
}

FD3D12RootSignature* FD3D12RootSignatureManager::CreateRootSignature(const FD3D12QuantizedBoundShaderState& QBSS)
{
	std::unique_ptr<FD3D12RootSignature> RootSignature = std::make_unique<FD3D12RootSignature>(GetParentAdapter(), QBSS);
	auto [It, bInserted] = RootSignatureMap.emplace(
		QBSS, std::move(RootSignature));
	return It->second.get();
}
void FD3D12RootSignatureManager::Destroy()
{
	RootSignatureMap.clear();
}
/*
 *
说明

- 空签名 ≠ 什么都不用:三角形的顶点(位置/颜色)从 VB 经 IA 进 shader,是"输入布局"路径,不是根签名参数。等要传 MVP 矩阵(常量缓冲)时,才往根签名加 CBV 参数——那是下一步让三角形动起来要做的。
- ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT:忘了它是新手最常见的坑,PSO 会以 E_INVALIDARG 挂掉且不说原因。
- 版本 1.0:D3D_ROOT_SIGNATURE_VERSION_1。1.1 加了静态描述符优化(告诉驱动描述符创建后不变),学习先用 1.0。
- 当前实现：Adapter 从 Shader ResourceCounts 生成 QBSS，Manager 按布局缓存根签名，RootSignatureDesc 生成原生描述；元数据暂由调用方填写。UE 从编译产物读取元数据。
---
 */
