#pragma once
#include <assert.h>
#include <forward_list>
#include "Core.h"
#include "Threads/ScopeLock.h"

// 控制渲染线程的轮询及其周期
extern COREMODULE bool GRenderThreadPollingOn;	
extern COREMODULE int32 GRenderThreadPollPeriodMs;	
namespace ENamedThreads
{
	/*
	 * 按bit位来配置线程的各种功能
	 *								 NamedThread
	 * 0000 0000,0000 0000,0000 0000,0000 0000
	 * 8位：留给各种namedThread
	 * 9: NamedThread的队列Queue 索引 0 MainQueue 1：LocalQueue
	 * 10：Task的优先级 0 NormalTaskPriority, 1: HighTaskPriority
	 * 11 12:线程的优先级：00：NormalThreadPriority，01：HighThreadPriority，10：BackgroundThreadPriority
	 *
	 */
	enum Type : int32
	{
		UnusedAnchor = -1,
#if STATS
		StatsThread,
#endif
		RHIThread,
		AudioThread,
		GameThread,
		ActualRenderingThread = GameThread + 1,
		AnyThread = 0xff,
		ThreadIndexMask = 0xff,//辅助运算上面的Thread

		MainQueue = 0x000,
		LocalQueue = 0x100,

		NumQueues = 2,

		QueueIndexMask = 0x100,
		QueueIndexShift = 8, //辅助运算上面的queue

		NormalTaskPriority = 0x000,
		HighTaskPriority = 0x200,

		NumTaskPriorities = 2,
		TaskPriorityMask = 0x200,
		TaskPriorityShift = 9,

		NormalThreadPriority = 0x000,
		HighThreadPriority = 0x400,
		BackgroundThreadPriority = 0x800,

		NumThreadPriorities = 3,
		ThreadPriorityMask = 0xC00,
		ThreadPriorityShift = 10,


		/** Combinations **/
#if STATS
		StatsThread_Local = StatsThread | LocalQueue,
#endif
		GameThread_Local = GameThread | LocalQueue,
		ActualRenderingThread_Local = ActualRenderingThread | LocalQueue,

		AnyHiPriThreadNormalTask = AnyThread | HighThreadPriority | NormalTaskPriority,
		AnyHiPriThreadHiPriTask = AnyThread | HighThreadPriority | HighTaskPriority,

		AnyNormalThreadNormalTask = AnyThread | NormalThreadPriority | NormalTaskPriority,
		AnyNormalThreadHiPriTask = AnyThread | NormalThreadPriority | HighTaskPriority,

		AnyBackgroundThreadNormalTask = AnyThread | BackgroundThreadPriority | NormalTaskPriority,
		AnyBackgroundHiPriTask = AnyThread | BackgroundThreadPriority | HighTaskPriority,
	};

	extern COREMODULE int32 bHasBackgroundThreads;
	extern COREMODULE int32 bHasHighPriorityThreads;

	//获取当前是哪一个ThreadIndex
	FORCEINLINE Type GetThreadIndex(Type ThreadAndIndex)
	{
		return ((ThreadAndIndex & ThreadIndexMask) == AnyThread) ? AnyThread : Type(ThreadAndIndex & ThreadIndexMask);
	}

	FORCEINLINE int32 GetQueueIndex(Type ThreadAndIndex)
	{
		return (ThreadAndIndex & QueueIndexMask) >> QueueIndexShift;
	}

	FORCEINLINE int32 GetTaskPriority(Type ThreadAndIndex)
	{
		return (ThreadAndIndex & TaskPriorityMask) >> TaskPriorityShift;
	}

	FORCEINLINE int32 GetThreadPriorityIndex(Type ThreadAndIndex)
	{
		int32 Result = (ThreadAndIndex & ThreadPriorityMask) >> ThreadPriorityShift;
		assert(Result >= 0 && Result < NumThreadPriorities);
		return Result;
	}
	FORCEINLINE Type SetPriorities(Type ThreadAndIndex, Type ThreadPriority, Type TaskPriority)
	{
		assert(
			!(ThreadAndIndex & ~ThreadIndexMask) &&  // not a thread index
			!(ThreadPriority & ~ThreadPriorityMask) && // not a thread priority
			(ThreadPriority & ThreadPriorityMask) != ThreadPriorityMask && // not a valid thread priority
			!(TaskPriority & ~TaskPriorityMask) // not a task priority
		);
		return Type(ThreadAndIndex | ThreadPriority | TaskPriority);
	}

