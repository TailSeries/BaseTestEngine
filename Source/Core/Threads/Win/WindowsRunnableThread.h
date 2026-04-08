#pragma once
#include "Core.h"
#include "Threads/Runnable.h"
#include "Threads/RunnableThread.h"
#include "Threads/ThreadManager.h"

class COREMODULE FRunnableThreadWin : public FRunnableThread
{
public:

	HANDLE Thread;

	//windows商
	static ::DWORD STDCALL _ThreadProc(LPVOID pThis)
	{
		auto* ThisThread =  static_cast<FRunnableThreadWin*>(pThis);
		FThreadManager::Get().AddThread(ThisThread->GetThreadID(), ThisThread);
		return ThisThread->GuardedRun();
	}

	/*如果调试器正在附加运行（例如你在 Visual Studio 中调试），那么不会进入保护模式（不触发异常捕获），这样你能在崩溃点直接看到 callstack；*/
	uint32 GuardedRun();

	/*线程真正的入口*/
	uint32 Run();

public:
	FRunnableThreadWin();
	virtual ~FRunnableThreadWin();
	static int TranslateThreadPriority(EThreadPriority Priority);
	virtual void SetThreadPriority(EThreadPriority NewPriority) override;
	virtual void Suspend(bool bShouldPause = true) override;
	virtual bool Kill(bool bShouldWait = false) override;
	virtual void WaitForCompletion() override;
	virtual bool CreateInternal(FRunnable* InRunnable, const char* InThreadName,
		uint32 InStackSize = 0,
		EThreadPriority InThreadPri = TPri_Normal, uint64 InThreadAffinityMask = 0,
		EThreadCreateFlags InCreateFlags = EThreadCreateFlags::None) override;
};
