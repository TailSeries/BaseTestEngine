#include "D3D12Device.h"
#include "D3DAdapter.h"
FD3D12Device::FD3D12Device(FD3D12Adapter* InAdapter, uint32 InGPUIndex)
    : Adapter(InAdapter)
    , GPUIndex(InGPUIndex)
{
	// 直接建立三条队列，（Direct copy Async），这时候Adapter的RootDevice必须已经创建了出来
    Queues.reserve(static_cast<uint32>(ED3D12QueueType::Count));
    for (uint32 i = 0; i < static_cast<uint32>(ED3D12QueueType::Count); i++)
    {
        Queues.push_back(std::make_unique<FD3D12Queue>(this, static_cast<ED3D12QueueType>(i)));
    }
}
FD3D12Device::~FD3D12Device() = default;


ID3D12Device* FD3D12Device::GetDevice()
{
    return Adapter->GetD3DDevice();   
}