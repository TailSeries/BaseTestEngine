#pragma once
#include "TaskGraphInterfaces.h"
#include "Threads/Event.h"
/**封装一个“触发 FEvent 的任务”。
当这个任务被 TaskGraph 调度执行时，它会调用 Event->Trigger()，从而唤醒等待这个 FEvent 的其他线程。
 */
class FTriggerEventGraphTask
{
public:
	FTriggerEventGraphTask(FEvent* Inevent, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyHiPriThreadHiPriTask);

	ENamedThreads::Type GetDesiredThread()
	{
		return DesiredThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);

private:
	FEvent* Event;
	ENamedThreads::Type DesiredThread;
};

