#pragma once
#include "TaskGraphInterfaces.h"

/**
 * std::function<void()> 的简单任务
 */
class FSimpleDelegateGraphTask
{
public:
	std::function<void()> TaskDelegate;
	const ENamedThreads::Type DesiredThread;

	FSimpleDelegateGraphTask(const std::function<void()>& InTaskDeletegate, ENamedThreads::Type InDesiredThread)
		: TaskDelegate(InTaskDeletegate)
		, DesiredThread(InDesiredThread)
	{

	}
	ENamedThreads::Type GetDesiredThread()
	{
		return DesiredThread;
	}
	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		TaskDelegate();
	}

	/**
	 * 传入多个依赖（InPrerequisites）。
	 * 当所有依赖完成后，调度执行这个任务。
	 * 返回 FGraphEventRef，可供后续任务依赖。
	 * @param InTaskDelegate 
	 * @param InPrerequisites 
	 * @param InDesiredThread 
	 * @return 
	 */
	static FGraphEventRef CreateAndDispatchWhenReady(const std::function<void()>& InTaskDelegate, const FGraphEventArray* InPrerequisites = NULL, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread);


	static FGraphEventRef CreateAndDispatchWhenReady(const std::function<void()>& InTaskDeletegate, const FGraphEventRef& InPrerequisite, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread);

};


/**
 *  它允许在委托里直接访问 当前线程和任务完成事件，
 */
class FDelegateGraphTask
{
public:
	std::function<void(ENamedThreads::Type, const FGraphEventRef&)> TaskDelegate;
	const ENamedThreads::Type DesiredThread;
	FDelegateGraphTask(const std::function<void(ENamedThreads::Type, const FGraphEventRef&)>& InTaskDeletegate, ENamedThreads::Type InDesiredThread)
		: TaskDelegate(InTaskDeletegate)
		, DesiredThread(InDesiredThread)
	{

	}
	ENamedThreads::Type GetDesiredThread()
	{
		return DesiredThread;
	}
	static ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::TrackSubsequents;
	}
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		TaskDelegate(CurrentThread, MyCompletionGraphEvent);
	}
	static FGraphEventRef CreateAndDispatchWhenReady(const std::function<void(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)>& InTaskDelegate, const FGraphEventArray* InPrerequisites = NULL, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread);
	static FGraphEventRef CreateAndDispatchWhenReady(const std::function<void(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)>& InTaskDeletegate, const FGraphEventRef& InPrerequisite, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread);
};
