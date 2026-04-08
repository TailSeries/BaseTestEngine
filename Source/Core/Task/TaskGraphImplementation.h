#pragma once
#include "StallingTaskQueue.h"
#include "TaskGraphInterfaces.h"
#include "WorkerThread.h"
#include "Threads/ThreadConfig.h"


/**
 * 提供统一接口（例如 QueueTask()、WaitUntilTaskCompletes() 等）
 * 不处理“谁依赖谁”（即任务图拓扑结构）
 * 只处理任务的分派、线程队列管理、唤醒/阻塞线程、查找可执行任务等
 */
class COREMODULE FTaskGraphImplementation:public FTaskGraphInterface
{
public:

	/** 任务图系统的单例访问器
	 * 但是我们并不是用局部静态变量生成的单例，而是用一个全局的TaskGraphImplementationSingleton 是一个指向 FTaskGraphImplementation 的静态指针
	 * 在Startup中去初始化这个指针
	 * @return 
	 */
	static FTaskGraphImplementation& Get();
	static FTaskGraphImplementation* GetSingletonPtr();
	static void Desconstruct();


	/**
	 * TaskGraph系统的初始化逻辑:
	 * Startup()
		└─> new FTaskGraphImplementation(...)
	       ├─ 判断是否启用多线程任务图
	       ├─ 计算线程数量
	       ├─ 创建每个 Thread 的 TaskGraphWorker 实例
	       ├─ 创建实际 OS 线程（FRunnableThread）
	       └─ 设置单例 TaskGraphImplementationSingleton
	 */
	FTaskGraphImplementation();


	/**
	 * 不要手动调用这个，除非系统销毁，不然它不一定安全的.
	 * 析构的时候只会wait 并且 delete 具名线程的thread。why？
	 */
	virtual ~FTaskGraphImplementation();


	/**将一个 FBaseGraphTask 类型的任务调度到指定线程上运行，或者调度到任意可用的 worker 线程池中。
	 * 
	 * @param Task 指向要调度的任务对象
	 * @param ThreadToExecuteOn 指定任务在哪个线程上执行。可以是一个具名线程（如 GameThread, RenderThread），也可以是 AnyThread（表示可以丢给 worker 线程池调度）。
	 * @param CurrentThreadIfKnown 表示你当前是在哪个线程调用这个函数的。若不确定，可以填 AnyThread，系统会通过 TLS 自动识别。
	 */
	virtual void QueueTask(class FBaseGraphTask* Task, ENamedThreads::Type ThreadToExecuteOn, ENamedThreads::Type InCurrentThreadIfKnown) final override;


	/**
	 * 
	 * @param Priority 线程优先级
	 * @param IndexToStart 准备启动的线程
	 */
	void StartTaskThread(int32 Priority, int32 IndexToStart);
	void StartAllTaskThreads(bool bDoBackgroundTask);

	/**
	 * 根据ThreadInNeed 线程类型和优先级，从 TaskGraph 中找到一个可执行任务。
	 * @param ThreadInNeed 当前请求任务的线程（例如 GameThread、RenderThread 或 WorkerThread）
	 * @return FBaseGraphTask* → 找到的任务指针，如果没有任务则返回 nullptr
	 */
	FBaseGraphTask* FindWork(ENamedThreads::Type ThreadInNeed);


	/**
	 * 让所有线程集中索引值为Index的线程进入或退出“停顿状态”，方便做 线程调度调优或性能分析。
	 * @param Index 
	 * @param Stall 
	 */
	void StallForTuning(int32 Index, bool Stall);


	/**
	 * 修改 所有 Worker 线程优先级的接口。它直接作用于线程调度，属于 性能调优工具。
	 * @param Pri 
	 */
	void SetTaskThreadPriorities(EThreadPriority Pri);

	/**
	 * 检查线程局部存储（TLS）以识别当前线程的 ID
	 *
	 * 如果当前线程无法识别，或者是一个尚未附加的命名线程，则返回 ENamedThreads::AnyThread。
	 * @return 
	 */
	ENamedThreads::Type GetCurrentThread();
	
	/**
	 * 划分线程的索引转换成优先级组索引，按顺序依次按间隔排
	 * @param ThreadIndex WorkerThreads数组的下标
	 * @return 
	 */
	int32 ThreadIndexToPriorityIndex(int32 ThreadIndex);


	/**
	 * 返回每个优先集的线程组里有多少个线程
	 * @return 
	 */
	virtual	int32 GetNumWorkerThreads() final override;


	/**
	 * 
	 * @param bLocalQueue 表示任务应该入队到当前线程的 局部任务队列，而不是全局队列 每个 Worker 有自己的局部队列。Push 任务时可以指定放到本地队列，提高缓存局部性。如果本地队列空了，可以去别的线程队列“偷任务”。
	 * @return 如果条件满足，返回的 Result 就会多一个 LocalQueue 标志位。调用方会根据这个标志决定把任务推到哪里。
	 */
	virtual ENamedThreads::Type GetCurrentThreadIfKnown(bool bLocalQueue) final override;


