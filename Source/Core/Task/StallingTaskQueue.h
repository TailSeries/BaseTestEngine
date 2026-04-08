#pragma once
#include <queue>

#include "Core.h"
#include "Threads/ScopeLock.h"
//todo 	我们还没有无锁结构，FStallingTaskQueue 我们先直接用锁 + std::priority_queue 来实现
/**
 *我们的规则是这样的：
 *一共 4 +  26 * 3 = 82 个线程，前面4个是namedthread.那么 4 ~  29  >> 0 ; 30 ~ 55 >> 1; 56 ~ 79 >> 2
 *0 1 2 是三个不同等级的thread，每一组set有一个queue，可以认为其一共可以操作的线程编号均为 0 ~ 26. 这个数值叠*priority + namedthread 就可以得到原来的线程编号了。
 *在ue原来的操作用，用bit位来记录当前究竟那个线程在操作queue，尤其是pop的时候传入了当前的thread，pop之后，这个线程就是挂起状态了
 *
 * @tparam T
 * @tparam NumPriorities 内部不同优先度的队列数量
 */
template<typename T, int32 NumPriorities>
class FStallingTaskQueue : public FNoncopyable
{
public:
	FStallingTaskQueue() {}
	int32 Push(T* InPayload, uint32 Priority)
	{

		{
			FRWScopeLock<SLT_Write> ScopeLock(PriorityQueueLocks[Priority]);
			PriorityQueues[Priority].push(InPayload);
		}

		int32 threadToWake = FindThreadToWake(masterState.load(std::memory_order_acquire));
		if (threadToWake >= 0) {
			TurnOffBit(threadToWake);
		}
		return threadToWake;
	}

	T* Pop(int32 threadId = 0, bool bAllowStall = false)
	{
		// 遍历所有的队列，直接拿出一个能用的
		for (int32 Index = 0; Index < NumPriorities; Index++)
		{
			{
				FRWScopeLock<SLT_Write> ScopeLock(PriorityQueueLocks[Index]);
				if (PriorityQueues[Index].size() > 0)
				{
					auto& Result = PriorityQueues[Index].top();
					PriorityQueues[Index].pop();
					return Result;
				}
			}

		}
		if (!bAllowStall) return nullptr;
		// 没任务，标记自己为空闲
		TurnOnBit(threadId);
		return nullptr;
	}



private:

	static int32 FindThreadToWake(uint64 state)
	{
		if (state == 0) return -1;
		int32 idx = 0;
		while ((state & 1) == 0) {
			state >>= 1;
			++idx;
		}
		return idx;
	}
	void TurnOnBit(int32 ThreadID)
	{
		uint64 OldState = masterState.load();
		uint64 NewState;
		do
		{
			NewState = OldState | uint64(1) << ThreadID;
		}
		while (masterState.compare_exchange_weak(OldState, NewState));
	}


	void TurnOffBit(int32 ThreadID)
	{
		uint64 OldState = masterState.load();
		uint64 NewState;
		do
		{
			NewState = OldState &  ~(uint64(1) << ThreadID);
		}
		while (masterState.compare_exchange_weak(OldState, NewState));
	}




private:
	// 每个bit位 1 代表该线程处在空闲状态 
	std::atomic<uint64>  masterState;
	std::priority_queue<T*> PriorityQueues[NumPriorities];
	FRWLock PriorityQueueLocks[NumPriorities];
};