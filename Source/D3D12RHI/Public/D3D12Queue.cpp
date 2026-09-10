#include "D3D12Queue.h"
#include "D3D12Device.h"

/**
 * 
 * @param InDevice 
 * @param InType 
 */
FD3D12Queue::FD3D12Queue(FD3D12Device* InDevice, ED3D12QueueType InType)
	:Device(InDevice)
	,Type(InType)

{
	ID3D12Device* D3DDevice = InDevice->GetDevice();
	D3D12_COMMAND_QUEUE_DESC Desc ={};
	Desc.Type = GetD3DCommandListType(InType);
	Desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	Desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	Desc.NodeMask = 0;
	VERIFY_D3D12(D3DDevice->CreateCommandQueue(&Desc, IID_PPV_ARGS(&D3DCommandQueue)));

	VERIFY_D3D12(D3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence.D3DFence)));
	Fence.FenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
	assert(Fence.FenceEvent != nullptr);
	Fence.OwnerQueue = this;
}
FD3D12Queue::~FD3D12Queue()
{
	if (Fence.FenceEvent)
	{
		CloseHandle(Fence.FenceEvent);
		Fence.FenceEvent = nullptr;
	}
}
// NextCompletionValue++ 后置递增——先用当前值 Signal,再加 1 留给下次。返回本次值,调用方不用自己算。
uint64 FD3D12Queue::Signal(FD3D12Fence& InFence)
{
	const uint64 SignalValue = InFence.NextCompletionValue++;
	VERIFY_D3D12(D3DCommandQueue->Signal(InFence.D3DFence.Get(), SignalValue));
	return SignalValue;
}

//GPU 端等待 ID3D12CommandQueue::Wait 在 GPU 队列里插一条等待指令,GPU 碰到就停,等 Fence 到 Value 再继续,CPU 不阻塞。用于跨队列同步。
void FD3D12Queue::Wait(FD3D12Fence& InFence, uint64 Value)
{
	VERIFY_D3D12(D3DCommandQueue->Wait(InFence.D3DFence.Get(), Value));
}

//CPU 端阻塞  GPU 当前 Fence 值,没追上就注册 event,CPU 睡在 WaitForSingleObjectEx。这是 UE InterruptThread 干的事,单线程下我们直接在调用线程做。
void FD3D12Queue::WaitCPU(uint64 Value)
{
	if (Fence.D3DFence->GetCompletedValue() < Value)
	{
		VERIFY_D3D12(Fence.D3DFence->SetEventOnCompletion(Value, Fence.FenceEvent));
		WaitForSingleObjectEx(Fence.FenceEvent, INFINITE, FALSE);
	}
}
