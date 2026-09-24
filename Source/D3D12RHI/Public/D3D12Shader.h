#pragma once
#include "D3D12RHIModule.h"
#include "D3D12RHIPrivate.h"
#include "RHIResources.h"
#include <vector>
#include <array>
// 运行时编译 HLSL → 字节码 blob。
// UE 是离线编译系统（ShaderCompilerWorker/DXC），我们学习阶段用 D3DCompile 直接运行时编译。
// Target 形如 "vs_5_0" / "ps_5_0"；失败返回 nullptr 并打印错误。
using Microsoft::WRL::ComPtr;



D3D12RHIMODULE ComPtr<ID3DBlob> CompileShader(const char* source, const char* EntryPoint, const char* Target);



struct D3D12RHIMODULE FD3D12ShaderData
{
	// shader 字节码
	std::vector<uint8> Code;
	FShaderCodePackedResourceCounts ResourceCounts{};
	//GetShaderBytecode() 返回的结构只是指针和长度，不会复制字节码；字节存储由 Code 持有。因此创建 PSO 时，Shader 对象必须还活着，且不能同时修改其 Code。
	D3D12_SHADER_BYTECODE GetShaderByteCode() const
	{
		return { Code.data(), Code.size() };
	}
};


class D3D12RHIMODULE FD3D12VertexShader :public FRHIVertexShader, public FD3D12ShaderData
{
public:
	enum { StaticFrequency = SF_Vertex };
};


class D3D12RHIMODULE FD3D12PixelShader :public FRHIPixelShader, public FD3D12ShaderData
{
public:
	enum { StaticFrequency = SF_Pixel };
};

using FD3D12VertexElements = std::vector<D3D12_INPUT_ELEMENT_DESC>;
// 我们暂时省略哈希与缓存，每次创建独立对象
class D3D12RHIMODULE FD3D12VertexDeclaration :public FRHIVertexDeclaration
{
public:
	explicit FD3D12VertexDeclaration(const FD3D12VertexElements& Elements, const uint16* InStrides);
	virtual bool GetInitializer(FVertexDeclarationElementList& Init) override;
	FD3D12VertexElements VertexElements;
	std::array<uint16, D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> StreamStrides{}; // IA 阶段最多D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT个槽位

};

