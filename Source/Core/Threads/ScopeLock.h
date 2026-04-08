#pragma once
#include "Core.h"
#if PLATFORM_WIN
	#include "Threads/Win/WindowsSyncUtils.h"
#endif


/*基于CriticalSection实现, 注意lock没有接管section的生命周期*/
/*
 * 进入作用域加锁，离开作用域解锁
 */
class COREMODULE FScopeLock
{
public:
	FScopeLock(FCriticalSection* InSynchObject)
		: SynchObject(InSynchObject)
	{
		SynchObject->Lock();
	}
	~FScopeLock()
	{
		Unlock();
	}

	void Unlock()
	{
		if (SynchObject)
		{
			SynchObject->Unlock();
			SynchObject = nullptr;
		}
	}
private:

	FScopeLock()=default;


	FScopeLock(const FScopeLock& InScopeLock) = default;
	FScopeLock& operator=(FScopeLock& InScopeLock)
	{
		return *this;
	}
private:
	FCriticalSection* SynchObject;
};




/*进入作用域解锁，离开作用域加锁*/
class FScopeUnlock
{
public:
	FScopeUnlock(FCriticalSection* InSynchObject)
		: SynchObject(InSynchObject)
	{
		if (InSynchObject)
		{
			InSynchObject->Unlock();
		}
	}

	~FScopeUnlock()
	{
		if (SynchObject)
		{
			SynchObject->Lock();
		}
	}

private:
	FScopeUnlock()=default;
	FScopeUnlock(const FScopeUnlock& InScopeLock) = default;
	FScopeUnlock& operator=(FScopeUnlock& InScopeLock)
	{
		return *this;
	}
private:
	FCriticalSection* SynchObject;
};


/*
 * 读写的作用域锁
 */
class FReadScopeLock
{
public:
	explicit FReadScopeLock(FRWLock& InLock)
		: Lock(InLock)
	{
		Lock.ReadLock();
	}

	~FReadScopeLock()
	{
		Lock.ReadUnlock();
	}

protected:
	FRWLock& Lock;

	NONCOPYABLE(FReadScopeLock);
};


class FWriteScopeLock
{
public:
	explicit FWriteScopeLock(FRWLock& InLock)
		: Lock(InLock)
	{
		Lock.WriteLock();
	}

	~FWriteScopeLock()
	{
		Lock.WriteUnlock();
	}

protected:
	FRWLock& Lock;

	NONCOPYABLE(FWriteScopeLock);
};


enum FRWScopeLockType
{
	SLT_ReadOnly = 0,
	SLT_Write,
};


template<FRWScopeLockType LockType>
struct SelectLockType;

template<>
struct SelectLockType<FRWScopeLockType::SLT_ReadOnly>
{
	using LockType = FReadScopeLock;
};


template<>
struct SelectLockType<FRWScopeLockType::SLT_Write>
{
	using LockType = FWriteScopeLock;
};

template<FRWScopeLockType LockType>
class FRWScopeLock:public SelectLockType<LockType>::LockType
{
	using Super = typename  SelectLockType<LockType>::LockType;
public:

	explicit FRWScopeLock(FRWLock& InLock)
		:Super(InLock)
	{
		
	}
	
};