	FORCEINLINE Type SetPriorities(Type ThreadAndIndex, int32 ThreadPriorityIndex, bool bHiPri)
	{
		assert(
			!(ThreadAndIndex & ~ThreadIndexMask) && // not a thread index
			ThreadPriorityIndex >= 0 && ThreadPriorityIndex < NumThreadPriorities // not a valid thread priority
		);
		return Type(ThreadAndIndex | (ThreadPriorityIndex << ThreadPriorityShift) | (bHiPri ? HighTaskPriority : NormalTaskPriority));
	}

	FORCEINLINE Type SetThreadPriority(Type ThreadAndIndex, Type ThreadPriority)
	{
		assert(
			!(ThreadAndIndex & ~ThreadIndexMask) &&  // not a thread index
			!(ThreadPriority & ~ThreadPriorityMask) && // not a thread priority
			(ThreadPriority & ThreadPriorityMask) != ThreadPriorityMask // not a valid thread priority
		);
		return Type(ThreadAndIndex | ThreadPriority);
	}

	FORCEINLINE Type SetTaskPriority(Type ThreadAndIndex, Type TaskPriority)
	{
		assert(
			!(ThreadAndIndex & ~ThreadIndexMask) &&  // not a thread index
			!(TaskPriority & ~TaskPriorityMask) // not a task priority
		);
		return Type(ThreadAndIndex | TaskPriority);
	}
}

namespace ESubsequentsMode
{
	//任务系统调度策略
	enum Type
	{
		/* 会被后续任务依赖，必须追踪 */
		TrackSubsequents,
		/** 不会被依赖，不需要追踪，节省开销，任务完成就销毁 */
		FireAndForget
	};
}

typedef TRefCountPtr<class FGraphEvent> FGraphEventRef;
using FGraphEventArray = std::vector<FGraphEventRef>;





/*
 * TaskGraph 的接口类，可以通过FTaskGraphInterface::Get() 来访问
 */
class COREMODULE FTaskGraphInterface
{
	friend class FBaseGraphTask;
public:
	static FTaskGraphInterface& Get();
	virtual ~FTaskGraphInterface();



	/**
	 * 显式地初始化 Task Graph 系统和其工作线程池，是整个异步任务系统的启动入口。
	 * @param NumThreads
	 * 	一般而言：
		禁用 TaskGraph（单线程执行）	NumThreads = 0
		启用但不启用渲染线程（非 threaded rendering）	至少 2
		启用并启用渲染线程	至少 3（主线程、渲染线程、至少一个 worker）
	 */
	static void Startup(int32 NumThreads);

	/**
	 * Shutdown() 会在系统空闲时清理任务图系统（包括 worker 线程、任务队列和内部单例），必须在没有任务正在执行时调用，否则行为未定义或可能崩溃。
	 */
	static void Shutdown();

	static bool IsRunning();

	static bool IsMultithread();

	/**
	 * 获取当前线程在 Task Graph 系统中的标识（ENamedThreads::Type），如果知道的话。
	 * @param bLocalQueue  表示是否希望返回值考虑“局部队列”的上下文（用于调度优化，默认 false,
	 *			在一些实现中，如果 bLocalQueue = true，可能返回比实际线程 ID 更具体的值，比如“本地队列的任务正在执行”。
	 * @return
	 */
	virtual ENamedThreads::Type GetCurrentThreadIfKnown(bool bLocalQueue = false) = 0;


	/**
	 * 返回 每个优先级集（Priority Set）中可用的 worker thread 数量。
		这是 非命名线程（即普通的后台 worker），不包括 GameThread、RenderThread 等.默认每个优先级下的线程数目是相同的
	 * @return
	 */
	virtual	int32 GetNumWorkerThreads() = 0;


	/**
	 * 这个函数主要用于调试或调度优化，返回值为 true 表示这个线程正在处理 task graph 中的任务。
	 * @param ThreadToCheck ：是一个 ENamedThreads::Type 类型，标识一个命名线程或 worker 线程
	 * @return
	 */
	virtual bool IsThreadProcessingTasks(ENamedThreads::Type ThreadToCheck) = 0;



	/**
	 * 将当前正在运行的线程注册到 TaskGraph 系统中，使其可以正常参与任务图任务（TaskGraph tasks）的调度与执行。每个线程必须在自己线程上下文中调用 AttachToThread，即：
	 * @param CurrentThread
	 */
	virtual void AttachToThread(ENamedThreads::Type CurrentThread) = 0;


