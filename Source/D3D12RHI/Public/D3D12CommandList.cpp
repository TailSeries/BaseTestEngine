#include "D3D12CommandList.h"
#include "D3D12Device.h"

FD3D12CommandAllocator::FD3D12CommandAllocator(FD3D12Device* InDevice, ED3D12QueueType QueueType)
{
	ID3D12Device* D3DDevice = InDevice->GetDevice();
	VERIFY_D3D12(D3DDevice->CreateCommandAllocator(GetD3DCommandListType(QueueType), IID_PPV_ARGS(&Allocator)));
	
}

FD3D12CommandAllocator::~FD3D12CommandAllocator() = default;

void FD3D12CommandAllocator::Reset()
{
	VERIFY_D3D12(Allocator->Reset());
}

FD3D12CommandList::FD3D12CommandList(FD3D12Device* InDevice, FD3D12CommandAllocator* InAllocator, ED3D12QueueType QueueType)
{
	ID3D12Device* D3DDevice = InDevice->GetDevice();
	// 初始 PSO（清屏不需要，先 nullptr）
	VERIFY_D3D12(D3DDevice->CreateCommandList(0, GetD3DCommandListType(QueueType), InAllocator->GetAllocator(), nullptr, IID_PPV_ARGS(&CommandList)));
	// CreateCommandList 建出来是"打开"状态，先 Close，让每帧统一 Reset→录制→Close
	VERIFY_D3D12(CommandList->Close());
}

FD3D12CommandList::~FD3D12CommandList() = default;

void FD3D12CommandList::Reset(FD3D12CommandAllocator* InAllocator)
{
	// Reset 会把 list 重新打开录制；第二参是初始 PSO，清屏传 nullptr
	VERIFY_D3D12(CommandList->Reset(InAllocator->GetAllocator(), nullptr));
}


void FD3D12CommandList::Close()
{
	VERIFY_D3D12(CommandList->Close());
}

/*
 * ---
	要点
	- CreateCommandList 建出来即"打开":所以 ctor 里立刻 Close()。这样每帧的流程统一成 Reset → 录制 → Close,第一帧不用特殊处理。
	- Reset(allocator, PSO) 的第二参 PSO:清屏不画东西,不需要管线状态,传 nullptr。第5章有了 PSO 后这里传真正的 PSO。
	- allocator 和 list 的两个 Reset 不一样:
	  - Allocator->Reset() = 丢弃这个分配器里所有已录命令的内存(GPU 必须跑完)。
	  - CommandList->Reset(alloc) = 让 list 重新开始录制(可以在 GPU 还在跑旧命令时做,只要 list 本身已提交)。
	- 多版本简化:UE 持 GraphicsCommandList1..10,我们只留基础 ID3D12GraphicsCommandList(清屏、后面画三角形都够)。
---
 */

