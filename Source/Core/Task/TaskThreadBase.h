#pragma once
#include "Core.h"
#include "TaskGraphInterfaces.h"
#include "WorkerThread.h"
#include "Threads/Runnable.h"
#include "Threads/RunnableThread.h"


/**
 * 在任务图系统中，不同类型的线程逻辑（如 WorkerThread、NamedThread）都会基于它实现。
 * 它不负责调度，但负责从任务队列中拉取任务并执行。
 */
class FTaskThreadBase:public FRunnable, FSingleThreadRunnable
{
public:
	FTaskThreadBase();


	/**
	 * 这是给某个线程对象（FTaskThreadBase）初始化基本信息的函数，通常在线程刚创建或者附加时调用。
	 * 说明这个函数的调用者不是线程自身，而是主线程或者管理线程（比如调度线程），避免并发冲突。
	 * 这里还会创建一个“stall event”，用来通知线程等待任务或其他同步信号。
	 * @param InThreadId 
	 * @param InPerThreadIDTLSSlot 
	 * @param InOwnerWorker 
	 */
	void Setup(ENamedThreads::Type InThreadId, uint32 InPerThreadIDTLSSlot, FWorkerThread* InOwnerWorker);


	/**
	 * 在当前运行的线程（即“this thread”）中初始化 TLS (Thread Local Storage) 的值。
	 * 一次性调用 TLS槽位是线程私有的，全局只分配一个槽号，但每个线程都要独立设置自己的 TLS 内容。这个函数只需要在线程启动或附加后调用一次，完成 TLS 初始化。
	 */
	void InitializeForCurrentThread();

	ENamedThreads::Type GetThreadId() const;


	/**
	 * 给**Named Thread（具名线程）**专门设计的任务执行循环接口。
	 * 该线程会不断处理 QueueIndex 指定的任务队列里的任务
	 * 线程会一直循环执行，直到满足两个条件：任务队列空闲（idle） 外部调用了 RequestQuit（请求线程退出）
	 * @param QueueIndex 
	 */
	virtual void ProcessTasksUntilQuit(int32 QueueIndex) = 0;


	/**
	 *给**Named Thread（具名线程）**专门设计的任务执行循环接口。
	 * 该函数应该用于让线程持续处理任务直到“空闲”，
	 * @return 已经处理的任务数量
	 */
	virtual uint64 ProcessTasksUntilIdle(int32 QueueIndex);


	/**
	 * 将任务 Task 直接加入任务队列中。并且假设调用此函数的线程就是这个任务队列对应的线程。
	 * 对于 Named threads，直接加入私有队列
	 * @param QueueIndex 指定任务队列索引，可能是多队列管理的一部分。
	 * @param Task 待加入的任务。
	 */
	virtual void EnqueueFromThisThread(int32 QueueIndex, FBaseGraphTask* Task) {};



	/**
	 * 请求当前任务线程退出，在任务处理完成、线程变为 idle 后返回控制权。它并不是立即终止线程，而是发出一个“优雅退出”的请求。
	 * @param QueueIndex 指定任务队列索引
	 */
	virtual void RequestQuit(int32 QueueIndex) = 0;


	/**
	 * 当你在“非当前线程”中向这个线程的任务队列中添加任务时调用。
	 * @param QueueIndex 
	 * @param Task 
	 * @return 
	 */
	virtual bool EnqueueFromOtherThread(int32 QueueIndex, FBaseGraphTask* Task)
	{
		return false;
	}

	virtual void WakeUp(int32 QueueIndex = 0) = 0;


	/**
	 * ，其作用是判断某个线程是否正在处理任务：
	 * @param QueueIndex  指定任务队列索引 多线程任务系统中，每个线程可能对应多个任务队列（例如不同优先级），所以要指定具体查询哪个队列。
	 * @return 
	 */
	virtual bool IsProcessingTasks(int32 QueueIndex) = 0;



protected:
	ENamedThreads::Type ThreadId; // 当前线程的类型id
	// TLS 槽位，每个线程启动后将自己的FTaskThreadBase* 放进这个槽里，这样就可以在线程运行的时候随时通过TLS拿线程实例了
	uint32 PerThreadIDTLSSlot;
	// 用于标记线程是否处于“停滞（stall）”状态, 不能用来标记线程同步
	std::atomic<int32> IsStalled;
	// 当前线程的任务
	std::vector<FBaseGraphTask*> NewTasks;

	// 我们用轻量结构 FWorkerThread 来保存线程，这里追寻一下自己的owner
	FWorkerThread* OwnerWorker;



public://FRunnable 的行为

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual  void Stop() override;
	virtual void Exit() override;
	virtual FSingleThreadRunnable* GetSingleThreadInterface() override;



public://FSingleThreadRunnable的行为
	virtual void Tick() override;



};