	/**
	 * 让调用它的那个线程（必须与 CurrentThread 参数表示的线程类型一致）去处理自己对应的任务队列，直到空闲。
	 *	注意必须是与 CurrentThread 类型一致，不能是任意的线程去访问队列
	 *	所以主线程就调用 ProcessThreadUntilIdle(ENamedThreads::GameThread); Anythread就调用 ProcessThreadUntilIdle(ENamedThreads::AnyThread);
	 * 它是线程主动“拉取”任务的机制，而不是任务系统自动推送任务。
	 * @param CurrentThread
	 * @return
	 */
	virtual uint64 ProcessThreadUntilIdle(ENamedThreads::Type CurrentThread) = 0;



	/**
	 *  这个函数会让调用线程持续运行任务图任务，直到被外部显式通知“停止”才返回。
		换句话说，它会进入一个“持续处理任务”的循环，不会自动返回，直到收到停止信号。
		这是比 ProcessThreadUntilIdle 更“持久”的任务处理方式，适合用作工作线程的主循环。
		同样调用者应该是当前线程的同类型type
	 * @param CurrentThread
	 */
	virtual void ProcessThreadUntilRequestReturn(ENamedThreads::Type CurrentThread) = 0;


	/**
	 * 这是向任务图系统发出请求，让指定的线程（由 CurrentThread 标识）在完成当前任务后，并且处于空闲状态时停止运行（返回）。
	 * @param CurrentThread
	 */
	virtual void RequestReturn(ENamedThreads::Type CurrentThread) = 0;


	/**
	 * 让调用该函数的线程（身份由 CurrentThreadIfKnown 指定，必须是“命名线程”）阻塞等待，直到传入的任务列表 Tasks 中的所有任务都执行完成。
		在等待期间，调用线程会处理自己负责的任务队列，以避免死锁。
		这是一个同步等待接口，保证所有依赖的任务完成后再继续执行。
	 * @param Tasks
	 * @param CurrentThreadIfKnown
	 */
	virtual void WaitUntilTasksComplete(const FGraphEventArray& Tasks, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread) = 0;


	/**
	 * 当 Tasks 中的所有任务完成后，触发（signal）传入的同步事件 InEvent。
	 * 触发事件的操作会在指定的线程 TriggerThread 上执行。
	 * 调用此函数的线程身份是 CurrentThreadIfKnown，用于任务图内部调度与优化。
	 * @param InEvent 要触发的同步事件（如 FEvent），通常用于线程间信号通知。
	 * @param Tasks 	需要等待完成的任务数组。
	 * @param CurrentThreadIfKnown 调用该接口的线程类型。默认 AnyThread。
	 * @param TriggerThread 触发事件操作要执行的线程类型。默认为高优先级线程。（指定在哪个线程上signal event）
	 */
	virtual void TriggerEventWhenTasksComplete(FEvent* InEvent, const FGraphEventArray& Tasks, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread, ENamedThreads::Type TriggerThread = ENamedThreads::AnyHiPriThreadHiPriTask) = 0;


	/**
	 * 这个函数让调用线程阻塞等待指定的单个任务 Task 完成。
	 * @param Task
	 * @param CurrentThreadIfKnown
	 */
	void WaitUntilTaskCompletes(const FGraphEventRef& Task, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		WaitUntilTasksComplete({ Task }, CurrentThreadIfKnown);
	}
	void WaitUntilTaskCompletes(FGraphEventRef&& Task, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		WaitUntilTasksComplete({ std::move(Task) }, CurrentThreadIfKnown);
	}


	/**
	 * 这个函数会在指定的单个任务 Task 完成时，触发同步事件 InEvent
	 * @param InEvent
	 * @param Task
	 * @param CurrentThreadIfKnown
	 * @param TriggerThread
	 */
	void TriggerEventWhenTaskCompletes(FEvent* InEvent, const FGraphEventRef& Task, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread, ENamedThreads::Type TriggerThread = ENamedThreads::AnyHiPriThreadHiPriTask)
	{
		FGraphEventArray Prerequistes;
		Prerequistes.push_back(Task);
		TriggerEventWhenTasksComplete(InEvent, Prerequistes, CurrentThreadIfKnown, TriggerThread);
	}


