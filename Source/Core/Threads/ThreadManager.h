#pragma once
#include <unordered_map>

#include "Core.h"
#if PLATFORM_WIN
#include "Win/WindowsSyncUtils.h"
#endif

/*
 * 管理Runnables 以及 Runnable Threads
 */
class FRunnableThread;
class COREMODULE FThreadManager
{
	/*
	 * 对于Threads的本身的管理，就需要加锁
	 */
	FCriticalSection ThreadsCritical;
	static bool bIsInitialized;

	/*
	 * 要被tick的所有线程的列表
	 */
	std::unordered_map<uint32, FRunnableThread*> Threads;

public:

	static FThreadManager& Get();

	/**
	 * 
	 * @param ThreadId 
	 * @param Thread 
	 */
	void AddThread(uint32 ThreadId, class FRunnableThread* Thread);

	/**
	 * 
	 * @param Thread 
	 */
	void RemoveThread(class FRunnableThread* Thread);


	/**
	 * Tick 所有的fakethreads
	 */
	void Tick();

	inline static const FString& GetThreadName(uint32 ThreadId)
	{
		static FString GameThreadName(TEXT("GameThread"));
		static FString RenderThreadName(TEXT("RenderThread"));
		if (ThreadId == GGameThreadId)
		{
			return GameThreadName;
		}
		else if (IsInActualRenderingThread())
		{
			return RenderThreadName;
		}
		return Get().GetThreadNameInternal(ThreadId);
	}

	static bool IsInitialized()
	{
		return bIsInitialized;
	}

#if PLATFORM_WIN || PLATFORM_MAC
	struct FThreadStackBackTrace
	{
		uint32 ThreadId;
		FString ThreadName;
		std::vector<uint64> ProgramCounters;
	};

	void GetAllThreadStackBackTraces(std::vector<FThreadStackBackTrace>& StackTraces);
#endif

	/*
	 * 遍历所有线程的接口
	 */
	void ForEachThread(TFunction<void(uint32, class FRunnableThread*)> Func);


	static FORCEINLINE bool IsInActualRenderingThread()
	{
		return FPlatformTLS::GetCurrentThreadId() == GRenderThreadId;
	}

private:
	std::vector<FRunnableThread*> GetForkableThreads();
	const FString& GetThreadNameInternal(uint32 ThreadId);

};
