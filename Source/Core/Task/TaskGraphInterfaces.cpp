#include "TaskGraphInterfaces.h"

#include "NullGraphTask.h"
#include "TaskGraphImplementation.h"

COREMODULE bool GRenderThreadPollingOn = false;	// Access/Modify on GT only. This value is set on the GT before actual state is changed on the RT.
COREMODULE int32 GRenderThreadPollPeriodMs = -1;	// Access/Modify on RT only.
namespace ENamedThreads
{
	COREMODULE int32 bHasBackgroundThreads = CREATE_BACKGROUND_TASK_THREADS;
	COREMODULE int32 bHasHighPriorityThreads = CREATE_HIPRI_TASK_THREADS;
}




#pragma region FTaskGraphInterface

FTaskGraphInterface& FTaskGraphInterface::Get()
{
	return FTaskGraphImplementation::Get();
}

FTaskGraphInterface::~FTaskGraphInterface()
{
}

void FTaskGraphInterface::Startup(int32 NumThreads)
{
	new FTaskGraphImplementation();
}

void FTaskGraphInterface::Shutdown()
{
	FTaskGraphImplementation::Get().Desconstruct();
}

bool FTaskGraphInterface::IsRunning()
{
	return FTaskGraphImplementation::GetSingletonPtr() != nullptr;
}

bool FTaskGraphInterface::IsMultithread()
{
	return FPlatformProcess::SupportsMultithreading();
}

void FTaskGraphInterface::BroadcastSlow_OnlyUseForSpecialPurposes(bool bDoTaskThreads, bool bDoBackgroundThreads, TFunction<void(ENamedThreads::Type CurrentThread)>& Callback)
{

}




#pragma endregion FTaskGraphInterface


#pragma region FGraphEvent
//todo 我们还没实现TLockFreeClassAllocator_TLSCache 无锁线程本地分配器，我们先直接使用make_shared
FGraphEventRef FGraphEvent::CreateGraphEvent()
{
	return std::make_shared<FGraphEvent>();
}

FGraphEvent* FGraphEvent::CreateGraphEventWithInlineStorage()
{
	return new FGraphEvent();
}

void FGraphEvent::DispatchSubsequents(ENamedThreads::Type CurrentThreadIfKnown)
{
	std::vector<FBaseGraphTask*> NewTasks;
	DispatchSubsequents(NewTasks, CurrentThreadIfKnown);
}
static int32 GTestDontCompleteUntilForAlreadyComplete = 1;
static int32 GIgnoreThreadToDoGatherOn = 0;
void FGraphEvent::DispatchSubsequents(std::vector<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThreadIfKnown)
{
	if (!EventsToWaitFor.empty())
	{
		FGraphEventArray TempEventsToWaitFor;
		EventsToWaitFor.swap(TempEventsToWaitFor);
		bool bSpawnGatherTask = true;

		if (GTestDontCompleteUntilForAlreadyComplete)
		{
			bSpawnGatherTask = false;
			// 所有后续任务中只要有一个任务没完成 bSpawnGatherTask = true
			for (auto& Item: TempEventsToWaitFor)
			{
				if (!Item->IsComplete())
				{
					bSpawnGatherTask = true;
					break;
				}
			}
		}
		if (bSpawnGatherTask)
		{
			/* 这里用一个空任务 FNullGraphTask 来等待 TempEventsToWaitFor 中所有事件完成
			 * 这个“聚合任务”本质是个 barrier，当它完成时，当前任务才能真正完成。
			 * 这样保证了当前任务延迟完成的语义。
			 */
			ENamedThreads::Type LocalThreadToDoGatherOn = ENamedThreads::AnyHiPriThreadHiPriTask;

			if (!GIgnoreThreadToDoGatherOn)
			{
				LocalThreadToDoGatherOn = ThreadToDoGatherOn;
			}
			
			TGraphTask<FNullGraphTask>::CreateTask(shared_from_this(), &TempEventsToWaitFor, CurrentThreadIfKnown).ConstructAndDispatchWhenReady(LocalThreadToDoGatherOn);
			return;
		}
	}

	// 没有前置事件了
	{
		FRWScopeLock<SLT_Write> ScopeLock(SubsequentListLock);
		NewTasks.insert(NewTasks.end(), SubsequentList.begin(), SubsequentList.end());
		SubsequentList.clear();
		IsClosed.store(true);
	}
	for (int32 Index = NewTasks.size() - 1; Index >= 0; Index--) 
	{
		FBaseGraphTask* NewTask = NewTasks[Index];
		NewTask->ConditionalQueueTask(CurrentThreadIfKnown);
	}
	NewTasks.clear();
}

void FGraphEvent::Recycle(FGraphEvent* ToRecycle)
{
	//todo 我们没有分配器，先直接delete
	delete ToRecycle;
}

FGraphEvent::~FGraphEvent()
{
	if (!IsComplete())
	{
		assert(IsClosed);
	}
	// 析构的时候，前置事件列表应该全部完成了
	CheckDontCompleteUntilIsEmpty(); //  
}
#pragma endregion FGraphEvent