	/**
	 * 允许你注册一个回调函数（Callback），这个函数会在任务图系统准备关闭之前被调用
	 * @param Callback
	 */
	virtual void AddShutdownCallback(TFunction<void()>& Callback) = 0;


	/**
	 * 这个函数用于唤醒指定的命名线程，通常是让那个线程从等待状态或休眠状态恢复，继续处理任务。
	 * @param ThreadToWake 是要唤醒的线程类型，例如 ENamedThreads::GameThread、
	 */
	virtual void WakeNamedThread(ENamedThreads::Type ThreadToWake) = 0;


	/**
	 * 该函数会遍历所有已知的线程类型，包括命名线程和工作线程，根据参数选择是否包含任务线程和后台线程。\
	 * 对每个线程，都会调用传入的 Callback，并传入当前线程的 ENamedThreads::Type。
	 * 由于需要遍历很多线程，且涉及跨线程调用等，函数执行较慢，故带有明显提示“仅限特殊用途”。
	 * @param bDoTaskThreads 是否包括任务图中的工作线程
	 * @param bDoBackgroundThreads 是否包括后台线程
	 * @param Callback 	针对每个线程调用的函数，参数是该线程的类型

	 */
	static void BroadcastSlow_OnlyUseForSpecialPurposes(bool bDoTaskThreads, bool bDoBackgroundThreads, TFunction<void(ENamedThreads::Type CurrentThread)>& Callback);

private:

	/**
	 * TaskGraph 中将任务安排到正确线程或线程池的核心接口。
	 * @param Task 要调度的任务
	 * @param ThreadToExecuteOn 表示这个任务要在哪个线程上执行（可能是 GameThread、RenderThread)
	 * @param CurrentThreadIfKnown 如果调用者知道自己当前线程，可以填这个值，用于优化调度路径；否则默认填 AnyThread

	 */
	virtual void QueueTask(class FBaseGraphTask* Task, ENamedThreads::Type ThreadToExecuteOn, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread) = 0;
};


/*任务执行的核心单元
 * 任务图系统中所有异步执行的单元都继承自它。
 * 管理任务的生命周期、依赖计数、线程调度、执行逻辑。
 */
class COREMODULE FBaseGraphTask
{
public:
	//小任务分配的大小就256字节，方便使用自定义分配器的情况
	enum
	{
		SMALL_TASK_SIZE = 256
	};


	/**
	 *ThreadToExecuteOn 	表示该任务希望在哪个线程上执行，由调度器根据它进行分发
	 * nNumberOfPrerequistitesOutstanding + 1了 依赖任务数量+1，以免异步时，任务执行完了，这个东西还没构造完成。
	 * @param InNumberOfPrerequistitesOutstanding  依赖数目
	 */
	FBaseGraphTask(int32 InNumberOfPrerequistitesOutstanding)
		: ThreadToExecuteOn(ENamedThreads::AnyThread)
		, NumberOfPrerequistitesOutstanding(InNumberOfPrerequistitesOutstanding + 1)
	{
		assert(++LifeStage== int32(LS_Contructed));
	}


	/**
	 * 在任务构造之后、调度之前，设置该任务期望在哪个线程上执行。
	 * 为什么不是在构造函数里设置？构造 FBaseGraphTask 时，任务的执行线程信息往往还未确定。
	 * 比如某些任务需要根据依赖任务的结果，或调度策略，动态决定在哪个线程上运行。
	 * 所以设计上将其延迟到构造之后设置，避免一开始就绑定死线程类型
	 * @param InThreadToExecuteOn
	 */
	void SetThreadToExecuteOn(ENamedThreads::Type InThreadToExecuteOn)
	{
		ThreadToExecuteOn = InThreadToExecuteOn;
		assert(++LifeStage == int32(LS_ThreadSet));
	}



	/**
	 * 任务在先决条件（Prerequisites）都满足之后，是否立即排队执行，取决于这个函数是否被调用、以及 bUnlock 是否为 true。
	 * @param CurrentThread
	 * @param NumAlreadyFinishedPrequistes
	 * @param bUnlock
	 */
	void PrerequisitesComplete(ENamedThreads::Type CurrentThread, int32 NumAlreadyFinishedPrequistes, bool bUnlock = true)
	{
		assert(++LifeStage == (int32)(LS_PrequisitesSetup));
		int32 NumToSub = NumAlreadyFinishedPrequistes + (bUnlock ? 1 : 0); // the +1 is for the "lock" we set up in the constructor
		if (NumberOfPrerequistitesOutstanding.fetch_sub(NumToSub) == NumToSub)
		{
			QueueTask(CurrentThread);
		}
	}


