#pragma once
/*windows上的线程同步工具*/
#if PLATFORM_WIN
#include "Core.h"

#include <Windows.h>


/*
 * 同一个线程可以多次进入临界区，多次解锁，只要加锁次数 == 解锁次数
 * 临界区 是同一个进程内部使用，保证只有一个线程可以申请到该对象
 */
class COREMODULE FWindowsCriticalSection
{

	CRITICAL_SECTION CriticalSection;
public:
	FWindowsCriticalSection()
	{
		// 创建一个windows的临界区对象，并且这个临界区对象的自选次数是4k
		/*设置为 4000 的意思是：线程将尝试 4000 次进入临界区，如果仍无法获得锁，再挂起等待。
		 * 在资源冲突短暂且频繁的场景下提高性能；
		 */
		InitializeCriticalSection(&CriticalSection);
		SetCriticalSectionSpinCount(&CriticalSection, 4000);
	}

	~FWindowsCriticalSection()
	{
		//删除临界区对象
		DeleteCriticalSection(&CriticalSection);
	}

	//上锁 	阻塞直到获得锁（可能导致线程等待）
	void Lock()
	{
		EnterCriticalSection(&CriticalSection);
	}

	/*
	 * 如果当前线程能立即获得锁，返回 TRUE 如果锁正被其他线程持有，返回 FALSE（不会阻塞等待）。
	 * 	不阻塞，立即返回锁是否获取成功
	 */
	bool TryLock()
	{
		if (TryEnterCriticalSection(&CriticalSection))
		{
			return true;
		}
		return false;
	}
	void Unlock()
	{
		LeaveCriticalSection(&CriticalSection);
	}
private:
	FWindowsCriticalSection(const FWindowsCriticalSection&) = default;
	FWindowsCriticalSection& operator=(const FWindowsCriticalSection&) = default;
};


/*
 * windows 上的mutex 是跨进程的，互斥量是可以命名的 ，互斥量一旦被创建，就可以通过名字打开它。
 * 可以跨越进程使用。所以创建互斥量需要的资源更多，性能比上面稍差，所以如果只为了在进程内部使用的话，使用临界区会带来速度上的优势并能够减少资源占用量
 * // 示例：
FWindowsSystemWideCriticalSection SystemLock(TEXT("MyGame_Unique_Mutex"), 5.0f);

if (SystemLock.IsValid())
{
    // 成功获取锁：可以安全运行
}
else
{
    // 获取失败：说明已有实例运行（或被其他进程锁定）
}
 */
class COREMODULE FWindowsSystemWideCriticalSection
{
public:
	//构造一个 具名，跨进程的的临界区对象，并且立即获得它的所有权
	explicit FWindowsSystemWideCriticalSection(const FString& InName, FTimespan<> InTimeout = FTimespan<>::zero());
	virtual ~FWindowsSystemWideCriticalSection();
	/*
	 * 当前线程是否拥有这个临界区对象的所有权
	 */
	bool IsValid() const;

	/*
	 * 如果当前拥有该系统范围的临界区（锁），则释放它。
	 */
	void Release();
private:
	FWindowsSystemWideCriticalSection(const FWindowsSystemWideCriticalSection&) = default;
	FWindowsSystemWideCriticalSection& operator=(const FWindowsSystemWideCriticalSection&) = default;

private:
	HANDLE Mutex;
};

/*读写锁*/
/*
 * 一个线程不能多次获取同一个锁（无论读锁还是写锁），否则会死锁,.每次加锁必须成对解锁。
 * 写锁是排他的：加了写锁后，其它线程无论是读锁还是写锁都要等待。
 * 适用于 读多写少 的资源场景
 */
class COREMODULE FWindowsRWLock
{
public:
	FWindowsRWLock(uint32 Level = 0)
	{
		InitializeSRWLock(&Mutex);
	}
	~FWindowsRWLock()
	{
	}

	void ReadLock()
	{
		AcquireSRWLockShared(&Mutex);
	}

	void WriteLock()
	{
		AcquireSRWLockExclusive(&Mutex);
	}
	void ReadUnlock()
	{
		ReleaseSRWLockShared(&Mutex);
	}

	void WriteUnlock()
	{
		ReleaseSRWLockExclusive(&Mutex);
	}

private:
	SRWLOCK Mutex;
};


/*
 * 注意：CriticalSection 以及 Mutex都在析构函数里做了操作（DeleteCriticalSection  	ReleaseMutex(Mutex);）
 * 在RWLock则不需要，对于RWLock来讲，其成员SRWLOCK Mutex;与该对象生命周期一致。
 * Windows 下的 Mutex 是一个内核对象，通过 CreateMutex 返回一个 HANDLE（句柄）。它需要手动关闭，是标准的资源管理流程。
 *
 */

typedef FWindowsCriticalSection FCriticalSection;
typedef FWindowsSystemWideCriticalSection FSystemWideCriticalSection;
typedef FWindowsRWLock FRWLock;


/*线程局部存储相关的内容*/
struct COREMODULE FGenericPlatformTLS
{
	static FORCEINLINE bool IsValidTlsSlot(uint32 SlotIndex)
	{
		return SlotIndex != 0xFFFFFFFF;
	}
};

struct COREMODULE FWindowsPlatformTLS :public FGenericPlatformTLS
{
	static FORCEINLINE uint32 GetCurrentThreadId(void)
	{
		return ::GetCurrentThreadId();
	}

	static FORCEINLINE uint32 AllocTlsSlot(void)
	{
		return TlsAlloc();
	}

	static FORCEINLINE void SetTlsValue(uint32 SlotIndex, void* Value)
	{
		TlsSetValue(SlotIndex, Value);
	}
	static FORCEINLINE void* GetTlsValue(uint32 SlotIndex)
	{
		return TlsGetValue(SlotIndex);
	}

	static FORCEINLINE void FreeTlsSlot(uint32 SlotIndex)
	{
		TlsFree(SlotIndex);
	}
};
typedef FWindowsPlatformTLS FPlatformTLS;
#endif
