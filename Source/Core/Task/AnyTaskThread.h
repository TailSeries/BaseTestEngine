#pragma once
#include "Core.h"
#include "TaskThreadBase.h"

/**
*	用于管理 “任意线程”（AnyThread）池中的工作线程。这是多线程任务系统中执行非指定线程任务（即 ENamedThreads::AnyThread）的关键执行体。
**/
class FTaskThreadAnyThread : public FTaskThreadBase
{
public:
	struct FThreadTaskQueue
	{
		/*	当前线程没有可执行任务时，线程会等待这个事件；任务加入队列时触发它唤醒线程。*/
		FEvent* StallRestartEvent;
		/*用于防止线程重入执行任务（例如任务中递归触发任务执行）。非零表示当前处于任务处理中。*/
		uint32 RecursionGuard;
		/*如果为 true，表示线程要退出执行循环，用于关机流程中终止线程*/
		bool QuitForShutdown;

		/*todo 如果调优开关打开，线程会主动阻塞，开发期间用于人工干预（通常仅在调试构建中使用）。和下面的锁 StallForTuning 配套使用，我们暂时没有分析*/
		//bool bStallForTuning;
		//FCriticalSection StallForTuning;

		FThreadTaskQueue()
			: StallRestartEvent(FPlatformProcess::GetSynchEventFromPool(false))
			, RecursionGuard(0)
			, QuitForShutdown(false)
			//, bStallForTuning(false)
		{

		}

		~FThreadTaskQueue()
		{
			FPlatformProcess::ReturnSynchEventToPool(StallRestartEvent);
			StallRestartEvent = nullptr;
		}
	};


	FTaskThreadAnyThread(int32 InPriorityIndex);

	virtual void ProcessTasksUntilQuit(int32 QueueIndex) override;
	virtual uint64 ProcessTasksUntilIdle(int32 QueueIndex) override;

	/**
	 * 	处理当前队列的任务
	 * @return 
	 */
	uint64 ProcessTasks();

	virtual void RequestQuit(int32 QueueIndex) override;
	virtual void WakeUp(int32 QueueIndex = 0) final override;
	void StallForTuning(bool Stall);
	virtual bool IsProcessingTasks(int32 QueueIndex) override;
private:
	/*
	 * 为当前线程查找一个待执行的任务
	 */
	FBaseGraphTask* FindWork();
	// 单一的任务队列对象
	FThreadTaskQueue Queue;
	int32 PriorityIndex;

};