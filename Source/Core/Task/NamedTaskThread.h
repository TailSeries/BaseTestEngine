#pragma once
#include "Core.h"
#include "TaskThreadBase.h"
#include "StallingTaskQueue.h"



class FNamedTaskThread : public FTaskThreadBase
{
public:

	struct FThreadTaskQueue
	{
		//todo 我们还没有无锁结构，FStallingTaskQueue 我们先直接用锁 + std::priority_queue 来实现
		FStallingTaskQueue<FBaseGraphTask, 2> StallQueue; // 内部先只用两个队列吧

		/*
		 * 某些任务执行时可能会间接触发新的任务入队；
		 * 为了防止当前线程再次进入自身的任务处理循环（死递归或重复执行），用这个变量做“守卫”；
		 * 处理任务开始的时候 ++，离开处理函数的时候--，不为0的时候就说明在处理任务
		 */
		uint32 RecursionGuard;

		//前执行的任务是一个“return to pool”的特殊任务，即当前线程可以归还；
		bool QuitForReturn;
		//当前执行的任务是一个“shutdown”指令，即线程要永久退出，整个线程池可能要销毁了。
		bool QuitForShutdown;

		//用于线程在“没有任务可执行”时，挂起自己、等待被唤醒
		FEvent* StallRestartEvent;

		/*
		 * 典型流程应该是这样的：
		 * 1.消费者线程进入循环，从队列里 Pop()；
		 * 2.如果没有任务，则调用 StallRestartEvent->Wait() 进入等待状态；
		 * 3.当生产者线程 Push() 了任务并发现有等待线程（通过 MasterState 位图），就会调用 StallRestartEvent->Trigger() 唤醒对应线程。
		 * 每个 FQueuedThread 都有一个自己的 StallRestartEvent，它代表当前线程的“等待句柄”。
		 */
		FThreadTaskQueue()
			: RecursionGuard(0)
			, QuitForReturn(false)
			, QuitForShutdown(false)
			, StallRestartEvent(FPlatformProcess::GetSynchEventFromPool(false))
		{

		}

		~FThreadTaskQueue()
		{
			FPlatformProcess::ReturnSynchEventToPool(StallRestartEvent);
			StallRestartEvent = nullptr;
		}
	};


	virtual void EnqueueFromThisThread(int32 QueueIndex, FBaseGraphTask* Task) override;
	virtual bool EnqueueFromOtherThread(int32 QueueIndex, FBaseGraphTask* Task) override;


	virtual void ProcessTasksUntilQuit(int32 QueueIndex) override;
	virtual uint64 ProcessTasksUntilIdle(int32 QueueIndex) override;
	// 真正分发任务给线程处理
	uint64 ProcessTasksNamedThread(int32 QueueIndex, bool bAllowStall);


	virtual bool IsProcessingTasks(int32 QueueIndex) override;

	/**
	 * 请求线程退出任务循环
	 * 你不应该在任意时机调用这个函数，尤其是线程可能还在处理任务时。不应该在任务线程仍然活跃（有任务在跑）时调用 RequestQuit()。
	 * 因为 RequestQuit() 是一个软退出机制，它只是设置了 QuitForShutdown / QuitForReturn 的标志位，并试图唤醒线程。
	 * 如果线程正在忙（处理中）而不是等待 StallRestartEvent，这个“退出请求”可能会被忽略或延迟响应
	 * @param QueueIndex 
	 */
	virtual void RequestQuit(int32 QueueIndex) override;

	virtual void WakeUp(int32 QueueIndex) override;

protected:
	FThreadTaskQueue& Queue(int32 QueueIndex);
	const FThreadTaskQueue& Queue(int32 QueueIndex) const;

private:
	// 每个命名线程有两个队列, 每条队列存储的都是 FBaseGraphTask*
	FThreadTaskQueue Queues[ENamedThreads::NumQueues];
};