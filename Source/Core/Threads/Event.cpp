#include "Event.h"

#include "EventPool.h"

void FEventWin::Trigger()
{
	SetEvent(Event);
}

void FEventWin::Reset()
{
	ResetEvent(Event);
}

bool FEventWin::Wait(uint32 WaitTime, const bool bIgnoreThreadIdleStats)
{
	return (WaitForSingleObject(Event, WaitTime) == WAIT_OBJECT_0);
}

FScopedEvent::FScopedEvent()
	:Event(FEventPool<EEventPoolTypes::AutoReset>::Get().GetEventFromPool())
{

}

FScopedEvent::~FScopedEvent()
{
	if (Event)
	{
		Event->Wait();
		FEventPool<EEventPoolTypes::AutoReset>::Get().ReturnToPool(Event);
		Event = nullptr;
	}
}

bool FScopedEvent::IsReady()
{
	if (Event)
	{
		if (Event->Wait(1))
		{
			FEventPool<EEventPoolTypes::AutoReset>::Get().ReturnToPool(Event);
			Event = nullptr;
			return true;
		}
		return false;
	}
	return true;
}