	/**当一个前置任务完成时，尝试减少剩余前置任务计数；如果这是最后一个，则将任务入队执行。
	 * @param CurrentThread
	 */
	void ConditionalQueueTask(ENamedThreads::Type CurrentThread)
	{
		if (--NumberOfPrerequistitesOutstanding == 0) //CPP20  这是个原子操作
		{
			QueueTask(CurrentThread);
		}
	}


	virtual ~FBaseGraphTask()
	{

	}




private:
	friend class FNamedTaskThread;
	friend class FTaskThreadBase;
	friend class FTaskThreadAnyThread;
	friend class FGraphEvent;
	friend class FTaskGraphImplementation;



	/**
	 * 实际执行该任务的逻辑。任务系统调度线程在准备执行任务时，会调用这个函数。
	 * @param NewTasks 允许在执行当前任务过程中产生新的任务，添加到这里，任务系统会后续处理它们
	 * @param CurrentThread 当前执行任务的线程类型（比如游戏线程、后台线程等），便于任务根据线程做不同处理或调度。表示当前正在执行任务的线程类型，通常它就是调用这个接口的线程的类型。
	 */
	virtual void ExecuteTask(std::vector<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThread) = 0;


	/**
	 *  是系统调用的接口，用于 执行任务 同上
	 * @param NewTasks
	 * @param CurrentThread
	 */
	FORCEINLINE void Execute(std::vector<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThread)
	{
		assert(++LifeStage == int32(LS_Executing));
		ExecuteTask(NewTasks, CurrentThread);
	}


	/**
	 * 是任务图系统中用于将一个任务入队、等待执行的核心逻辑之一。这个函数是任务图系统中把“准备就绪”的任务送入调度队列的步骤。
	 * @param CurrentThreadIfKnown 一般就是调用这个接口线程的Type 传错一般不会影响任务执行正确性（一般来说）；但可能会失去一些线程亲和性优化；
	 */
	void QueueTask(ENamedThreads::Type CurrentThreadIfKnown)
	{
		assert(++LifeStage == (int32)(LS_Queued));
		FTaskGraphInterface::Get().QueueTask(this, ThreadToExecuteOn, CurrentThreadIfKnown);
	}

	ENamedThreads::Type	ThreadToExecuteOn;
	std::atomic<int32> NumberOfPrerequistitesOutstanding;


	//记录一个任务的生命周期
	enum ELifeStage
	{
		LS_BaseContructed = 0,
		LS_Contructed,
		LS_ThreadSet,
		LS_PrequisitesSetup,
		LS_Queued,
		LS_Executing,
		LS_Deconstucted,
	};
public:
	std::atomic<int32> LifeStage{0};

};



/**
 * 是 一个同步事件，可以认为是“多任务的等待目标”；
 * 本质上是一个前置任务完成的标志；
 * 一个任务完成时，可以通过 FGraphEvent 通知多个后继任务；
 * 生命周期由 引用计数 管理
 */
class COREMODULE FGraphEvent:public std::enable_shared_from_this<FGraphEvent>
{
public:

	/**
	 *
	 * @return 创建一个任务图事件（GraphEvent）对象，并返回一个引用计数智能指针 FGraphEventRef。
	 */
	static FGraphEventRef CreateGraphEvent();

	FGraphEvent()
		: ThreadToDoGatherOn(ENamedThreads::AnyHiPriThreadHiPriTask)
	{
	}
	~FGraphEvent();

	/**
	 *
	 * @return CreateGraphEventWithInlineStorage() 会创建一个 FGraphEvent* 指针，但不会**自动增加引用计数（ReferenceCount = 0）。
	 */
	static FGraphEvent* CreateGraphEventWithInlineStorage();


	bool AddSubsequent(FBaseGraphTask* Task)
	{
		// UE 的TClosableLockFreePointerListUnorderedSingleConsumer 有closed状态功能，我们这里IsClosed 假装一下
		if (IsClosed.load())
		{
			return false;
		}
		{
			FRWScopeLock<SLT_Write> RWScopeLock(SubsequentListLock);
			SubsequentList.push_front(Task);
		}
		return true;
	}

