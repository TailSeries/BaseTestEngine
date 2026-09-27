#include "D3D12Descriptors.h"
#include "D3D12Device.h"
#include <stdexcept>

FD3D12DescriptorHeap::FD3D12DescriptorHeap(FD3D12Device* InDevice, D3D12_DESCRIPTOR_HEAP_TYPE InType, uint32 InNumDescriptors, bool bInShaderVisible)
	:Parent(InDevice)
	,Type(InType)
	,NumDescriptors(InNumDescriptors)
{
    if (!InDevice || InNumDescriptors == 0 ||
        (bInShaderVisible && InType != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV &&
         InType != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER))
        throw std::invalid_argument("Invalid descriptor heap description");
    FreeSlots.reserve(NumDescriptors);
    AllocatedSlots.resize(NumDescriptors, false);
    for (uint32 Index = NumDescriptors; Index > 0; --Index)
        FreeSlots.push_back(Index - 1);
    ID3D12Device* D3DDevice = InDevice->GetDevice();

	// RTV/DSV 的 shader-visible 请求已在上面拒绝。
	bShaderVisible = bInShaderVisible && Type != D3D12_DESCRIPTOR_HEAP_TYPE_RTV && Type != D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

	D3D12_DESCRIPTOR_HEAP_DESC Desc = {};
	Desc.Type = Type;
	Desc.NumDescriptors = InNumDescriptors;
	Desc.Flags = bShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	Desc.NodeMask = 0;// 我们不支持SLI之类的东西，一个Device就一个Node
	VERIFY_D3D12(D3DDevice->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&Heap)));

	// // 增量大小硬件相关，必须查
	DescriptorSize = D3DDevice->GetDescriptorHandleIncrementSize(Type);
	Cpubase = Heap->GetCPUDescriptorHandleForHeapStart();
	if (bShaderVisible)
	{
		GpuBase = Heap->GetGPUDescriptorHandleForHeapStart();
	}
}

FD3D12DescriptorHeap::~FD3D12DescriptorHeap() = default;

D3D12_CPU_DESCRIPTOR_HANDLE FD3D12DescriptorHeap::GetCPUHandle(uint32 slot) const
{
    if (slot >= NumDescriptors) throw std::out_of_range("Descriptor slot out of range");
	D3D12_CPU_DESCRIPTOR_HANDLE H = Cpubase;
	H.ptr += static_cast<size_t>(slot * DescriptorSize);
	return H;
}

D3D12_GPU_DESCRIPTOR_HANDLE FD3D12DescriptorHeap::GetGPUHandle(uint32 slot) const
{
    if (!bShaderVisible) throw std::logic_error("Heap has no GPU handle");
    if (slot >= NumDescriptors) throw std::out_of_range("Descriptor slot out of range");
	D3D12_GPU_DESCRIPTOR_HANDLE H = GpuBase;
	H.ptr += static_cast<size_t>(slot * DescriptorSize);
	return H;
}

uint32 FD3D12DescriptorHeap::Allocate()
{
    if (FreeSlots.empty()) throw std::runtime_error("Descriptor heap exhausted");
    const uint32 Slot = FreeSlots.back();
    FreeSlots.pop_back();
    AllocatedSlots[Slot] = true;
    ++AllocatedCount;
    return Slot;
}

void FD3D12DescriptorHeap::Free(uint32 Slot)
{
    if (Slot >= NumDescriptors || !AllocatedSlots[Slot])
        throw std::invalid_argument("Invalid or duplicate descriptor free");
    AllocatedSlots[Slot] = false;
    --AllocatedCount;
    FreeSlots.push_back(Slot); // ctor 已 reserve，不在析构路径中重新分配内存。
}
