#pragma once
#include <assert.h>
#include <vector>

#include "Core.h"
#include "Event.h"
#include "ThreadConfig.h"
#include "TlsAutoCleanup.h"
#include "Common/LogAssert.h"
#include "Win/WindowsSyncUtils.h"
class FRunnable;

/*
 * 线程基础的接口类，负责对一个线程的生命周期的管理。具体的线程类实现这个接口，
 * 用于创建和管理后台线程。它提供了一种方便的方式来启动、暂停、恢复和停止线程
 */
class COREMODULE FRunnableThread
{
	friend class FThreadManager;
	// TLS的索引槽 索引槽的数量有上限的，win 1088 linux 1024 mac 512 android 128 ~ 256
	static unsigned int RunnableTlsSlot;
public:

	/*返回一个新的槽位用来存储 runnable thread 的 pointer 每个线程应该只调用一次*/
	static unsigned int GetTlsSlot();
	FRunnableThread();
	virtual ~FRunnableThread();
	/*
	 * 创建一个线程并且指定线程栈空间的大小和线程的优先级。
	 * 最后三个参数：线程栈大小
	 * 要绑定的物理核心
	 * 是否允许使用超线程核心
	 */
	static FRunnableThread* Create(FRunnable* InRunnable, const char* ThreadName, unsigned int InStackSize = 0, EThreadPriority InThreadPri = TPri_Normal, uint64 InThreadAffinityMask = GetNoAffinityMask(), EThreadCreateFlags InCreateFlags = EThreadCreateFlags::None);
	virtual void SetThreadPriority(EThreadPriority NewPriority) = 0;
	virtual void Suspend(bool bShouldPause = true) = 0;
	virtual bool Kill(bool bShouldWait = true) = 0;
	virtual void WaitForCompletion() = 0;

	const uint32 GetThreadID() const
	{
		return ThreadID;
	}
	const FString& GetThreadName() const
	{
		return ThreadName;
	}

	EThreadPriority GetThreadPriority() const
	{
		return ThreadPriority;
	}
	static FRunnableThread* GetRunnableThread()
	{
		FRunnableThread* RunnableThread = (FRunnableThread*)FPlatformTLS::GetTlsValue(RunnableTlsSlot);
		return RunnableThread;
	}
	//当前线程必须销毁的一些实例
	std::vector<FTlsAutoCleanup*> TlsInstances;
protected:

	virtual bool CreateInternal(FRunnable* InRunnable, const char* InThreadName,
		uint32 InStackSize = 0,
		EThreadPriority InThreadPri = TPri_Normal, uint64 InThreadAffinityMask = 0,
		EThreadCreateFlags InCreateFlags = EThreadCreateFlags::None) = 0;
	void SetTls();
	void FreeTls();


	std::string ThreadName;
	FRunnable* Runnable{ nullptr }; // 线程要做的事的逻辑类

	// 线程同步事件，类似于windows的event机制，或者类似标准库的condition_variable
	FEvent* ThreadInitSyncEvent;

	//处理器核心掩码
	uint64 ThreadAffinityMask;



	//线程优先级
	EThreadPriority ThreadPriority;


	//线程id
	uint32 ThreadID;




protected:
	enum class ThreadType
	{
		Real,
		Fake, // 单核cpu下的假线程
		Forkable, // linux或者macos下才有的机制，在 Linux/macOS 上使用 fork() 创建子进程时：
	/*
		*linux与mac上会存在fork的问题：fork都复制了整个进程，但是只启用了它所在的那个执行路径，如果它所在的那个执行路径fork后有创建线程的话，子进程会创建线程；如果它所在的那个执行路径fork后没有创建线程而在fork之前有创建线程的话，那么fork不会启用fork之前创建的线程，只会启用当前所在线程，继续执行下去。
		*这样的话，在fork之前存在线程的话，我们fork之后子进程不会跟父进程完全一样。因此我们希望可以被forkable的线程在主进程中是fake的，在子进程中才真的创建
	*/
	};

private:
	/*设置新创建线程*/
	static void SetupCreatedThread(FRunnableThread*& NewThread, FRunnable* InRunnable, const char* ThreadName, uint32 InStackSize, EThreadPriority InThreadPri, uint64 InThreadAffinityMask, EThreadCreateFlags InCreateFlags);

	//单线程模式下，我们允许thread manager来
	virtual void Tick() {}

	virtual FRunnableThread::ThreadType GetThreadType() const
	{
		return ThreadType::Real;
	}

	virtual void OnPostFork()
	{
		LogStringMsg("Only forkable threads should receive OnPostFork.");
		assert(false);
	}

	void PostCreate(EThreadPriority ThreadPriority);
};


/*Fake Thread 要用的执行结构体*/
class COREMODULE FSingleThreadRunnable
{
public:

	virtual ~FSingleThreadRunnable() {}
	virtual void Tick() = 0;
};

class COREMODULE FFakeThread : public FRunnableThread
{
	static uint32 ThreadIdCounter;


protected:
	bool bIsSuspended;
	FSingleThreadRunnable* SingleThreadRunnable;
public:
	//为 虚拟线程 ID（fake thread ID） 预留一个位，用于标记这些 ID 与 真实的系统线程 ID 区分开来，避免冲突
	static constexpr uint32 FakeIdReservedBit = 1 << 31;
	FFakeThread();
	virtual ~FFakeThread();

	virtual void Tick() override;

	virtual void SetThreadPriority(EThreadPriority NewPriority) override;

	virtual void Suspend(bool bShouldPause) override;
	virtual bool Kill(bool bShouldWait) override;
	virtual void WaitForCompletion() override;
	virtual bool CreateInternal(FRunnable* InRunnable, const char* InThreadName,
		uint32 InStackSize,
		EThreadPriority InThreadPri, uint64 InThreadAffinityMask,
		EThreadCreateFlags InCreateFlags = EThreadCreateFlags::None) override;
	virtual FRunnableThread::ThreadType GetThreadType() const override;
};