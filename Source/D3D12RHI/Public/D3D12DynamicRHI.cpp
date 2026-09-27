#include "D3D12DynamicRHI.h"
#include "D3D12Adapter.h"
#include "D3D12Device.h"
#include "D3D12Resources.h"
#include "D3D12Shader.h"
#include "D3D12CommandContext.h"

FD3D12DynamicRHI::FD3D12DynamicRHI() = default;

FD3D12DynamicRHI::~FD3D12DynamicRHI() = default;

void FD3D12DynamicRHI::Init()
{
	FD3D12AdapterDesc Desc;
	if (!FD3D12Adapter::FindAdapter(Desc))
        throw std::runtime_error("No D3D12 adapter available");
	Adapter = std::make_unique<FD3D12Adapter>(Desc);
	Adapter->InitializeDevices();
	Device = Adapter->GetDevice();
}

void FD3D12DynamicRHI::Shutdown()
{
    if (Device) Device->GetDefaultCommandContext().WaitForGPU();
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
	// D3D12 没有独立的 Shader 对象；这里保存字节码，随后交给 PSO 创建使用
	if (CreateShaderDesc.Code.empty())
	{
		throw std::invalid_argument("Shader bytecode must not be empty");
	}

	TRefCountPtr<FD3D12VertexShader> Shader = std::make_shared<FD3D12VertexShader>();
	Shader->Code.assign(CreateShaderDesc.Code.begin(), CreateShaderDesc.Code.end());
	Shader->ResourceCounts = CreateShaderDesc.ResourceCounts;
	return Shader;
}

TRefCountPtr<FRHIPixelShader> FD3D12DynamicRHI::RHICreatePixelShader(const FRHICreateShaderDesc& CreateShaderDesc)
{
	// D3D12 没有独立的 Shader 对象；这里保存字节码，随后交给 PSO 创建使用
	if (CreateShaderDesc.Code.empty())
	{
		throw std::invalid_argument("Shader bytecode must not be empty");
	}

	TRefCountPtr<FD3D12PixelShader> Shader = std::make_shared<FD3D12PixelShader>();
	Shader->Code.assign(CreateShaderDesc.Code.begin(), CreateShaderDesc.Code.end());
	Shader->ResourceCounts = CreateShaderDesc.ResourceCounts;
	return Shader;
}






IRHICommandContext* FD3D12DynamicRHI::RHIGetDefaultContext()
{
    if (!Device) throw std::logic_error("RHI is not initialized");
    return &Device->GetDefaultCommandContext();
}
