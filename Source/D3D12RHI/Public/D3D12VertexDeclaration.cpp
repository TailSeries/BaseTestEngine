#include "d3d12shader.h"
#include "D3D12DynamicRHI.h"
#include <stdexcept>
#include "RHI.h"
#include <algorithm>

// RHI 元素类型 → D3D12 输入格式。
// UE 在 Key 的构造函数中 switch；这里提取成文件内部辅助函数。


static DXGI_FORMAT ToD3D12VertexFormat(EVertexElementType Type)
{
	switch (Type)
	{
	case VET_Float1:  return DXGI_FORMAT_R32_FLOAT;
	case VET_Float2:  return DXGI_FORMAT_R32G32_FLOAT;
	case VET_Float3:  return DXGI_FORMAT_R32G32B32_FLOAT;
	case VET_Float4:  return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case VET_UInt:    return DXGI_FORMAT_R32_UINT;
	case VET_UByte4:  return DXGI_FORMAT_R8G8B8A8_UINT;
	case VET_UByte4N: return DXGI_FORMAT_R8G8B8A8_UNORM;
	case VET_Short2:  return DXGI_FORMAT_R16G16_SINT;
	case VET_Short2N: return DXGI_FORMAT_R16G16_SNORM;
	default:
		throw std::invalid_argument("Unsupported vertex element type");
	}
}

// D3D12 输入格式 → RHI 元素类型。
static EVertexElementType FromD3D12VertexFormat(DXGI_FORMAT Format)
{
	switch (Format)
	{
	case DXGI_FORMAT_R32_FLOAT:          return VET_Float1;
	case DXGI_FORMAT_R32G32_FLOAT:       return VET_Float2;
	case DXGI_FORMAT_R32G32B32_FLOAT:    return VET_Float3;
	case DXGI_FORMAT_R32G32B32A32_FLOAT: return VET_Float4;
	case DXGI_FORMAT_R32_UINT:           return VET_UInt;
	case DXGI_FORMAT_R8G8B8A8_UINT:      return VET_UByte4;
	case DXGI_FORMAT_R8G8B8A8_UNORM:     return VET_UByte4N;
	case DXGI_FORMAT_R16G16_SINT:        return VET_Short2;
	case DXGI_FORMAT_R16G16_SNORM:       return VET_Short2N;
	default:
		throw std::invalid_argument("Unsupported D3D12 vertex format");
	}
}


// 对应 UE 的 FD3D12VertexDeclarationKey。
// 当前只做转换和规范化；哈希与缓存随后补。

struct FD3D12VertexDeclarationKey
{
	// D3D12_INPUT_ELEMENT_DESC 的布局描述
	FD3D12VertexElements VertexElements;

	std::array<uint16, D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> StreamStrides{};
	// 注意，可能有这样的考虑：不能把 Offset + 属性大小 <= Stride 当作通用硬性约束，否则会排除属性重叠、零步长等特殊读取方式
	explicit FD3D12VertexDeclarationKey(const FVertexDeclarationElementList& InElements)
	{
		if (InElements.size() > D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT) // 一个布局里最多32个元素（float4），最多128个float分量（D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENTS_COMPONENTS）
		{
			throw std::invalid_argument("Too many vertex elements");
		}

		std::array<bool, D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> UsedStreams{};
		VertexElements.reserve(InElements.size());
		for (const FVertexElement& Element : InElements)
		{
			const uint8 StreamIndex = Element.StreamIndex;
			if (StreamIndex >= StreamStrides.size())
			{
				throw std::invalid_argument(
					"Vertex stream index out of range");
			}

			D3D12_INPUT_ELEMENT_DESC D3DElement{};
			D3DElement.InputSlot = Element.StreamIndex;
			D3DElement.AlignedByteOffset = Element.Offset;
			D3DElement.Format = ToD3D12VertexFormat(Element.Type);
			D3DElement.SemanticIndex = Element.AttributeIndex;

			D3DElement.InputSlotClass = Element.bUseInstanceIndex
				? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
				: D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

			D3DElement.InstanceDataStepRate = Element.bUseInstanceIndex ? 1 : 0; // 每个实例前进一条数据

			if (UsedStreams[StreamIndex])
			{
				if (StreamStrides[StreamIndex] != Element.Stride)
				{
					// 已经登记的slot的步长和元素要求不一致
					throw std::invalid_argument(
						"Elements in one stream must use the same stride");
				}
			}
			else
			{
				UsedStreams[StreamIndex] = true;
				StreamStrides[StreamIndex] = Element.Stride;
			}

			VertexElements.push_back(D3DElement);
		}

		// 对齐 UE：按输入流、偏移、属性索引排序。
		std::sort(
			VertexElements.begin(),
			VertexElements.end(),
			[](const D3D12_INPUT_ELEMENT_DESC& A,
				const D3D12_INPUT_ELEMENT_DESC& B)
			{
				if (A.InputSlot != B.InputSlot)
				{
					return A.InputSlot < B.InputSlot;
				}

				if (A.AlignedByteOffset != B.AlignedByteOffset)
				{
					return A.AlignedByteOffset < B.AlignedByteOffset;
				}

				return A.SemanticIndex < B.SemanticIndex;
			});

		// UE 在计算哈希之后设置这个常量字符串。
		// 当前暂未计算哈希，仍保留最后设置的顺序。
		for (D3D12_INPUT_ELEMENT_DESC& Element : VertexElements)
		{
			Element.SemanticName = "ATTRIBUTE";
		}
	}

};


FD3D12VertexDeclaration::FD3D12VertexDeclaration(const FD3D12VertexElements& Elements, const uint16* InStrides)
	:VertexElements(Elements)
{
	// 调用方传入包含完整槽位数量的步长数组。
	std::copy_n(
		InStrides,
		StreamStrides.size(),
		StreamStrides.begin());
}


// 对齐 UE：从后端描述反向恢复 RHI 顶点元素。
bool FD3D12VertexDeclaration::GetInitializer(FVertexDeclarationElementList& Init)
{
	Init.clear();
	Init.reserve(VertexElements.size());
	for (const D3D12_INPUT_ELEMENT_DESC& Element : VertexElements)
	{
		Init.emplace_back(
			static_cast<uint8>(Element.InputSlot),
			static_cast<uint8>(Element.AlignedByteOffset),
			FromD3D12VertexFormat(Element.Format),
			static_cast<uint8>(Element.SemanticIndex),
			StreamStrides[Element.InputSlot],
			Element.InputSlotClass ==
			D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA);
	}

	return true;
}


TRefCountPtr<FRHIVertexDeclaration> FD3D12DynamicRHI::RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	FD3D12VertexDeclarationKey Key(Elements);

	// UE 此处通过 Key 查询缓存，未命中才创建。
	// 当前暂不缓存，每次创建独立声明对象。
	return std::make_shared<FD3D12VertexDeclaration>(Key.VertexElements, Key.StreamStrides.data());
}
