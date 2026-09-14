#include "D3D12Resources.h"
#include "D3D12Device.h"

FD3D12Resource::FD3D12Resource(FD3D12Device* InParent, ID3D12Resource* InResource, D3D12_RESOURCE_STATES InInitialSatte, const D3D12_RESOURCE_DESC& InDesc, D3D12_HEAP_TYPE InHeapType)
	:Parent(InParent),
	Resource(InResource),
	State(InInitialSatte),
	Desc(InDesc),
	HeapType(InHeapType)
{
	// Buffer 才有 GPU 虚拟地址；Texture 没有（保持 0）
	if (Resource && InDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		GPUVirtualAddress = Resource->GetGPUVirtualAddress();
	}
}

FD3D12Resource::~FD3D12Resource() = default;

void* FD3D12Buffer::GetMappedData()
{
	if (!MappedData && ResourcePtr)
	{
		D3D12_RANGE ReadRange{ 0,0 };
		VERIFY_D3D12(ResourcePtr->GetResource()->Map(0, &ReadRange, &MappedData));

	}

	return MappedData;
}


FD3D12Texture::~FD3D12Texture() = default;

