#include "TaskGraphImplementation.h"

#include "AnyTaskThread.h"
#include "NamedTaskThread.h"
#include "ReturnGraphTask.h"
#include "TriggerEventGraphTask.h"
#include "Common/LogAssert.h"
static FTaskGraphImplementation* TaskGraphImplementationSingleton = nullptr;
FTaskGraphImplementation& FTaskGraphImplementation::Get()
{
	return *TaskGraphImplementationSingleton;
}

FTaskGraphImplementation* FTaskGraphImplementation::GetSingletonPtr()
{
	return TaskGraphImplementationSingleton;
}

void FTaskGraphImplementation::Desconstruct()
{
	delete TaskGraphImplementationSingleton;
	TaskGraphImplementationSingleton = nullptr;
}

FTaskGraphImplementation::FTaskGraphImplementation()
{
	int32 MaxTaskThreads = MAX_THREADS;
	bCreatedHiPriorityThreads = !!ENamedThreads::bHasHighPriorityThreads;
	bCreatedBackgroundPriorityThreads = !!ENamedThreads::bHasBackgroundThreads;
	// 获取可以工作的线程数量 2 ~ 26个
	int32 NumTaskThreads = FPlatformProcess::NumberOfWorkerThreadsToSpawn();

	/*
	 * 如果不启用多线程（如启动参数里禁用了或者处于 -onethread 模式）：
	 *	只创建一个线程（通常是主线程）
	 *		把 LastExternalThread 设置为 ActualRenderingThread - 1，表示没有渲染线程
	 */
	if (!FTaskGraphInterface::IsMultithread())
	{
		MaxTaskThreads = 1;
		NumTaskThreads = 1;
		LastExternalThread = (ENamedThreads::Type)(ENamedThreads::ActualRenderingThread - 1);
		bCreatedHiPriorityThreads = false;
		bCreatedBackgroundPriorityThreads = false;
		ENamedThreads::bHasBackgroundThreads = 0;
		ENamedThreads::bHasHighPriorityThreads = 0;
	}
	else
	{
		LastExternalThread = ENamedThreads::ActualRenderingThread;
		// todo 处理fork的情况，太麻烦了，我觉得我自己的设计一定屏蔽fork

	}
	// 具名线程的数量
	NumNamedThreads = LastExternalThread + 1;
	// 不同优先级的线程集的数量
	NumTaskThreadSets = 1 + bCreatedHiPriorityThreads + bCreatedBackgroundPriorityThreads;
	// 实际创建的线程数量：正常应该就是 NumTaskThreads * NumTaskThreadSets + NumNamedThreads 了
	NumThreads = std::max(std::min<int32>(NumTaskThreads * NumTaskThreadSets + NumNamedThreads, MAX_THREADS), NumNamedThreads + 1);
	NumThreads = std::min(NumThreads, NumNamedThreads + NumTaskThreads * NumTaskThreadSets);

	// 每个优先级线程集中的线程数量 基本就是 NumTaskThreads
	NumTaskThreadsPerSet = (NumThreads - NumNamedThreads) / NumTaskThreadSets;
	LogStringMsg("Started task graph with %d named threads and %d total threads with %d sets of task threads.", NumNamedThreads, NumThreads, NumTaskThreadSets);
	// 检查是否重入
	assert(ReentrancyCheck.load(std::memory_order_relaxed) == 0);
	ReentrancyCheck.fetch_add(1, std::memory_order_relaxed);
	PerThreadIDTLSSlot = FPlatformTLS::AllocTlsSlot();


	// 创建线程执行逻辑对象
	for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ThreadIndex++)
	{
		bool bAnyTaskThread = ThreadIndex >= NumNamedThreads;
		// 分类创建匿名线程和具名线程
		if (bAnyTaskThread)
		{
			// 实际用来执行线程任务的逻辑对象
			WorkerThreads[ThreadIndex].TaskGraphWorker = new FTaskThreadAnyThread(ThreadIndexToPriorityIndex(ThreadIndex));
		}
		else
		{
			WorkerThreads[ThreadIndex].TaskGraphWorker = new FNamedTaskThread();
		}

		WorkerThreads[ThreadIndex].TaskGraphWorker->Setup(ENamedThreads::Type(ThreadIndex), PerThreadIDTLSSlot, &WorkerThreads[ThreadIndex]);
	}


	TaskGraphImplementationSingleton = this;

	//创建线程对象
	/*我们这里的优先级和windows定义的相比弱一点，都是在TPri_Normal 之下的
	 *	Hi >> TPri_SlightlyBelowNormal
	 *	Normal >> TPri_BelowNormal
	 *	low >> TPri_Lowest
	 */
	const char* PrevGroupName = nullptr;
	for (int32 ThreadIndex = LastExternalThread + 1; ThreadIndex < NumThreads; ThreadIndex ++)
	{
		FString Name;
		const char* GroupName = "TaskGraphNormal";
		int32 Priority = ThreadIndexToPriorityIndex(ThreadIndex);
		EThreadPriority ThreadPri;
		// windows上返回 0xFFFFFFFFFFFFFFFF，在window上，我们不必约束大小核
		uint64 Affinity = FPlatformProcess::GetTaskGraphThreadMask();
		if (Priority == 1)
		{
			Name = std::format("TaskGraphThreadHP {}", ThreadIndex - (LastExternalThread + 1));
			GroupName = "TaskGraphHigh";
			ThreadPri = TPri_SlightlyBelowNormal;
			// If the platform defines FPlatformAffinity::GetTaskGraphHighPriorityTaskMask then use it
			if (FPlatformProcess::GetTaskGraphHighPriorityTaskMask() != 0xFFFFFFFFFFFFFFFF)
			{
				Affinity = FPlatformProcess::GetTaskGraphHighPriorityTaskMask();
			}

		}
		else if (Priority == 2)
		{
			Name = std::format("TaskGraphThreadBP {}", ThreadIndex - (LastExternalThread + 1));
			GroupName = "TaskGraphLow";
			ThreadPri = TPri_Lowest;

			if (FPlatformProcess::GetTaskGraphBackgroundTaskMask() != 0xFFFFFFFFFFFFFFFF)
			{
				Affinity = FPlatformProcess::GetTaskGraphBackgroundTaskMask();
			}
		}
		else
		{
			Name = std::format("TaskGraphThreadNP {}", ThreadIndex - (LastExternalThread + 1));
			GroupName = "TaskGraphNormal";
			ThreadPri = TPri_BelowNormal;
		}

		int32 StackSize = 1024 * 1024;
		if (GroupName != PrevGroupName)
		{
			PrevGroupName = GroupName;
		}


		//todo 当前是不是可fork的，我们后续跨平台的时候来设计fork的功能
		//if (FForkProcessHelper::IsForkedMultithreadInstance() && GAllowTaskGraphForkMultithreading)
		//{
		//	WorkerThreads[ThreadIndex].RunnableThread = FForkProcessHelper::CreateForkableThread(&Thread(ThreadIndex), *Name, StackSize, ThreadPri, Affinity);
		//}
		//else
		{
			// 创建真正的线程类对象
			WorkerThreads[ThreadIndex].RunnableThread = FRunnableThread::Create(&Thread(ThreadIndex), Name.c_str(), StackSize, ThreadPri, Affinity);
		}
		// 这个线程创建完毕，我们就认为这个线程已经被Attach进来了
		WorkerThreads[ThreadIndex].bAttached = true;
	}

}

