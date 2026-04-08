#include "WindowsRunnableThread.h"

uint32 FRunnableThreadWin::GuardedRun()
{
	uint32 ExitCode = 0;
	FPlatformProcess::SetThreadAffinityMask(ThreadAffinityMask);
	FPlatformProcess::SetThreadName(ThreadName.c_str());
	ExitCode = Run();
	return ExitCode;
}

uint32 FRunnableThreadWin::Run()
{
	uint32 ExitCode = 1;
	if (Runnable->Init() == true)
	{
		ThreadInitSyncEvent->Trigger();
		SetTls();
		ExitCode = Runnable->Run();
		Runnable->Exit();
		FreeTls();
	}
	else
	{
		//就算初始化失败，我们也还是放开这个事件，通知主线程可以继续了
		ThreadInitSyncEvent->Trigger();

	}

	return ExitCode;
}

FRunnableThreadWin::FRunnableThreadWin()
	: Thread(NULL)
{
}

FRunnableThreadWin::~FRunnableThreadWin()
{
	if (Thread != NULL)
	{
		Kill(true);
	}
}

int FRunnableThreadWin::TranslateThreadPriority(EThreadPriority Priority)
{
	switch (Priority)
	{
	case TPri_AboveNormal: return THREAD_PRIORITY_HIGHEST;
	case TPri_Normal: return THREAD_PRIORITY_HIGHEST - 1;
	case TPri_BelowNormal: return THREAD_PRIORITY_HIGHEST - 3;
	case TPri_Highest: return THREAD_PRIORITY_HIGHEST;
	case TPri_TimeCritical: return THREAD_PRIORITY_HIGHEST;
	case TPri_Lowest: return THREAD_PRIORITY_HIGHEST - 4;
	case TPri_SlightlyBelowNormal: return THREAD_PRIORITY_HIGHEST - 2;
	default: assert(false);
		return TPri_Normal;
	}
}

void FRunnableThreadWin::SetThreadPriority(EThreadPriority NewPriority)
{
	ThreadPriority = NewPriority;
	::SetThreadPriority(Thread, TranslateThreadPriority(ThreadPriority));
}

void FRunnableThreadWin::Suspend(bool bShouldPause)
{
	if (bShouldPause == true)
	{
		SuspendThread(Thread);
	}
	else
	{
		ResumeThread(Thread);
	}
}

bool FRunnableThreadWin::Kill(bool bShouldWait)
{
	bool bDidExitOK = true;
	if (Runnable)
	{
		//无论需要不要wait，我们都调用一下stop
		Runnable->Stop();
	}
	if (bShouldWait == true)
	{
		WaitForSingleObject(Thread, INFINITE);
	}

	CloseHandle(Thread);
	Thread = NULL;

	return bDidExitOK;
}

void FRunnableThreadWin::WaitForCompletion()
{
	WaitForSingleObject(Thread, INFINITE);
}

bool FRunnableThreadWin::CreateInternal(FRunnable* InRunnable, const char* InThreadName, uint32 InStackSize, EThreadPriority InThreadPri, uint64 InThreadAffinityMask, EThreadCreateFlags InCreateFlags)
{
	static bool bOnce = false;
	if (!bOnce)
	{
		bOnce = true;
		/*将主线程设置为“正常优先级”，因为默认情况下 Windows 创建的主线程不是 Normal 优先级（而是较高的 THREAD_PRIORITY_HIGHEST）*/
		::SetThreadPriority(::GetCurrentThread(), TranslateThreadPriority(TPri_Normal)); // set the main thread to be normal, since this is no longer the windows default.
	}


	Runnable = InRunnable;
	ThreadAffinityMask = InThreadAffinityMask;
	//线程的同步事件 windows上事件是内核对象，它本质上是系统范围可见的同步机制。任何线程，只要有这个事件对象的句柄（HANDLE），都可以等待或设置这个事件。而不必关联这个事件到线程
	ThreadInitSyncEvent = FPlatformProcess::GetSynchEventFromPool(true);

	ThreadName = InThreadName ? InThreadName : "Unnamed Thread";
	ThreadPriority = InThreadPri;

	{
		/*
		 * windows创建thread的函数，返回一个句柄，这个句柄才真正关联线程
		 * 而我们则把执行的委托逻辑交给了_ThreadProc，这个函数里调用我们的线程的执行逻辑
		 */
		Thread = CreateThread(NULL, InStackSize, _ThreadProc, this, STACK_SIZE_PARAM_IS_A_RESERVATION | CREATE_SUSPENDED, (::DWORD*)&ThreadID);
	}

	if (Thread == NULL)
	{
		//创建失败我们就不要把Runnable进行赋值
		Runnable = nullptr;
	}
	else
	{
		// 创建成功，保证线程唤醒
		ResumeThread(Thread);
		ThreadInitSyncEvent->Wait(INFINITE);
		ThreadPriority = TPri_Normal;
		SetThreadPriority(InThreadPri);
	}

	FPlatformProcess::ReturnSynchEventToPool(ThreadInitSyncEvent);
	ThreadInitSyncEvent = nullptr;
	return Thread != NULL;

}
