#include "AnyTaskThread.h"

#include "TaskGraphImplementation.h"

FTaskThreadAnyThread::FTaskThreadAnyThread(int32 InPriorityIndex)
	:PriorityIndex(InPriorityIndex)
{

}

void FTaskThreadAnyThread::ProcessTasksUntilQuit(int32 QueueIndex)
{
	const bool bIsMultiThread = FTaskGraphInterface::IsMultithread();
	do
	{
		ProcessTasks();
	} while (!Queue.QuitForShutdown && bIsMultiThread);
}

uint64 FTaskThreadAnyThread::ProcessTasksUntilIdle(int32 QueueIndex)
{
	// 奇了怪了，AnyThread下这个函数不允许运行在多线程下
	if (FTaskGraphInterface::IsMultithread() == false)
	{
		return ProcessTasks();
	}
	else
	{
		assert(0);
		return 0;
	}
}

uint64 FTaskThreadAnyThread::ProcessTasks()
{
	bool bCountAsStall = true;
	uint64 ProcessedTasks = 0;
	verify(++Queue.RecursionGuard == 1);
	bool bDidStall = false;
	while (true)
	{
		FBaseGraphTask* Task = FindWork();
		if (!Task)
		{
			const bool bIsMultithread = FTaskGraphInterface::IsMultithread();
			if (bIsMultithread)
			{
				// 找不到任务的时候我们就让线程等待直到可以找到任务来处理
				Queue.StallRestartEvent->Wait(MAX_uint32, bCountAsStall);
				bDidStall = true;
			}

			if (Queue.QuitForShutdown || !bIsMultithread)
			{
				// 队列要求退出线程的话，比如关机流程
				break;
			}

			continue;
		}

		/*
		 * Windows 的线程调度器有时候行为不佳，会让低优先级的后台线程（BackgroundThreadPriority）继续运行，即使其他高优线程已经准备好。
		 * 我们这里手动让出时间片 如果线程是“后台优先级”，且本轮没有“挂起等待任务”，就主动让出 CPU 给其他线程。
		 */
		if (!bDidStall && PriorityIndex == (ENamedThreads::BackgroundThreadPriority >> ENamedThreads::ThreadPriorityShift))
		{
			FPlatformProcess::Sleep(0);
		}
		bDidStall = false;
		Task->Execute(NewTasks, ENamedThreads::Type(ThreadId));
		ProcessedTasks++;
	}
	verify(!--Queue.RecursionGuard);
	return ProcessedTasks;
}

void FTaskThreadAnyThread::RequestQuit(int32 QueueIndex)
{
	Queue.QuitForShutdown = true;
	Queue.StallRestartEvent->Trigger();
}

void FTaskThreadAnyThread::WakeUp(int32 QueueIndex)
{
	Queue.StallRestartEvent->Trigger();
}

void FTaskThreadAnyThread::StallForTuning(bool Stall)
{
	// todo 我们暂时没有用于stat的东西在
	/*if (Stall)
	{
		Queue.StallForTuning.Lock();
		Queue.bStallForTuning = true;
	}
	else
	{
		Queue.bStallForTuning = false;
		Queue.StallForTuning.Unlock();
	}*/
}

bool FTaskThreadAnyThread::IsProcessingTasks(int32 QueueIndex)
{
	return !!Queue.RecursionGuard;
}

FBaseGraphTask* FTaskThreadAnyThread::FindWork()
{
	return FTaskGraphImplementation::Get().FindWork(ThreadId);
}
