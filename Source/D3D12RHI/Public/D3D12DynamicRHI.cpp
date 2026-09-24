#include "D3D12DynamicRHI.h"
#include "D3D12Adapter.h"
#include "D3D12Device.h"
#include "D3D12Resources.h"
#include "D3D12Shader.h"

FD3D12DynamicRHI::FD3D12DynamicRHI() = default;

FD3D12DynamicRHI::~FD3D12DynamicRHI() = default;

void FD3D12DynamicRHI::Init()
{
	FD3D12AdapterDesc Desc;
	FD3D12Adapter::FindAdapter(Desc);
	Adapter = std::make_unique<FD3D12Adapter>(Desc);
	Adapter->InitializeDevices();
	Device = Adapter->GetDevice();
}

void FD3D12DynamicRHI::Shutdown()
{
	Device = nullptr;
	Adapter.reset();
}

TRefCountPtr<FRHIBuffer> FD3D12DynamicRHI::RHICreateBuffer(const FRHIBufferDesc& Desc, const void* InitialData)
{
	return Device->CreateBuffer(Desc, InitialData);
}

TRefCountPtr<FRHITexture> FD3D12DynamicRHI::RHICreateTexture(const FRHITextureDesc& Desc, const void* InitialData)
{
	return Device->CreateTexture(Desc, InitialData);
}

TRefCountPtr<FRHIVertexShader> FD3D12DynamicRHI::RHICreateVertexShader(const FRHICreateShaderDesc& CreateShaderDesc)
{
	//这个过程没有创建独立的 ID3D12VertexShader 对象，原生字节码随后交给 PSO 创建使用
	if (CreateShaderDesc.Code.empty())
	{
		assert(false && "Vertex shader bytecode must not be empty");
		return nullptr;
	}

	TRefCountPtr<FD3D12VertexShader> Shader = std::make_shared<FD3D12VertexShader>();
	Shader->Code.assign(CreateShaderDesc.Code.begin(), CreateShaderDesc.Code.end());
	Shader->ResourceCounts = CreateShaderDesc.ResourceCounts;
	return Shader;
}

TRefCountPtr<FRHIPixelShader> FD3D12DynamicRHI::RHICreatePixelShader(const FRHICreateShaderDesc& CreateShaderDesc)
{
	//这个过程没有创建独立的 ID3D12VertexShader 对象，原生字节码随后交给 PSO 创建使用
	if (CreateShaderDesc.Code.empty())
	{
		assert(false && "Vertex shader bytecode must not be empty");
		return nullptr;
	}

	TRefCountPtr<FD3D12PixelShader> Shader = std::make_shared<FD3D12PixelShader>();
	Shader->Code.assign(CreateShaderDesc.Code.begin(), CreateShaderDesc.Code.end());
	Shader->ResourceCounts = CreateShaderDesc.ResourceCounts;
	return Shader;
}





