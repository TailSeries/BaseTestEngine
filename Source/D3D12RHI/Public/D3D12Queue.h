#pragma once
#include "D3D12RHIPrivate.h"
#include "Core/Base/GenericPlatform.h"

/*
 * 对齐 UE 的 D3D12Queue.h:枚举 + 命令类型辅助函数 + FD3D12Fence + FD3D12Queue。先讲三个 UE 对照点,再给你敲的内容:
 * 1. ED3D12QueueType 完全照抄 UE(Direct=0, Copy, Async, Count)。Count 是惯用法——用作数组长度。
 * 2. FD3D12Fence 是 struct(UE 也是),成员保留 UE 的 OwnerQueue / D3DFence / NextCompletionValue=1。
 * 一处必要偏离:UE 的 Fence 不持有 event,等待交给中断线程;我们单线程,得自己加 HANDLE FenceEvent + Signal/WaitCPU 才能自洽。
 * 3. 构造方式偏离:UE 用 FD3D12Queue(Device*, Type, Index) 构造函数。我们改用默认构造 + Init()——因为 Device 里要放 Queues[Count] 值数组,带参构造 + 值数组会牵扯 move 语义,Init() 模式更简单。这是"简化内部细节、不动骨架"的典型。
 */

using Microsoft::WRL::ComPtr;
class FD3D12Device;
class FD3D12Queue;

// D3D12 队列类型
enum class ED3D12QueueType
{
	Direct = 0,
	Copy, //拷贝
	Async, //异步计算
	Count, // 队列种类数量，用作数组长度
};

// 队列类型 → D3D12 命令列表类型（对应 UE GetD3DCommandListType）
inline D3D12_COMMAND_LIST_TYPE GetD3DCommandListType(ED3D12QueueType Type)
{
	switch (Type)
	{
		case ED3D12QueueType::Direct: return D3D12_COMMAND_LIST_TYPE_DIRECT;
		case ED3D12QueueType::Copy: return D3D12_COMMAND_LIST_TYPE_COPY;
		case ED3D12QueueType::Async: return D3D12_COMMAND_LIST_TYPE_COMPUTE;
		default:return D3D12_COMMAND_LIST_TYPE_DIRECT;
	}
}
// ── FD3D12Fence（UE: struct，D3D12Submission.h）────────
// 单线程简化：自带 event，Signal/WaitCPU 自洽
struct FD3D12Fence
{
	FD3D12Queue* OwnerQueue = nullptr;// 回指所属队列（UE 有）
	ComPtr<ID3D12Fence> D3DFence;
	uint64 NextCompletionValue = 1; // 下一个要 Signal 的值（UE 命名）
	HANDLE FenceEvent = nullptr;
};

// ── FD3D12Queue（UE: class FD3D12Queue final）─────────
class D3D12RHIMODULE FD3D12Queue
{
public:
	// UE 带参构造(UE 还有第三参 int32 QueueIndex,单队列省略——简化内部细节)
	FD3D12Queue(FD3D12Device* InDevice, ED3D12QueueType InType);
	~FD3D12Queue();

	FD3D12Queue(const FD3D12Queue&) = delete;
	FD3D12Queue& operator=(const FD3D12Queue&) = delete;


	// 微小偏离：UE 返回 void，我们返回本次 signal 的值，方便 WaitCPU 直接用
	uint64 Signal(FD3D12Fence& InFence);
	// 对应 UE FD3D12Queue::Wait(FD3D12Fence&, uint64) — GPU 端等待（跨队列同步用）
	void Wait(FD3D12Fence& InFence, uint64 Value);

	// 无 UE 对应，InterruptThread 的单线程替代：CPU 阻塞到 GPU 完成 Value
	void WaitCPU(uint64 Value);

	ID3D12CommandQueue* GetD3DQueue() const { return D3DCommandQueue.Get(); }

	FD3D12Device* Device = nullptr;//  // 回指父 Device
	ED3D12QueueType            Type = ED3D12QueueType::Direct;
	ComPtr<ID3D12CommandQueue> D3DCommandQueue;    // UE 同名成员
	FD3D12Fence                Fence{ }; // 每队列一个 Fence（UE 也是）
};




