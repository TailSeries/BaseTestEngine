#include "RunnableThread.h"
#include "Runnable.h"
#include "ThreadManager.h"
#include "Win/WindowsSyncUtils.h"


uint32 FRunnableThread::RunnableTlsSlot = FRunnableThread::GetTlsSlot();
unsigned int FRunnableThread::GetTlsSlot()
{
	uint32 TlsSlot = FPlatformTLS::AllocTlsSlot();
	return TlsSlot;
}

FRunnableThread::FRunnableThread()
	: Runnable(nullptr)
	, ThreadInitSyncEvent(nullptr)
	, ThreadAffinityMask(GetNoAffinityMask())
	, ThreadPriority(TPri_Normal)
	, ThreadID(0)
{
}

FRunnableThread::~FRunnableThread()
{
	FThreadManager::Get().RemoveThread(this);
}

FRunnableThread* FRunnableThread::Create(FRunnable* InRunnable, const char* ThreadName, unsigned int InStackSize, EThreadPriority InThreadPri, uint64 InThreadAffinityMask, EThreadCreateFlags InCreateFlags)
{
	//先检查系统本身是否支持多线程
	bool bCreateRealThread = FPlatformProcess::SupportsMultithreading();
	FRunnableThread* NewThread = nullptr;
	if (bCreateRealThread)
	{
		//调用该平台创建线程的方法
		NewThread = FPlatformProcess::CreateRunnableThread();
	}
	else if (InRunnable->GetSingleThreadInterface())
	{
		NewThread = new FFakeThread();
	}
	if (NewThread)
	{
		SetupCreatedThread(NewThread, InRunnable, ThreadName, InStackSize, InThreadPri, InThreadAffinityMask, InCreateFlags);
	}

	return NewThread;
}

void FRunnableThread::SetupCreatedThread(FRunnableThread*& NewThread, FRunnable* InRunnable, const char* ThreadName, uint32 InStackSize, EThreadPriority InThreadPri, uint64 InThreadAffinityMask, EThreadCreateFlags InCreateFlags)
{
	bool bIsValid = NewThread->CreateInternal(InRunnable, ThreadName, InStackSize, InThreadPri, InThreadAffinityMask, InCreateFlags);
	if (bIsValid)
	{
		NewThread->PostCreate(InThreadPri);
	}
	else
	{
		//创建失败的情况下删除这个thread
		delete NewThread;
		NewThread = nullptr;
	}

}
void FRunnableThread::PostCreate(EThreadPriority InThreadPriority)
{
	//todo 后续实现我们的stats
	//FStartupMessages::Get().AddThreadMetadata(FName(*GetThreadName()), GetThreadID());
}


void FRunnableThread::SetTls()
{
	FPlatformTLS::SetTlsValue(RunnableTlsSlot, this);
}
void FRunnableThread::FreeTls()
{
	FPlatformTLS::SetTlsValue(RunnableTlsSlot, nullptr);
	for (auto& Instance : TlsInstances)
	{
		delete Instance;
		Instance = nullptr;
	}
}


FFakeThread::FFakeThread()
	: bIsSuspended(false)
	, SingleThreadRunnable(nullptr)
{
	ThreadID = ThreadIdCounter++;
	ThreadID |= FakeIdReservedBit;
	FThreadManager::Get().AddThread(ThreadID, this);
}

FFakeThread::~FFakeThread()
{
	FThreadManager::Get().RemoveThread(this);
}

void FFakeThread::Tick()
{
	if (SingleThreadRunnable && !bIsSuspended)
	{
		SingleThreadRunnable->Tick();
	}
}

void FFakeThread::SetThreadPriority(EThreadPriority NewPriority)
{
}

void FFakeThread::Suspend(bool bShouldPause)
{
	bIsSuspended = bShouldPause;
}

bool FFakeThread::Kill(bool bShouldWait)
{
	FThreadManager::Get().RemoveThread(this);
	return true;
}

void FFakeThread::WaitForCompletion()
{
	FThreadManager::Get().RemoveThread(this);
}

bool FFakeThread::CreateInternal(FRunnable* InRunnable, const char* InThreadName, uint32 InStackSize, EThreadPriority InThreadPri, uint64 InThreadAffinityMask, EThreadCreateFlags InCreateFlags)
{
	ThreadName = InThreadName;
	ThreadAffinityMask = InThreadAffinityMask;

	SingleThreadRunnable = InRunnable->GetSingleThreadInterface();
	if (SingleThreadRunnable)
	{
		InRunnable->Init();

		Runnable = InRunnable;
	}
	return SingleThreadRunnable != nullptr;
}

FRunnableThread::ThreadType FFakeThread::GetThreadType() const
{
	return ThreadType::Fake;

}
uint32 FFakeThread::ThreadIdCounter = 0xffff; // 65535 避开一般的真实的线程id的范围
/*一个线程池 FQueuedThread*/