FTaskGraphImplementation::~FTaskGraphImplementation()
{
	for (auto& Callback:ShutdownCallbacks)
	{
		Callback();
	}

	ShutdownCallbacks.clear();
	for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ThreadIndex++)
	{
		Thread(ThreadIndex).RequestQuit(-1);
	}

	//具名线程，我们要wait
	for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ThreadIndex++)
	{
		if (ThreadIndex > LastExternalThread)
		{
			WorkerThreads[ThreadIndex].RunnableThread->WaitForCompletion();
			delete WorkerThreads[ThreadIndex].RunnableThread;
			WorkerThreads[ThreadIndex].RunnableThread = nullptr;
		}
		WorkerThreads[ThreadIndex].bAttached = false;
	}
	TaskGraphImplementationSingleton = nullptr;
	NumTaskThreadsPerSet = 0;
	FPlatformTLS::FreeTlsSlot(PerThreadIDTLSSlot);
}

void FTaskGraphImplementation::QueueTask(FBaseGraphTask* Task, ENamedThreads::Type ThreadToExecuteOn, ENamedThreads::Type InCurrentThreadIfKnown)
{
	if (ENamedThreads::GetThreadIndex(ThreadToExecuteOn) == ENamedThreads::AnyThread)
	{
		// 期望执行的线程类型是AnyThread
		if (FPlatformProcess::SupportsMultithreading())
		{
			uint32 TaskPriority = ENamedThreads::GetTaskPriority(Task->ThreadToExecuteOn);
			int32 Priority = ENamedThreads::GetThreadPriorityIndex(Task->ThreadToExecuteOn);
			if (Priority == (ENamedThreads::BackgroundThreadPriority >> ENamedThreads::ThreadPriorityShift) && (!bCreatedBackgroundPriorityThreads || !ENamedThreads::bHasBackgroundThreads))
			{
				// 没有background threads的时候直接给他们转到normal,注意taskpriority也转到normal
				Priority = ENamedThreads::NormalThreadPriority >> ENamedThreads::ThreadPriorityShift;
				TaskPriority = ENamedThreads::NormalTaskPriority >> ENamedThreads::TaskPriorityShift;
			}
			else if (Priority == (ENamedThreads::HighThreadPriority >> ENamedThreads::ThreadPriorityShift) && (!bCreatedHiPriorityThreads || !ENamedThreads::bHasHighPriorityThreads))
			{
				// 没有hi threads的时候我们直接给它们转到 normal，注意taskpriority应该转到hi
				Priority = ENamedThreads::NormalThreadPriority >> ENamedThreads::ThreadPriorityShift;
				TaskPriority = ENamedThreads::HighTaskPriority >> ENamedThreads::TaskPriorityShift;
			}

			uint32 PriIndex = TaskPriority ? 0 : 1;
			int32 IndexToStart = IncomingAnyThreadTasks[Priority].Push(Task, PriIndex);
			if (IndexToStart >=0)
			{
				StartTaskThread(Priority, IndexToStart);

			}
			return;
		}
		else
		{
			ThreadToExecuteOn = ENamedThreads::GameThread;
		}

	}

	// 如果是具名线程
	ENamedThreads::Type CurrentThreadIfKnown;
	if (ENamedThreads::GetThreadIndex(InCurrentThreadIfKnown) == ENamedThreads::AnyThread)
	{
		// 如果进来的线程标记就是AnyThread，我们通过TLS获取当前的线程类型
		CurrentThreadIfKnown = GetCurrentThread();
	}
	else
	{
		CurrentThreadIfKnown = ENamedThreads::GetThreadIndex(InCurrentThreadIfKnown);
	}

	{
		// 将任务
		int32 QueueToExecuteOn = ENamedThreads::GetQueueIndex(ThreadToExecuteOn);
		ThreadToExecuteOn = ENamedThreads::GetThreadIndex(ThreadToExecuteOn);
		FTaskThreadBase* Target = &Thread(ThreadToExecuteOn);
		if (ThreadToExecuteOn == ENamedThreads::GetThreadIndex(CurrentThreadIfKnown))
		{
			Target->EnqueueFromThisThread(QueueToExecuteOn, Task);
		}
		else
		{
			Target->EnqueueFromOtherThread(QueueToExecuteOn, Task);
		}
	}

}

