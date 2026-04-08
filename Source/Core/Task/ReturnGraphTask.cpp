#include "ReturnGraphTask.h"

FReturnGraphTask::FReturnGraphTask(ENamedThreads::Type InThreadToReturnFrom)
	:ThreadToReturnFrom(InThreadToReturnFrom)
{
	assert(ThreadToReturnFrom != ENamedThreads::Type::AnyThread);
}

ENamedThreads::Type FReturnGraphTask::GetDesiredThread()
{
	return ThreadToReturnFrom;
}

ESubsequentsMode::Type FReturnGraphTask::GetSubsequentsMode()
{
	return ESubsequentsMode::TrackSubsequents;
}

void FReturnGraphTask::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	// 执行任务的线程和设定要结束任务的线程必须是同一个线程
	assert(ENamedThreads::GetThreadIndex(ThreadToReturnFrom) == ENamedThreads::GetThreadIndex(CurrentThread));
	//向 TaskGraph 发出信号：“让这个线程退出 ProcessThreadUntilRequestReturn() 循环。”
	FTaskGraphInterface::Get().RequestReturn(ThreadToReturnFrom);
}


