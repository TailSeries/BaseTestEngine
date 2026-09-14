#include "D3D12Descriptors.h"
#include "D3D12Device.h"
#include <cassert>

FD3D12DescriptorHeap::FD3D12DescriptorHeap(FD3D12Device* InDevice, D3D12_DESCRIPTOR_HEAP_TYPE InType, uint32 InNumDescriptors, bool bInShaderVisible)
	:Parent(InDevice)
	,Type(InType)
	,NumDescriptors(InNumDescriptors)
{
	ID3D12Device* D3DDevice = InDevice->GetDevice();

	// RTV/DSV 堆强制非shader-visible（如果呆了SHADER_VISIBLE就要让它创建失败）
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
	D3D12_CPU_DESCRIPTOR_HANDLE H = Cpubase;
	H.ptr += static_cast<size_t>(slot * DescriptorSize);
	return H;
}

D3D12_GPU_DESCRIPTOR_HANDLE FD3D12DescriptorHeap::GetGPUHandle(uint32 slot) const
{
	assert(bShaderVisible);
	D3D12_GPU_DESCRIPTOR_HANDLE H = GpuBase;
	H.ptr += static_cast<size_t>(slot * DescriptorSize);
	return H;
}

uint32 FD3D12DescriptorHeap::Allocate()
{
	assert(NextFreeSlot < NumDescriptors);
	return NextFreeSlot++;
}
/*
 * 说明
 * - GetCPUHandle 手动算偏移:UE 用 CD3DX12_CPU_DESCRIPTOR_HANDLE(base, slot, size) 辅助类,底层就是 base.ptr + slot*size,我们手写一样。
 * - GetGPUDescriptorHandleForHeapStart 只对 shader-visible 合法:对非 shader-visible 堆调用是未定义行为,所以 ctor 里用 bShaderVisible 守住。
 * - 线性 Allocate:UE 有 free-list、子分配、回收;我们先"下一个空槽"够用
 **/