void FTaskGraphImplementation::StartTaskThread(int32 Priority, int32 IndexToStart)
{
	ENamedThreads::Type ThreadToWake = static_cast<ENamedThreads::Type>(IndexToStart + Priority * NumTaskThreadsPerSet + NumNamedThreads);
	static_cast<FTaskThreadAnyThread&>(Thread(ThreadToWake)).WakeUp();
}

void FTaskGraphImplementation::StartAllTaskThreads(bool bDoBackgroundTask)
{
	for (int32 Index = 0; Index < GetNumWorkerThreads(); Index++)
	{
		for (int32 Priority = 0; Priority < ENamedThreads::NumThreadPriorities; Priority++)
		{
			if (Priority == (ENamedThreads::NormalThreadPriority >> ENamedThreads::ThreadPriorityShift) ||
				(Priority == (ENamedThreads::HighThreadPriority >> ENamedThreads::ThreadPriorityShift) && bCreatedHiPriorityThreads) ||
				(Priority == (ENamedThreads::BackgroundThreadPriority >> ENamedThreads::ThreadPriorityShift) && bCreatedBackgroundPriorityThreads && bDoBackgroundTask)
				)
			{
				StartTaskThread(Priority, Index);
			}
		}
	}
}

FBaseGraphTask* FTaskGraphImplementation::FindWork(ENamedThreads::Type ThreadInNeed)
{
	int32 LocalNumWorkingThread = GetNumWorkerThreads();
	int32 MyIndex = int32((uint32(ThreadInNeed) - NumNamedThreads) % NumTaskThreadsPerSet);
	int32 Priority = int32((uint32(ThreadInNeed) - NumNamedThreads) / NumTaskThreadsPerSet);
	return IncomingAnyThreadTasks[Priority].Pop(MyIndex, true);
}

