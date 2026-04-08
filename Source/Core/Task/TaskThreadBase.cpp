#include "TaskThreadBase.h"

FTaskThreadBase::FTaskThreadBase()
	:ThreadId(ENamedThreads::AnyThread)
	,PerThreadIDTLSSlot(0xffffffff)
	,OwnerWorker(nullptr)
{
	NewTasks.reserve(128);
}

void FTaskThreadBase::Setup(ENamedThreads::Type InThreadId, uint32 InPerThreadIDTLSSlot, FWorkerThread* InOwnerWorker)
{
	ThreadId = InThreadId;
	assert(ThreadId > 0);
	PerThreadIDTLSSlot = InPerThreadIDTLSSlot;
	OwnerWorker = InOwnerWorker;
}

void FTaskThreadBase::InitializeForCurrentThread()
{
	// 注意这里设置的不是this 而是OwnerWorker
	FPlatformTLS::SetTlsValue(PerThreadIDTLSSlot, OwnerWorker);
}

ENamedThreads::Type FTaskThreadBase::GetThreadId() const
{
	return ThreadId;
}

uint64 FTaskThreadBase::ProcessTasksUntilIdle(int32 QueueIndex)
{
	return 0;
}

bool FTaskThreadBase::Init()
{
	InitializeForCurrentThread();
	return true;
}

uint32 FTaskThreadBase::Run()
{
	ProcessTasksUntilQuit(0);

	/*
	 * 这不是清理你通过 FPlatformTLS::SetTlsValue 设置的 自定义 TLS 值，而是专门用于清理UE 内部的线程本地缓存系统，例如：
	 * FMalloc 线程局部缓存（比如线程私有的 TBB, Binned 分配器缓存）；
	 * 小对象内存池中的 per-thread block cache；
	 * FSlowHeartBeatScope 或 FStatsThreadState 中的缓存状态；
	 * 某些平台上分配器为了性能维护的 thread-local free list。
	 */
	//FMemory::ClearAndDisableTLSCachesOnCurrentThread();
	return 0;
}

void FTaskThreadBase::Stop()
{
	RequestQuit(-1);
}

void FTaskThreadBase::Exit()
{
	FRunnable::Exit();
}

FSingleThreadRunnable* FTaskThreadBase::GetSingleThreadInterface()
{
	return this;
}

void FTaskThreadBase::Tick()
{
	ProcessTasksUntilIdle(0);
}





