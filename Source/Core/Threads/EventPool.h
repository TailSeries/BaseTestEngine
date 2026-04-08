#pragma once
#include <forward_list>

#include "Event.h"
#include "ScopeLock.h"

enum class COREMODULE EEventPoolTypes
{
	AutoReset,
	ManualReset
};

/*
 * 一个轻量的 包装器类,内部持有一个 FEvent*,这个FEvent*的真实所有权是EventPool中的
 * 线程安全封装；
 * 析构时自动归还事件对象到池中；
 * 操作接口透明转发给内部的 FEvent（如 Wait, Trigger, Reset 等）。
 * 
 */
class COREMODULE FSafeRecyclableEvent  final : public FEvent
{
public:
	FEvent* InnerEvent;

	FSafeRecyclableEvent(FEvent* InInnerEvent)
		: InnerEvent(InInnerEvent)
	{
	}

	~FSafeRecyclableEvent()
	{
		InnerEvent = nullptr;
	}

	virtual bool Create(bool bIsManualReset = false)
	{
		return InnerEvent->Create(bIsManualReset);
	}

	virtual bool IsManualReset()
	{
		return InnerEvent->IsManualReset();
	}

	virtual void Trigger()
	{
		InnerEvent->Trigger();
	}

	virtual void Reset()
	{
		InnerEvent->Reset();
	}

	virtual bool Wait(uint32 WaitTime, const bool bIgnoreThreadIdleStats = false)
	{
		return InnerEvent->Wait(WaitTime, bIgnoreThreadIdleStats);
	}
};

template<EEventPoolTypes PoolType>
class FEventPool
{
public:
	FEventPool()=default;
	static FEventPool<PoolType>& Get()
	{
		static FEventPool<PoolType> ins;
		return ins;
	};

	~FEventPool()
	{
		EmptyPool();
	}
	void EmptyPool()
	{
		while (FEvent* Event = Pool.front())
		{
			Pool.pop_front();
			delete Event;
		}
	}

	FEvent* GetEventFromPool()
	{
		
		FEvent* Result = nullptr;
		{
			FScopeLock ScopeLock(&PoolCriticalSection);
			if (!Pool.empty())
			{
				Result = Pool.front();
				Pool.pop_front();
			}
		}

		if (!Result)
		{
			Result = FPlatformProcess::CreateSynchEvent((PoolType == EEventPoolTypes::ManualReset));
		}
		return new FSafeRecyclableEvent(Result);
	}
	void ReturnToPool(FEvent* Event)
	{
		FScopeLock ScopeLock(&PoolCriticalSection);
		FSafeRecyclableEvent* SafeEvent = (FSafeRecyclableEvent*)Event;
		FEvent* Result = SafeEvent->InnerEvent;
		delete SafeEvent;
		Result->Reset();
		Pool.push_front(Result);
	}
private:
	//todo 这里我们没有实现无锁队列，所以我们这里用个锁，后面优化
	FCriticalSection PoolCriticalSection;
	std::forward_list<FEvent*> Pool;
};