	void CheckDontCompleteUntilIsEmpty()
	{
		assert(!EventsToWaitFor.size());
	}


	/**
	 * 延迟当前事件的触发，直到指定的事件触发完成。
	 * 这里实际上只是把事件加进数组，没有直接在这个函数里等待
	 * @param EventToWaitFor  需要等待完成的事件。
	 */
	void DontCompleteUtil(FGraphEventRef EventToWaitFor)
	{
		EventsToWaitFor.push_back(EventToWaitFor);
	}


	/**
	 * 标记当前事件已经完成（所有前置条件满足，任务执行完毕）
	 * 获取所有依赖于当前事件的后续任务列表，并将该列表“关闭”，意味着不允许再添加新的后续任务。
	 * 对每个后续任务，减少它们等待的前置任务计数（剩余未完成的依赖数）
	 * 如果某个后续任务的剩余依赖数为零，说明它的所有前置任务已完成，则将该任务放入任务队列等待执行。
	 * @param CurrentThreadIfKnown 是个优化选项，告诉调度器“当前运行线程是哪个”，避免内部通过 TLS 查询，提升性能。
	 */
	void  DispatchSubsequents(ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread);


	/**上一版本的增强版，不仅处理原本的后续任务，还可以一次性添加一批新的后续任务 NewTasks。
	 *
	 * @param NewTasks
	 * @param CurrentThreadIfKnown
	 */
	void DispatchSubsequents(std::vector<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread);


	/**
	 * 为什么不直接使用 SubsequentList == 0 来判断呢？ 因为可能还没有任何任务添加进来
	 * @return
	 */
	bool IsComplete() const
	{
		return IsClosed.load();
	}
	void SetDebugName(const TCHAR* Name)
	{
		DebugName = Name;
	}

private:
	friend TRefCountPtr<FGraphEvent>;
	/**
	 * 负责将一个已经完成生命周期的 FGraphEvent 回收到任务图系统的事件池中，供后续复用。
	 * @param ToRecycle
	 */
	static void Recycle(FGraphEvent* ToRecycle);



private:

	//todo 暂时没有无锁结构，这里用RWLock保证安全先
	/**
	 * 用于存储所有依赖此事件的后继任务（Subsequents）。
	 * 用途：当当前 FGraphEvent 完成时，会遍历这个链表，把所有后继任务排队调度。
	 * 无锁结构：适合在多个线程中添加后继任务，但只允许 单个消费者线程取出（通常是事件完成时的那个线程）。
	 * 原来的里面还有个closed的标志，我们没有，所以我们用一个原子标志位
	 */
	std::forward_list<FBaseGraphTask*> SubsequentList;
	FRWLock SubsequentListLock;
	std::atomic_bool IsClosed;

	// 前置事件列表
	FGraphEventArray EventsToWaitFor;
	/*
	 * 指定在哪个线程执行「后继任务收集」操作。
	 * 可用于绑定 gather 操作到特定线程。
	 */
	ENamedThreads::Type	ThreadToDoGatherOn;


	const char* DebugName{ nullptr };

};



/**
 * TGraphTask 是一个模板类，包装了一个用户自定义的任务（TTask）。
 * 它将这个任务嵌入任务图系统中，并提供了设置前置任务（prerequisites）和后续任务（subsequents）的功能。
 * 所有图任务都会继承自 FBaseGraphTask，它提供了调度、引用计数等底层机制。
 * @tparam TTask 实际任务类型，比如一个继承了某些接口、实现了 DoTask 的结构体。
 */
template<typename TTask>
class TGraphTask final :public FBaseGraphTask
{
public:


	/**
	 * 一个 辅助构造类，用来完成任务构建和调度的逻辑。
	 */
	class FConstructor
	{
	public:
		/**
		 * 构造任务后立刻调度（如果满足条件）。
		 * @tparam T 
		 * @param Args 
		 * @return 
		 */
		template<typename...T>
		FGraphEventRef ConstructAndDispatchWhenReady(T&&... Args)
		{
			new (static_cast<void*>(&Owner->TaskStorage)) TTask(std::forward<T>(Args)...);
			return Owner->Setup(Prerequisites, CurrentThreadIfKnown);
		}


		/**
		 * 构造任务，标记为可执行或挂起状态（等待前置任务完成）。
		 * @tparam T 
		 * @param Args 
		 * @return 
		 */
		template<typename...T>
		TGraphTask* ConstructAndHold(T... Args)
		{
			new (static_cast<void*>(&Owner->TaskStorage)) TTask(std::forward<T>(Args)...);
			return Owner->Hold(Prerequisites, CurrentThreadIfKnown);
		}


