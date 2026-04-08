#include "WindowsSyncUtils.h"
#if PLATFORM_WIN
FWindowsSystemWideCriticalSection::FWindowsSystemWideCriticalSection(const FString& InName, FTimespan<> InTimeout)
{
	FString NormalizedMutexName(InName);


	Mutex = CreateMutex(NULL, true, NormalizedMutexName.c_str());

	// 如果这个mutex已经被其他进程占用了
	if (Mutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS)
	{
		bool bMutexOwned = false;

		if (InTimeout != FTimespan<>::zero())
		{
			/*
			 *等待这个mutex
			 */
			DWORD WaitResult = WaitForSingleObject(Mutex, FTimespanCast<1, 1000>(InTimeout).count());

			/*
			 * 拿到了 Mutex（WAIT_OBJECT_0）
			 * 前一个持有者意外退出（WAIT_ABANDONED）
			 * 超时（WAIT_TIMEOUT）
			 */
			if (WaitResult == WAIT_ABANDONED || WaitResult == WAIT_OBJECT_0)
			{
				bMutexOwned = true;
			}
		}

		if (!bMutexOwned)
		{
			// 如果失败了，清理句柄避免泄漏
			CloseHandle(Mutex);
			Mutex = NULL;
		}

	}

}
FWindowsSystemWideCriticalSection::~FWindowsSystemWideCriticalSection()
{
	Release();
}


bool FWindowsSystemWideCriticalSection::IsValid() const
{
	return Mutex != NULL;
}

void FWindowsSystemWideCriticalSection::Release()
{
	if (IsValid())
	{
		/*放弃所有权*/
		ReleaseMutex(Mutex);
		CloseHandle(Mutex);
		Mutex = NULL;
	}
}
#endif
