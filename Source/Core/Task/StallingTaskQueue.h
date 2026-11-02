#pragma once

#include <atomic>
#include <cassert>
#include <new>
#include <boost/lockfree/queue.hpp>

#include "Core.h"

// 每个优先级一条 MPMC FIFO，0 为最高任务优先级。
// 只保存任务指针；任务生命周期由调用方管理。构造/析构时不得有并发访问。
// 预分配池内的操作无锁；池耗尽后允许分配内存，该慢路径可能阻塞。
template<typename T, int32 NumPriorities, uint32 InitialCapacityPerPriority = 1024>
class FStallingTaskQueue : public FNoncopyable
{
	static_assert(NumPriorities > 0);
public:
	// 成功发布任务后返回一个已认领的空闲线程编号，调用方负责 Trigger 其 Event。
	// -1 表示当前没有空闲线程；不是入队失败。
	int32 Push(T* InPayload, uint32 Priority)
	{
		assert(InPayload != nullptr);
		assert(Priority < NumPriorities);
		auto& Tasks = PriorityQueues[Priority].Tasks;
		if (!Tasks.bounded_push(InPayload))
		{
			AllocationFallbackCount.fetch_add(1, std::memory_order_relaxed);
			// 入队失败不能伪装成“不需要唤醒线程”，否则任务会永久丢失。
			if (!Tasks.push(InPayload))
			{
				throw std::bad_alloc();
			}
		}

		// Invalidate a Pop that observed empty queues before publication.
		uint64 OldState = MasterState.load(std::memory_order_acquire);
		for (;;)
		{
			const int32 ThreadToWake = FindThreadToWake(OldState & IdleMask);
			const uint64 WakeBit = ThreadToWake >= 0 ? (uint64(1) << ThreadToWake) : 0;
			const uint64 NewState = ((OldState + GenerationStep) & ~IdleMask) |
				((OldState & IdleMask) & ~WakeBit);
			if (MasterState.compare_exchange_weak(OldState, NewState,
				std::memory_order_acq_rel, std::memory_order_acquire))
			{
				return ThreadToWake;
			}
		}
	}

	// ThreadId 是调用线程在本队列内的唯一编号 [0, 32)。
	// bAllowStall=true 且返回 nullptr 后，调用方应等待保留信号的 auto-reset Event。
	T* Pop(int32 ThreadId = 0, bool bAllowStall = false)
	{
		assert(ThreadId >= 0 && ThreadId < 32);
		const uint64 MyBit = uint64(1) << ThreadId;
		for (;;)
		{
			const uint64 ObservedState = MasterState.load(std::memory_order_acquire);
			// WakeUp/StartAllTaskThreads 也可能不经过 Push 直接触发 Event。
			// 调用 Pop 的线程已在运行，先撤销自己的空闲登记。
			if (ObservedState & MyBit)
			{
				uint64 Expected = ObservedState;
				const uint64 RunningState = ((ObservedState + GenerationStep) & ~IdleMask) |
					((ObservedState & IdleMask) & ~MyBit);
				MasterState.compare_exchange_weak(Expected, RunningState,
					std::memory_order_acq_rel, std::memory_order_acquire);
				continue;
			}
			for (int32 Index = 0; Index < NumPriorities; ++Index)
			{
				T* Result = nullptr;
				if (PriorityQueues[Index].Tasks.pop(Result))
				{
					return Result;
				}
			}

			if (!bAllowStall)
			{
				return nullptr;
			}

			// Retry if a producer published work while we checked the queues.
			uint64 Expected = ObservedState;
			const uint64 NewState = ((ObservedState + GenerationStep) & ~IdleMask) |
				((ObservedState & IdleMask) | MyBit);
			if (MasterState.compare_exchange_weak(Expected, NewState,
				std::memory_order_acq_rel, std::memory_order_acquire))
			{
				return nullptr;
			}
		}
	}

	// 检查原子实现，不包含扩容分配器的行为。
	bool IsLockFree() const
	{
		if (!MasterState.is_lock_free() || !AllocationFallbackCount.is_lock_free())
			return false;
		for (const auto& Queue : PriorityQueues)
			if (!Queue.Tasks.is_lock_free())
				return false;
		return true;
	}

	// 进入允许分配的慢路径次数；竞争中节点可能刚被归还，因此不等于实际分配次数。
	uint64 GetAllocationFallbackCount() const
	{
		return AllocationFallbackCount.load(std::memory_order_relaxed);
	}

private:
	// 低 32 位为空闲线程位图，高 32 位为代数，任何入队/空闲登记变化都推进代数。
	// 与有限宽度的 ABA 标签一样，要求单次操作期间代数不能完整回绕。
	static constexpr uint64 IdleMask = 0xffffffffull;
	static constexpr uint64 GenerationStep = uint64(1) << 32;

	static int32 FindThreadToWake(uint64 IdleBits)
	{
		if (IdleBits == 0)
		{
			return -1;
		}
		int32 Index = 0;
		while ((IdleBits & 1) == 0)
		{
			IdleBits >>= 1;
			++Index;
		}
		return Index;
	}

	struct FPriorityQueue
	{
		boost::lockfree::queue<T*> Tasks{InitialCapacityPerPriority};
	};

	alignas(64) std::atomic<uint64> MasterState{0};
	alignas(64) std::atomic<uint64> AllocationFallbackCount{0};
	FPriorityQueue PriorityQueues[NumPriorities];
};
