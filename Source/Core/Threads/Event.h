#pragma once
/*
 * 模拟windows上的eventhandle的机制
 * 1.windows Event 有两种类型
 *	1.1 Manual-reset Event 事件被设置为信号态后，保持信号态，直到显式重置.用于多个线程等待某个全局条件成立，例如“资源加载完毕”。
 *		比如现在你有两个 wait，在线程里调用一次SetEvent
 *		WaitForSingleObject(gEvent, INFINITE); // 第一次：能收到，
		WaitForSingleObject(gEvent, INFINITE); // 第二次：也能收到
 *	1.2 Auto-reset Event 	一旦有线程等待并被唤醒，事件会自动重置为非信号态。用于 适合单个线程等待的场景，比如生产者消费者。
 *		比如现在你有两个 wait，在线程里调用一次SetEvent
 *		WaitForSingleObject(gEvent, INFINITE); // 第一次：能收到，随后event被自动重置为非信号态
		WaitForSingleObject(gEvent, INFINITE); // 第二次：会一直卡住，直到再次 SetEvent

	类似标准库的condition_variable

 */

#include "Core.h"
#include "Base/Atomic.h"

#if PLATFORM_WIN
#include "Windows.h"
#endif


class COREMODULE FEvent
{
public:
	/**
	 * 
	 * @param bIsManualReset 控制事件是否手动重置
	 * @return 
	 */
	virtual bool Create(bool bIsManualReset = false) = 0;

	
	/**
	 *  事件是否需要手动重置
	 * @return 
	 */
	virtual bool IsManualReset() = 0;


	/**
	 * 触发事件,唤醒等待这个事件的线程
	 */
	virtual void Trigger() = 0;


	/**
	 * 相当于 ResetEvent
	 */
	virtual void Reset() = 0;

	/**
	 * 等待一定时间，一直到这个event被触发
	 * @param WaitTime 需要等待的毫秒数
	 * @param bIgnoreThreadIdleStats 是否忽略统计当前线程空闲时间
	 * @return 
	 */
	virtual bool Wait(uint32 WaitTime, const bool bIgnoreThreadIdleStats = false) = 0;


	/*
	 * 无限等待一个非常长的时间
	 */
	bool Wait()
	{
		return Wait(TNumericLimits<uint32>::max());
	}


	FEvent()
		: EventId(0)
		, EventStartCycles(0)
	{
	}
	virtual ~FEvent()
	{
	}


protected:


protected:
	/*用于给每个创建的事件生成一个独立的唯一标识符（ID）。*/
	static TAtomic<uint32> EventUniqueId;
	uint32 EventId;
	/*记录这个事件的cpu的等待了多少cpu周期 0.2 ~ 1 ns*/
	/*
	 * 只能记录几秒，但不等于Event只能等待这么点时间，这只是用来性能分析的
	 */
	TAtomic<uint32> EventStartCycles;

};

enum class EEventMode { AutoReset, ManualReset };


#if PLATFORM_WIN

class COREMODULE FEventWin : public FEvent
{
public:
	FEventWin()
		: Event(nullptr)
	{
	}
	virtual ~FEventWin()
	{
		if (Event != nullptr)
		{
			CloseHandle(Event);
		}
	}
	virtual bool Create(bool bIsManualReset = false) override
	{
		Event = CreateEvent(nullptr, bIsManualReset, 0, nullptr);
		ManualReset = bIsManualReset;

		return Event != nullptr;
	}
	virtual bool IsManualReset() override
	{
		return ManualReset;
	}

	virtual void Trigger() override;
	virtual void Reset() override;
	virtual bool Wait(uint32 WaitTime, const bool bIgnoreThreadIdleStats = false) override;

private:
	HANDLE Event;
	bool ManualReset;
};

#endif
class COREMODULE FSingleThreadEvent : public FEvent
{
	bool bTriggered;
	bool bManualReset;
public:
	FSingleThreadEvent()
		: bTriggered(false)
		, bManualReset(false)
	{
	}

	virtual bool Create(bool bIsManualReset = false) override
	{
		bManualReset = bIsManualReset;
		return true;
	}

	virtual bool IsManualReset() override
	{
		return bManualReset;
	}

	virtual void Trigger() override
	{
		bTriggered = true;
	}

	virtual void Reset() override
	{
		bTriggered = false;
	}
	virtual bool Wait(uint32 WaitTime, const bool bIgnoreThreadIdleStats = false) override
	{
		bTriggered = bManualReset;
		return true;
	}

};


class COREMODULE FScopedEvent
{
public:

	/** Default constructor. */
	 FScopedEvent();

	/** Destructor. */
	 ~FScopedEvent();

	/** Triggers the event. */
	void Trigger()
	{
		Event->Trigger();
	}

	/**
	 * Checks if the event has been triggered (used for special early out cases of scope event)
	 * if this returns true once it will return true forever
	 *
	 * @return returns true if the scoped event has been triggered once
	 */
	 bool IsReady();

	/**
	 * Retrieve the event, usually for passing around.
	 *
	 * @return The event.
	 */
	FEvent* Get()
	{
		return Event;
	}

private:

	/** Holds the event. */
	FEvent* Event;
};