	private:
		friend class TGraphTask;

		/**
		 * 指向外部的 TGraphTask<TTask> 实例的指针。
		 */
		TGraphTask* Owner;

		//指向前置任务事件的数组 当前任务在运行前必须等待完成的所有依赖任务。这些前置任务完成后，当前任务才能被安排执行。
		const FGraphEventArray* Prerequisites;

		/**
		 * 如果你已经知道当前线程的线程 ID，可以传入它以优化调度流程，避免还要从Tls中查询浪费性能
		 */
		ENamedThreads::Type CurrentThreadIfKnown;

		FConstructor(TGraphTask* InOwner, const FGraphEventArray* InPrerequisites, ENamedThreads::Type InCurrentThreadIfKnown)
			:Owner(InOwner)
			,Prerequisites(InPrerequisites)
			,CurrentThreadIfKnown(InCurrentThreadIfKnown)
		{
			
		}
		// 不允许复制拷贝
		FConstructor(const FConstructor& Other) = delete;
		void operator=(const FConstructor& Other) = delete;

	};


public:

	TGraphTask(FGraphEventRef InSubsequents, int32 NumberOfPrerequistitesOutStanding)
		:FBaseGraphTask(NumberOfPrerequistitesOutStanding)
		, TaskConstructed(false)
	{
		Subsequents.swap(InSubsequents);
	}
	~TGraphTask() override
	{
		assert(!TaskConstructed);
	}


	/**创建一个图任务（Graph Task）
	 * 
	 * @param Prerequistes 先决任务数组，表示此任务依赖哪些前置任务完成后才能执行。
	 * @param CurrentThreadIfKnown 当前线程的标识（如在主线程/渲染线程上调用）。如果不确定，传 AnyThread。
	 * @return 
	 */
	static FConstructor CreateTask(const FGraphEventArray* InPrerequistes = nullptr, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::Type::AnyThread)
	{
		int32 NumPrereq = InPrerequistes ? InPrerequistes->size() : 0;
		return FConstructor(new TGraphTask(TTask::GetSubsequentsMode() == ESubsequentsMode::FireAndForget ? nullptr :FGraphEvent::CreateGraphEvent(), NumPrereq ), InPrerequistes, CurrentThreadIfKnown);
	}


	/**专门用于创建一种特殊任务：Gather Task，也叫“合并任务”或“任务合流器”。
	 * 目的不是执行某个逻辑本身，而是合并多个任务的 Subsequents（后继任务列表），从而支持：在某个任务中调用 WaitFor() 等待其他任务，并将自己的后继挂到被等待任务之后。
	 * @param SubsequentsToAssume 指向某个任务的 FGraphEvent，从中“接管”它的所有后继任务。
	 * @param Prerequisites 本任务的依赖任务（可选）。
	 * @param CurrentThreadIfKnown 
	 * @return 
	 */
	static FConstructor CreateTask(FGraphEventRef SubsequentsToAssume, const FGraphEventArray* Prerequisites = NULL, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		return FConstructor(new TGraphTask(SubsequentsToAssume, Prerequisites ? Prerequisites->size() : 0), Prerequisites, CurrentThreadIfKnown);
	}


	void Unlock(ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		ConditionalQueueTask(CurrentThreadIfKnown);
	}

	FGraphEventRef GetCompletionEvent()
	{
		return Subsequents;
	}
private:
	friend class FConstructor;
	friend class FGraphEvent;


	/**执行嵌入任务（TTask::DoTask）；
	 *销毁嵌入任务（手动调用析构）；原本placement new 的，我们不需要真的delete
	 *调度后续任务（调用 Subsequents->DispatchSubsequents(...)）；
	 *销毁自身（回收内存）。
	 * 
	 * @param NewTasks 
	 * @param CurrentThread 
	 */
	virtual void ExecuteTask(std::vector<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThread) override
	{
		assert(TaskConstructed);
		if (TTask::GetSubsequentsMode() == ESubsequentsMode::TrackSubsequents)
		{
			// 准备执行Subsequents会被后续任务依赖的时候检查后续Subsequents事件的前置事件是不是空
			Subsequents->CheckDontCompleteUntilIsEmpty();
		}

		TTask& Task = *reinterpret_cast<TTask*>(&TaskStorage);
		{
			Task.DoTask(CurrentThread, Subsequents);
			Task.~TTask();
		}
		TaskConstructed = false;

		if (TTask::GetSubsequentsMode() == ESubsequentsMode::TrackSubsequents)
		{
			// 注意这里设置内存屏障，保证cpu乱序后，上面的写入对下面的线程逻辑可见
			FPlatformProcess::SetMemoryBarrier();
			Subsequents->DispatchSubsequents(NewTasks, CurrentThread);
		}

		delete this;
	}