void FTaskGraphImplementation::StallForTuning(int32 Index, bool Stall)
{
	for (int32 Priority = 0; Priority < ENamedThreads::NumThreadPriorities; Priority++)
	{
		ENamedThreads::Type ThreadToWake = ENamedThreads::Type(Index + Priority * NumTaskThreadsPerSet + NumNamedThreads);
		((FTaskThreadAnyThread&)Thread(ThreadToWake)).StallForTuning(Stall);
	}
}

void FTaskGraphImplementation::SetTaskThreadPriorities(EThreadPriority Pri)
{
	for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ThreadIndex++)
	{
		if (ThreadIndex > LastExternalThread)
		{
			WorkerThreads[ThreadIndex].RunnableThread->SetThreadPriority(Pri);
		}
	}
}

ENamedThreads::Type FTaskGraphImplementation::GetCurrentThread()
{
	ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread;
	FWorkerThread* TLSPointer = (FWorkerThread*)FPlatformTLS::GetTlsValue(PerThreadIDTLSSlot);
	if (TLSPointer)
	{
		// 当前TLS中记录了线程的地址，这个地址本身又在WorkerThreads数组中有记录，既然如此，这两个地址作差，即可得到数组偏移量
		int32 ThreadIndex = static_cast<int32>(TLSPointer - WorkerThreads);
		if (ThreadIndex < NumNamedThreads)
		{
			CurrentThreadIfKnown = ENamedThreads::Type(ThreadIndex);
		}
		else
		{
			int32 Priority = (ThreadIndex - NumNamedThreads) / NumTaskThreadsPerSet;
			CurrentThreadIfKnown = ENamedThreads::SetPriorities(ENamedThreads::Type(ThreadIndex), Priority, false);

		}
	}

	return CurrentThreadIfKnown;
}

FTaskThreadBase& FTaskGraphImplementation::Thread(int32 Index)
{
	return *WorkerThreads[Index].TaskGraphWorker;
}

int32 FTaskGraphImplementation::ThreadIndexToPriorityIndex(int32 ThreadIndex)
{
	int32 Result = (ThreadIndex - NumNamedThreads) / NumTaskThreadsPerSet;
	return Result;
}


int32 FTaskGraphImplementation::GetNumWorkerThreads()
{
	int32 Result = (NumThreads - NumNamedThreads) / NumTaskThreadSets;
	return Result;
}

ENamedThreads::Type FTaskGraphImplementation::GetCurrentThreadIfKnown(bool bLocalQueue)
{
	ENamedThreads::Type Result = GetCurrentThread();
	if (bLocalQueue && ENamedThreads::GetThreadIndex(Result) >= 0 && ENamedThreads::GetThreadIndex(Result) < NumNamedThreads)
	{
		Result = ENamedThreads::Type(int32(Result) | int32(ENamedThreads::LocalQueue));
	}
	return Result;
}

bool FTaskGraphImplementation::IsThreadProcessingTasks(ENamedThreads::Type ThreadToCheck)
{
	int32 QueueIndex = ENamedThreads::GetQueueIndex(ThreadToCheck);
	ThreadToCheck = ENamedThreads::GetThreadIndex(ThreadToCheck);
	return Thread(ThreadToCheck).IsProcessingTasks(QueueIndex);
}

void FTaskGraphImplementation::AttachToThread(ENamedThreads::Type CurrentThread)
{
	CurrentThread = ENamedThreads::GetThreadIndex(CurrentThread);
	Thread(CurrentThread).InitializeForCurrentThread();
}

uint64 FTaskGraphImplementation::ProcessThreadUntilIdle(ENamedThreads::Type CurrentThread)
{
	int32 QueueIndex = ENamedThreads::GetQueueIndex(CurrentThread);
	CurrentThread = ENamedThreads::GetThreadIndex(CurrentThread);
	return Thread(CurrentThread).ProcessTasksUntilIdle(QueueIndex);
}

void FTaskGraphImplementation::ProcessThreadUntilRequestReturn(ENamedThreads::Type CurrentThread)
{
	int32 QueueIndex = ENamedThreads::GetQueueIndex(CurrentThread);
	CurrentThread = ENamedThreads::GetThreadIndex(CurrentThread);
	assert(CurrentThread >= 0 && CurrentThread < NumNamedThreads);
	assert(CurrentThread == GetCurrentThread());
	Thread(CurrentThread).ProcessTasksUntilQuit(QueueIndex);
}

