#include "ThreadManager.h"

#include "ScopeLock.h"
#include "Threads/RunnableThread.h"
bool FThreadManager::bIsInitialized = false;
FThreadManager& FThreadManager::Get()
{
	static FThreadManager Singleton;
	FThreadManager::bIsInitialized = true;
	return Singleton;
}

void FThreadManager::AddThread(uint32 ThreadId, class FRunnableThread* Thread)
{
	const bool bIsSingleThreadEnvironment = FPlatformProcess::SupportsMultithreading() == false;


	if (bIsSingleThreadEnvironment && Thread->GetThreadType() == FRunnableThread::ThreadType::Real)
	{
		assert((ThreadId & FFakeThread::FakeIdReservedBit) == 0);

		/*在单线程模拟多线程的环境中，用特定的 ID 位来区分“假线程”与“真线程”。

		这条断言确保操作系统分配给真线程的 ID 不会与假线程的标记位冲突。

		如果断言触发，说明设计中的线程 ID 分配逻辑出现了问题，需要修正。*/
	}

	FScopeLock ThreadsLock(&ThreadsCritical);
	auto it = Threads.find(ThreadId);
	if (it != Threads.end())
	{
		Threads.emplace(ThreadId, Thread);
	}
}

void FThreadManager::RemoveThread(class FRunnableThread* Thread)
{
	FScopeLock ThreadsLock(&ThreadsCritical);

	uint32 TID =  Thread->GetThreadID();
	auto it = Threads.find(TID);
	if (it != Threads.end())
	{
		Threads.erase(it);
	}
}

void FThreadManager::Tick()
{
	const bool bIsSingleThreadEnvironment = FPlatformProcess::SupportsMultithreading() == false;
	if (bIsSingleThreadEnvironment)
	{
		FScopeLock ThreadsLock(&ThreadsCritical);
		for (auto& ThreadPair : Threads)
		{
			//我们只Tick所有的Facke线程
			if (ThreadPair.second->GetThreadType() != FRunnableThread::ThreadType::Real)
			{
				ThreadPair.second->Tick();
			}
		}

	}
}

void FThreadManager::GetAllThreadStackBackTraces(std::vector<FThreadStackBackTrace>& StackTraces)
{
	//todo 用来获取分析堆栈，我们后面再实现
}

void FThreadManager::ForEachThread(TFunction<void(uint32, class FRunnableThread*)> Func)
{
	FScopeLock Lock(&ThreadsCritical);
	for (auto& Pair : Threads)
	{
		Func(Pair.first, Pair.second);
	}

}

std::vector<FRunnableThread*> FThreadManager::GetForkableThreads()
{
	std::vector<FRunnableThread*> ForkableThreads;
	FScopeLock Lock(&ThreadsCritical);
	for (const auto& Pair : Threads)
	{
		if (Pair.second->GetThreadType() == FRunnableThread::ThreadType::Forkable)
		{
			ForkableThreads.push_back(Pair.second);
		}
	}
	return ForkableThreads;
}

const FString& FThreadManager::GetThreadNameInternal(uint32 ThreadId)
{
	static FString NoThreadName;
	FScopeLock ThreadsLock(&ThreadsCritical);
	const auto Threadit = Threads.find(ThreadId);
	if (Threadit != Threads.end())
	{
		return (Threadit->second)->GetThreadName();
	}
	return NoThreadName;
}
