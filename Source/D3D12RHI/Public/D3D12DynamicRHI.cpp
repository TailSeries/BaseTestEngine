#include "D3D12DynamicRHI.h"
#include "D3D12Adapter.h"
#include "D3D12Device.h"
#include "D3D12Resources.h"

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