void FTaskGraphImplementation::RequestReturn(ENamedThreads::Type CurrentThread)
{
	int32 QueueIndex = ENamedThreads::GetQueueIndex(CurrentThread);
	CurrentThread = ENamedThreads::GetThreadIndex(CurrentThread);
	assert(CurrentThread != ENamedThreads::AnyThread);
	Thread(CurrentThread).RequestQuit(QueueIndex);
}

void FTaskGraphImplementation::WaitUntilTasksComplete(const FGraphEventArray& Tasks, ENamedThreads::Type CurrentThreadIfKnown)
{
	ENamedThreads::Type CurrentThread = CurrentThreadIfKnown;
	if (ENamedThreads::GetThreadIndex(CurrentThreadIfKnown) == ENamedThreads::AnyThread)
	{
		bool bIsHiPri = !!ENamedThreads::GetTaskPriority(CurrentThreadIfKnown);
		int32 Priority = ENamedThreads::GetThreadPriorityIndex(CurrentThreadIfKnown);
		assert(!ENamedThreads::GetQueueIndex(CurrentThreadIfKnown));
		CurrentThreadIfKnown = ENamedThreads::GetThreadIndex(GetCurrentThread());
		CurrentThread = ENamedThreads::SetPriorities(CurrentThreadIfKnown, Priority, bIsHiPri);
	}
	else
	{
		CurrentThreadIfKnown = ENamedThreads::GetThreadIndex(CurrentThreadIfKnown);
		assert(CurrentThreadIfKnown == ENamedThreads::GetThreadIndex(GetCurrentThread()));
	}

	// 具名线程，且没有在执行任务
	if (CurrentThreadIfKnown != ENamedThreads::AnyThread && CurrentThreadIfKnown < NumNamedThreads && !IsThreadProcessingTasks(CurrentThread))
	{
		if (Tasks.size() < 8)
		{
			bool bAnyPending = false;
			for (int32 Index = 0; Index < Tasks.size(); Index++)
			{
				FGraphEvent* Task = Tasks[Index].get();
				if (Task && !Task->IsComplete())
				{
					bAnyPending = true;
					break;
				}
			}
			if (!bAnyPending)
			{
				return;
			}
		}
		// 等待具名线程中的所有任务完成
		TGraphTask<FReturnGraphTask>::CreateTask(&Tasks, CurrentThread).ConstructAndDispatchWhenReady(CurrentThread);
		ProcessThreadUntilRequestReturn(CurrentThread);
	}
	else
	{
		if (!FTaskGraphInterface::IsMultithread())
		{
			bool bAnyPending = false;
			for (int32 Index = 0; Index < Tasks.size(); Index++)
			{
				FGraphEvent* Task = Tasks[Index].get();
				if (Task && !Task->IsComplete())
				{
					bAnyPending = true;
					break;
				}
			}
			if (!bAnyPending)
			{
				return;
			}
		}
		// 等待所有任务执行
		FScopedEvent Event;
		TriggerEventWhenTasksComplete(Event.Get(), Tasks, CurrentThreadIfKnown);
	}
}

void FTaskGraphImplementation::TriggerEventWhenTasksComplete(FEvent* InEvent, const FGraphEventArray& Tasks, ENamedThreads::Type CurrentThreadIfKnown, ENamedThreads::Type TriggerThread)
{
	bool bAnyPending = true;
	if (Tasks.size() < 8)
	{
		bAnyPending = false;
		for (int32 Index = 0; Index < Tasks.size(); Index++)
		{
			FGraphEvent* Task = Tasks[Index].get();
			if (Task && !Task->IsComplete())
			{
				bAnyPending = true;
				break;
			}
		}
	}
	if (!bAnyPending)
	{
		InEvent->Trigger();
		return;
	}
	TGraphTask<FTriggerEventGraphTask>::CreateTask(&Tasks, CurrentThreadIfKnown).ConstructAndDispatchWhenReady(InEvent, TriggerThread);
}

void FTaskGraphImplementation::AddShutdownCallback(std::function<void()>& Callback)
{
	ShutdownCallbacks.push_back(Callback);
}

void FTaskGraphImplementation::WakeNamedThread(ENamedThreads::Type ThreadToWake)
{
	const ENamedThreads::Type ThreadIndex = ENamedThreads::GetThreadIndex(ThreadToWake);
	if (ThreadToWake)
	{
		Thread(ThreadIndex).WakeUp(ENamedThreads::GetQueueIndex(ThreadToWake));
	}
}