	/**
	 * 某个线程是否正在执行任务
	 * @param ThreadToCheck 
	 * @return 
	 */
	virtual bool IsThreadProcessingTasks(ENamedThreads::Type ThreadToCheck) final override;


	/**
	 * 当一个“外部线程”（不是 TaskGraph 系统自己创建的工作线程，比如你自己开的 std::thread）想要参与到 UE4 的任务系统（TaskGraph）时，就必须调用这个函数。
	 * 它会把线程的信息注册到 TaskGraph 里，并设置线程局部存储（TLS，Thread Local Storage），这样 UE4 知道这个线程是谁、属于哪个 ENamedThreads::Type。
	 * @param CurrentThread 
	 */
	virtual void AttachToThread(ENamedThreads::Type CurrentThread) final override;


	/**
	 * 
	 * @param CurrentThread 
	 * @return 返回完成任务的数量
	 */
	virtual uint64 ProcessThreadUntilIdle(ENamedThreads::Type CurrentThread) final override;

	virtual void ProcessThreadUntilRequestReturn(ENamedThreads::Type CurrentThread) override;

	virtual void RequestReturn(ENamedThreads::Type CurrentThread) override;


	virtual void WaitUntilTasksComplete(const FGraphEventArray& Tasks, ENamedThreads::Type CurrentThreadIfKnown) override;

	/**
	 * 任务完成后在期望的线程Trigger Event
	 * @param InEvent 
	 * @param Tasks 
	 * @param CurrentThreadIfKnown 
	 * @param TriggerThread 
	 */
	virtual void TriggerEventWhenTasksComplete(FEvent* InEvent, const FGraphEventArray& Tasks, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::Type::AnyThread, ENamedThreads::Type TriggerThread = ENamedThreads::Type::AnyHiPriThreadHiPriTask) override;


	virtual void AddShutdownCallback(std::function<void()>& Callback);

	virtual void WakeNamedThread(ENamedThreads::Type ThreadToWake) override;

private:

	/**
	 * 从 WorkerThreads 中一直拿到对应的真实的线程逻辑对象
	 * @param Index WorkerThreads 中的索引
	 * @return 
	 */
	FTaskThreadBase& Thread(int32 Index);

private:
	enum
	{
		//每个优先级组可能分配最多 26 条线程，总共 N 组。总的线程数量就是前面的+具名线程+1
		MAX_THREADS = 26 * (CREATE_HIPRI_TASK_THREADS + CREATE_BACKGROUND_TASK_THREADS + 1) + ENamedThreads::ActualRenderingThread + 1,
		MAX_THREAD_PRIORITIES = 3  //最大优先级数量。UE4 中默认支持三种任务优先级线程池：正常（Normal） HiPri Background
	};

	// 所有的工作线程
	FWorkerThread WorkerThreads[MAX_THREADS];
	// 实际创建和使用的线程
	int32 NumThreads;
	// 实际在使用的named threads
	int32 NumNamedThreads;
	// 有多少不同优先集的线程组
	int32 NumTaskThreadSets;
	// 每个优先集下的线程的数量
	int32 NumTaskThreadsPerSet;
	// normal 优先级是必须存在的，所以这里只有其余两个的bool标记要不要创建HiPriority以及Backgroundpriority
	bool bCreatedHiPriorityThreads;
	bool bCreatedBackgroundPriorityThreads;


	/**
	 * 记录最后一个已附加的“外部线程”的线程 ID。
	 * 所谓“外部线程”是指不是 UE4 自己通过 FRunnableThread::Create 创建的线程，比如：渲染线程 游戏线程 主线程
	 * 所有的匿名线程必须是internal的
	 */
	ENamedThreads::Type LastExternalThread;

	//线程安全的计数器，用于检测是否出现递归进入的情况（reentrant call）
	//比如某个线程正在执行任务，但任务中又递归触发 ProcessTasks() 等，可能产生调度死锁
	std::atomic<int32> ReentrancyCheck;
	//TLS 槽位
	uint32 PerThreadIDTLSSlot;
	//在任务图系统关闭时，会依次调用的清理回调。
	std::vector<TFunction<void()>> ShutdownCallbacks;
	/*
	 * 每个优先级都有一个任务队列，存储投递给 AnyThread 的任务（不限目标线程）
	 * 每个任务队列支持两个线程优先级线程在等待或处理（通常是 Normal 和 HiPri）。
	 * 注意这个结构从pop的设计来看默认应该是 0 是高优先级，1是低优先级。
	 */
	FStallingTaskQueue<FBaseGraphTask, 2> IncomingAnyThreadTasks[MAX_THREAD_PRIORITIES];
};

