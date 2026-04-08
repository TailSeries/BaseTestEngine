#pragma once
#include "TaskGraphInterfaces.h"
/**什么都不做的任务，执行时不会有实际工作
 *主要用来作为“占位符”或“聚合点”，将多个任务“聚合”成一个前置条件。
 *比如你有多个任务想要等待它们全部完成，可以用一个 FNullGraphTask 作为它们的共同前置任务，方便管理依赖关系。
 */
class FNullGraphTask : public FCustomStatIDGraphTaskBase
{
public:
	//todo 我们暂时还没有用来分析的statid，先不用吧
	FNullGraphTask(ENamedThreads::Type InDesiredThread)
		:DesiredThread(InDesiredThread)
	{

	}
	virtual ~FNullGraphTask() {}
	ENamedThreads::Type GetDesiredThread()
	{
		return DesiredThread;
	}

	// 注意因为TaskGraph的限制，这个函数必须是静态函数
	static ESubsequentsMode::Type GetSubsequentsMode()
	{
		//当这个任务完成时，系统会关注它的后续任务（subsequents），保证它们能正确触发执行
		return ESubsequentsMode::TrackSubsequents;
	}


	virtual void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		// do nothing
	}

private:
	/** Thread to run on, can be ENamedThreads::AnyThread **/
	ENamedThreads::Type DesiredThread;
};

