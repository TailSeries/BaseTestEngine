#include "TriggerEventGraphTask.h"



FTriggerEventGraphTask::FTriggerEventGraphTask(FEvent* Inevent, ENamedThreads::Type InDesiredThread)
	:Event(Inevent)
	,DesiredThread(InDesiredThread)
{

}

void FTriggerEventGraphTask::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	Event->Trigger();
}
