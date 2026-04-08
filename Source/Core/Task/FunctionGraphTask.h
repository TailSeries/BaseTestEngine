#pragma once
#include "TaskGraphInterfaces.h"

/**
 * 通用的一个执行函数式任务的帮助模板，
 * @tparam Signature 
 * @tparam SubsequentsMode 
 */
template<typename Signature, ESubsequentsMode::Type SubsequentsMode>
class TFunctionGraphTaskImpl
{
private:
	std::function<Signature> Function;
	const ENamedThreads::Type DesiredThread;
public:
	TFunctionGraphTaskImpl(std::function<Signature>&& InFunction, ENamedThreads::Type InDesiredThread)
		:Function(InFunction),DesiredThread(InDesiredThread)
	{
	}
	ENamedThreads::Type GetDesiredThread() const
	{
		return DesiredThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode()
	{
		return SubsequentsMode;
	}

	FORCEINLINE void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		DoTaskImpl(Function, CurrentThread, MyCompletionGraphEvent);
	}

private:
	FORCEINLINE static void DoTaskImpl(std::function<void()>& Function, ENamedThreads::Type CurrentThread,
		const FGraphEventRef& MyCompletionGraphEvent)
	{
		Function();
	}

	FORCEINLINE static void DoTaskImpl(std::function<void(const FGraphEventRef&)>& Function,
		ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Function(MyCompletionGraphEvent);
	}

	FORCEINLINE static void DoTaskImpl(std::function<void(ENamedThreads::Type, const FGraphEventRef&)>& Function,
		ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Function(CurrentThread, MyCompletionGraphEvent);
	}
};

// 进一步直接封装一个FunctionGraphTask
struct FFunctionGraphTask
{
public:
	static FGraphEventRef CreateAndDispatchWhenReady(std::function<void()> InFunction, const FGraphEventArray* InPrerequisites = nullptr, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
	{
		return TGraphTask<TFunctionGraphTaskImpl<void(), ESubsequentsMode::TrackSubsequents>>::CreateTask(InPrerequisites).ConstructAndDispatchWhenReady(std::move(InFunction), InDesiredThread);
	}

	static FGraphEventRef CreateAndDispatchWhenReady(std::function<void(ENamedThreads::Type, const FGraphEventRef&)> InFunction, const FGraphEventArray* InPrerequisites = nullptr, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
	{
		return TGraphTask<TFunctionGraphTaskImpl<void(ENamedThreads::Type, const FGraphEventRef&), ESubsequentsMode::TrackSubsequents>>::CreateTask(InPrerequisites).ConstructAndDispatchWhenReady(std::move(InFunction), InDesiredThread);
	}
	static FGraphEventRef CreateAndDispatchWhenReady(std::function<void()> InFunction,  const FGraphEventRef& InPrerequisite, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
	{
		FGraphEventArray Prerequisites;
		Prerequisites.push_back(InPrerequisite);
		return CreateAndDispatchWhenReady(std::move(InFunction), &Prerequisites, InDesiredThread);
	}

	static FGraphEventRef CreateAndDispatchWhenReady(std::function<void(ENamedThreads::Type, const FGraphEventRef&)> InFunction, const FGraphEventRef& InPrerequisite, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
	{
		FGraphEventArray Prerequisites;
		Prerequisites.push_back(InPrerequisite);
		return CreateAndDispatchWhenReady(std::move(InFunction), &Prerequisites, InDesiredThread);
	}
};