	/**传入前置任务列表（如果有的话）；
	 * 指定当前线程编号（可用于优化）；
	 * 返回当前任务的“完成事件”（Subsequents）引用，用于被别的任务依赖。
	 * @param Prerequisites 
	 * @param CurrentThreadIfKnown 
	 * @return 返回当前任务的完成事件，这允许其他任务使用当前任务作为前置条件，形成依赖图。
	 */
	FGraphEventRef Setup(const FGraphEventArray* Prerequisites = nullptr, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		// 保存 Subsequents 的引用计数，避免接下来这玩意儿在任务中有什么问题被销毁了
		FGraphEventRef ReturnedEventRef = Subsequents;
		//注意 SetupPrereqs 是没有用到 Subsequents的
		SetupPrereqs(Prerequisites, CurrentThreadIfKnown, true);
		return ReturnedEventRef;
	}

	/**
	 * 将当前任务注册为所有前置任务（Prerequisites）的后继任务（Subsequent），并在前置任务全部完成后自动进入可执行状态。
	 * @param Prerequisites 
	 * @param CurrentThreadIfKnown 
	 * @param bUnlock 参数为 true，表示如果所有前置任务都已经完成，可以立即执行这个任务（立即 unlock）。
	 */
	void SetupPrereqs(const FGraphEventArray* Prerequisites, ENamedThreads::Type CurrentThreadIfKnown, bool bUnlock)
	{
		/*
		 * 标记为已构造状态。
		 */
		TaskConstructed = true;
		TTask& Task = *(TTask*)&TaskStorage;
		SetThreadToExecuteOn(Task.GetDesiredThread());
		int32 AlreadyCompletedPrerequisites = 0;
		if (Prerequisites)
		{
			for (int32 index = 0; index < Prerequisites->size(); index++)
			{
				auto& Prerequisite = (*Prerequisites)[index];
				if (!Prerequisite || !Prerequisite->AddSubsequent(this))
				{
					// 对于加入失败的前置任务或者前置任务已经为null，我们直接让AlreadyCompletedPrerequisites++
					AlreadyCompletedPrerequisites++;
				}
			}
		}
		// 尝试一下前置任务是不是全部完成了可以执行当前任务了
		PrerequisitesComplete(CurrentThreadIfKnown, AlreadyCompletedPrerequisites, bUnlock);
	}

	
	/**调用同样的依赖构建函数 SetupPrereqs()；
	 * 将 bUnlock = false，意味着即便所有前置任务都已完成，也不会立即调度本任务执行；
	 * 相当于“把任务放在那里，等我手动告诉你开始执行”。
	 * @param Prerequisites 
	 * @param CurrentThreadIfKnown 
	 * @return 返回 TGraphTask* 指针而不是 FGraphEventRef，表明你可能会手动管理或继续调用其他函数（如 Unlock()）。
	 */
	TGraphTask* Hold(const FGraphEventArray* Prerequisites = NULL, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		SetupPrereqs(Prerequisites, CurrentThreadIfKnown, false);
		return this;
	}


	/**
	 * 一个内存缓冲区，大小和对齐方式都严格匹配模板参数 TTask 类型
	 * 对齐缓冲区大小和对齐方式之后，才能在这块内存上放心使用placement new
	 */
	std::aligned_storage_t<sizeof(TTask), alignof(TTask)> TaskStorage;


	/**
	 * 标记任务是否已经构造
	 */
	bool TaskConstructed;

	/**
	 *若 Subsequents 为 null，说明没有后继任务，也可用作轻量任务优化（如 FireAndForget 模式）。
	 * 后续事件
	 */
	FGraphEventRef Subsequents;
};




#pragma region CustomTasks
/*
 * 各种自定义的任务
 */
/*
 * stat 任务
 */
class FCustomStatIDGraphTaskBase
{
public:

};
#pragma endregion
