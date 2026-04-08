#pragma once
#include "TaskGraphInterfaces.h"


/**
 * TaskGraph 系统里，线程有两种常见运行模式：
 * 1.ProcessThreadUntilIdle：处理完当前队列里的任务就退出。
 * 2.ProcessThreadUntilRequestReturn：线程会一直运行任务，直到收到显式的“返回请求”。当线程运行在第 2 种模式时，就需要一个“标记”来告诉它 “该停了，可以返回调用者了”。
 *
 * 它就是这个“标记任务”。本质上是一个特殊的 FBaseGraphTask，当 TaskGraph 调度它时，不会执行实际逻辑，而是告诉调度循环 “触发返回”
 * 这样 ProcessThreadUntilRequestReturn 就能跳出循环，把控制权还给原始调用者。
 */
class FReturnGraphTask
{
public:
	FReturnGraphTask(ENamedThreads::Type InThreadToReturnFrom);


	ENamedThreads::Type GetDesiredThread();

	static ESubsequentsMode::Type GetSubsequentsMode();

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);

private:


	/**
	 * 表示 哪个 NamedThread 要被要求“返回”
	 */
	ENamedThreads::Type ThreadToReturnFrom;
	
};