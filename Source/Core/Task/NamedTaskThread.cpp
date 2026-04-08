#include "NamedTaskThread.h"

extern COREMODULE bool GRenderThreadPollingOn;
extern COREMODULE int32 GRenderThreadPollPeriodMs;

void FNamedTaskThread::EnqueueFromThisThread(int32 QueueIndex, FBaseGraphTask* Task)
{
	uint32 PriIndex = ENamedThreads::GetTaskPriority(Task->ThreadToExecuteOn) ? 0 : 1;
	int32 ThreadToStart = Queue(QueueIndex).StallQueue.Push(Task, PriIndex);
	assert(ThreadToStart < 0);
}

bool FNamedTaskThread::EnqueueFromOtherThread(int32 QueueIndex, FBaseGraphTask* Task)
{
	uint32 PriIndex = ENamedThreads::GetTaskPriority(Task->ThreadToExecuteOn) ? 0 : 1;
	int32 ThreadToStart = Queue(QueueIndex).StallQueue.Push(Task, PriIndex);
	if (ThreadToStart >= 0)
	{
		Queue(QueueIndex).StallRestartEvent->Trigger();
		return true;

	}
	return false;
}

void FNamedTaskThread::ProcessTasksUntilQuit(int32 QueueIndex)
{
	Queue(QueueIndex).QuitForReturn = false;
	const bool bIsMultiThread = FTaskGraphInterface::IsMultithread();
	verify(++Queue(QueueIndex).RecursionGuard == 1);// 一个线程不应该重入自己的调用循环
	do
	{
		const bool bAllowStall = bIsMultiThread;
		ProcessTasksNamedThread(QueueIndex, bAllowStall);
	} while (!Queue(QueueIndex).QuitForReturn && !Queue(QueueIndex).QuitForShutdown && bIsMultiThread); 
	verify(!--Queue(QueueIndex).RecursionGuard);
}

uint64 FNamedTaskThread::ProcessTasksUntilIdle(int32 QueueIndex)
{
	Queue(QueueIndex).QuitForReturn = false;
	verify(++Queue(QueueIndex).RecursionGuard == 1);// 一个线程不应该重入自己的调用循环
	uint64 ProcessedTasks = ProcessTasksNamedThread(QueueIndex, false);
	verify(!--Queue(QueueIndex).RecursionGuard);
	return ProcessedTasks;
}

uint64 FNamedTaskThread::ProcessTasksNamedThread(int32 QueueIndex, bool bAllowStall)
{
	uint64 ProcessedTasks = 0;
	bool bCountAsStall = false;
	// 渲染线程有特殊处理
	const bool bIsRenderThreadMainQueue = (ENamedThreads::GetThreadIndex(ThreadId) == ENamedThreads::ActualRenderingThread) && (QueueIndex == 0);
	while (!Queue(QueueIndex).QuitForReturn)
	{
		const bool bIsRenderThreadAndPolling = bIsRenderThreadMainQueue && (GRenderThreadPollPeriodMs >= 0);
		const bool bStallQueueAllowStall = bAllowStall && !bIsRenderThreadAndPolling;
		FBaseGraphTask* Task = Queue(QueueIndex).StallQueue.Pop(0, bStallQueueAllowStall);
		if (!Task)
		{
			if (bAllowStall)
			{
				// 没东西让线程处理了，我们让线程休眠在这里
				Queue(QueueIndex).StallRestartEvent->Wait(bIsRenderThreadAndPolling ? GRenderThreadPollPeriodMs : MAX_uint32, bCountAsStall);
				if (Queue(QueueIndex).QuitForShutdown)
				{
					return ProcessedTasks;
				}
				continue;
			}
			else
			{
				//我们不需要线程stall的话，我们直接 break
				break;
			}
		}
		else
		{
			Task->Execute(NewTasks, static_cast<ENamedThreads::Type>(ThreadId | (QueueIndex << ENamedThreads::QueueIndexShift)));
			ProcessedTasks++;
		}
	}
	return ProcessedTasks;
}

bool FNamedTaskThread::IsProcessingTasks(int32 QueueIndex)
{
	// 处理任务的时候这个值会++ 离开任务的时候这个值会--
	return !!Queue(QueueIndex).RecursionGuard;
}


void FNamedTaskThread::WakeUp(int32 QueueIndex)
{
	// 唤醒一个线程来处理队列里的内容
	Queue(QueueIndex).StallRestartEvent->Trigger();
}


void FNamedTaskThread::RequestQuit(int32 QueueIndex)
{
	if (!Queue(0).StallRestartEvent)
	{
		/*
		 * 如果当前队列的 StallRestartEvent 是 nullptr，说明线程系统还没完全初始化，这个线程可能尚未启动或处于未就绪状态。
		 * 在这种情况下，调用 RequestQuit() 没有意义，也不安全。
		 * 为什么判断 Queue(0) 就可以，因为 塞进任务至少有一个 Queue(0)
		 */
		return;
	}
	if (QueueIndex == -1)
	{
		//QueueIndex == -1 的时候我们认为这是在关机
		Queue(0).QuitForShutdown = true;
		Queue(1).QuitForShutdown = true;
		Queue(0).StallRestartEvent->Trigger();
		Queue(1).StallRestartEvent->Trigger();
	}
	else
	{
		Queue(QueueIndex).QuitForReturn = true;
	}

}

FNamedTaskThread::FThreadTaskQueue& FNamedTaskThread::Queue(int32 QueueIndex)
{
	return Queues[QueueIndex];
}

const FNamedTaskThread::FThreadTaskQueue& FNamedTaskThread::Queue(int32 QueueIndex) const
{
	return Queues[QueueIndex];
}
