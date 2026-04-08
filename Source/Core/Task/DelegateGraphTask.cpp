#include "DelegateGraphTask.h"

FGraphEventRef FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(const std::function<void()>& InTaskDelegate, const FGraphEventArray* InPrerequisites, ENamedThreads::Type InDesiredThread)
{
	return TGraphTask<FSimpleDelegateGraphTask>::CreateTask(InPrerequisites).ConstructAndDispatchWhenReady<const std::function<void()>&>(InTaskDelegate, InDesiredThread);
}

FGraphEventRef FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(const std::function<void()>& InTaskDeletegate, const FGraphEventRef& InPrerequisite, ENamedThreads::Type InDesiredThread)
{
	FGraphEventArray Prerequisites;
	Prerequisites.push_back(InPrerequisite);
	return CreateAndDispatchWhenReady(InTaskDeletegate, &Prerequisites, InDesiredThread);

}




FGraphEventRef FDelegateGraphTask::CreateAndDispatchWhenReady(const std::function<void(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)>& InTaskDelegate, const FGraphEventArray* InPrerequisites, ENamedThreads::Type InDesiredThread)
{
	return TGraphTask<FDelegateGraphTask>::CreateTask(InPrerequisites).ConstructAndDispatchWhenReady<const std::function<void(ENamedThreads::Type, const FGraphEventRef&)>&>(InTaskDelegate, InDesiredThread);

}

FGraphEventRef FDelegateGraphTask::CreateAndDispatchWhenReady(const std::function<void(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)>& InTaskDeletegate, const FGraphEventRef& InPrerequisite, ENamedThreads::Type InDesiredThread)
{
	FGraphEventArray Prerequisites;
	Prerequisites.push_back(InPrerequisite);
	return CreateAndDispatchWhenReady(InTaskDeletegate, &Prerequisites, InDesiredThread);
}
