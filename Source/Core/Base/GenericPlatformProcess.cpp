#include "GenericPlatformProcess.h"

#include "Threads/EventPool.h"
#include "Threads/Win/WindowsRunnableThread.h"
#include <bit>


namespace WindowsPlatformProcessImpl
{
	static void SetThreadName(LPCSTR ThreadName)
	{
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
		/**
		 * Code setting the thread name for use in the debugger.
		 *
		 * http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
		 */
		const uint32 MS_VC_EXCEPTION = 0x406D1388;

		struct THREADNAME_INFO
		{
			uint32 dwType;		// Must be 0x1000.
			LPCSTR szName;		// Pointer to name (in user addr space).
			uint32 dwThreadID;	// Thread ID (-1=caller thread).
			uint32 dwFlags;		// Reserved for future use, must be zero.
		};

		THREADNAME_INFO ThreadNameInfo;
		ThreadNameInfo.dwType = 0x1000;
		ThreadNameInfo.szName = ThreadName;
		ThreadNameInfo.dwThreadID = ::GetCurrentThreadId();
		ThreadNameInfo.dwFlags = 0;

		__try
		{
			RaiseException(MS_VC_EXCEPTION, 0, sizeof(ThreadNameInfo) / sizeof(ULONG_PTR), (ULONG_PTR*)&ThreadNameInfo);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
#endif
	}

	static void SetThreadDescription(PCSTR lpThreadDescription)
	{


		typedef HRESULT(WINAPI* SetThreadDescriptionFnPtr)(HANDLE hThread, PCWSTR lpThreadDescription);

#pragma warning( push )
#pragma warning( disable: 4191 )	// unsafe conversion from 'type of expression' to 'type required'
		static SetThreadDescriptionFnPtr RealSetThreadDescription = (SetThreadDescriptionFnPtr)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "SetThreadDescription");
#pragma warning( pop )

		/*
		 * windows 上设置线程名必须宽字符串
		 */
		int len = MultiByteToWideChar(CP_UTF8, 0, lpThreadDescription, -1, nullptr, 0);
		if (len > 0 && RealSetThreadDescription) {
			std::wstring wName(len, L'\0');
			MultiByteToWideChar(CP_UTF8, 0, lpThreadDescription, -1, &wName[0], len);
			RealSetThreadDescription(GetCurrentThread(), wName.c_str());
		}
	}
}


bool FGenericPlatformProcess::SupportsMultithreading()
{
	//todo 我们默认就是支持多线程就行,后续改成ini或者启动参数控制
	return true;
}

void FGenericPlatformProcess::ReturnSynchEventToPool(FEvent* Event)
{
	if (!Event)
	{
		return;
	}

	if (Event->IsManualReset())
	{
		FEventPool<EEventPoolTypes::ManualReset>::Get().ReturnToPool(Event);
	}
	else
	{
		FEventPool<EEventPoolTypes::AutoReset>::Get().ReturnToPool(Event);
	}
}

const uint64 FGenericPlatformProcess::GetTaskGraphThreadMask()
{
	return 0xFFFFFFFFFFFFFFFF;
}

const uint64 FGenericPlatformProcess::GetTaskGraphHighPriorityTaskMask()
{

	return 0xFFFFFFFFFFFFFFFF;

}

const uint64 FGenericPlatformProcess::GetTaskGraphBackgroundTaskMask()
{
	return 0xFFFFFFFFFFFFFFFF;
}
void FWindowsPlatformProcess::SetThreadAffinityMask(uint64 AffinityMask)
{
	if (AffinityMask != GetNoAffinityMask())
	{
		::SetThreadAffinityMask(::GetCurrentThread(), (DWORD_PTR)AffinityMask);
	}
}

void FWindowsPlatformProcess::SetThreadName(const char* ThreadName)
{
	/*
	 * 我们使用vs2022 没必要使用这种老式的方式来设置线程名
	 */
	 //WindowsPlatformProcessImpl::SetThreadDescription(ThreadName);

	WindowsPlatformProcessImpl::SetThreadName(ThreadName);
}

void FWindowsPlatformProcess::Sleep(float Seconds)
{

	uint32 Milliseconds = (uint32)(Seconds * 1000.0);
	if (Milliseconds == 0)
	{
		::SwitchToThread();
	}
	else
	{
		::Sleep(Milliseconds);
	}
}

int32 FWindowsPlatformProcess::NumberOfWorkerThreadsToSpawn()
{
	//服务器模式最多启用 4 个工作线程（通常服务器可分配的核心少，负载特点不同）
	static int32 MaxServerWorkerThreads = 4;
	static int32 MaxWorkerThreads = 26; // 普通客户端最多启用 26 个工作线程（一般是根据最大锁链长度或合理线程池大小定的）。
	int32 NumberOfCores = FWindowsPlatformProcess::NumberOfCores();
	int32 NumberOfCoresIncludingHyperthreads = FWindowsPlatformProcess::NumberOfCoresIncludingHyperthreads();
	int32 NumberOfThreads = 0;
	/*
	 * 如果存在超线程（逻辑核心数多于物理核心数），则用逻辑核心数-2 来确定线程数。
	 * 否则用物理核心数减 1。
	 * 减去 1 或 2 是为了留出一个或两个核心给主线程或系统，避免饱和。
	 */
	if (NumberOfCoresIncludingHyperthreads > NumberOfCores)
	{
		NumberOfThreads = NumberOfCoresIncludingHyperthreads - 2;
	}
	else
	{
		NumberOfThreads = NumberOfCores - 1;
	}

	int32 MaxWorkerThreadsWanted = MaxWorkerThreads;
	return std::max(std::min(NumberOfThreads, MaxWorkerThreadsWanted), 2);
}



static void QueryCpuInformation(uint32& OutGroupCount, uint32& OutNumaNodeCount, uint32& OutCoreCount, uint32& OutLogicalProcessorCount, bool bForceSingleNumaNode = false)
{
	GROUP_AFFINITY FilterGroupAffinity = {};

	if (bForceSingleNumaNode)
	{
		PROCESSOR_NUMBER ProcessorNumber = {};
		USHORT NodeNumber = 0;

		GetThreadIdealProcessorEx(GetCurrentThread(), &ProcessorNumber);
		GetNumaProcessorNodeEx(&ProcessorNumber, &NodeNumber);
		GetNumaNodeProcessorMaskEx(NodeNumber, &FilterGroupAffinity);
	}

	OutGroupCount = OutNumaNodeCount = OutCoreCount = OutLogicalProcessorCount = 0;
	uint8* BufferPtr = nullptr;
	DWORD BufferBytes = 0;

	if (false == GetLogicalProcessorInformationEx(RelationAll, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)BufferPtr, &BufferBytes))
	{
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			BufferPtr = reinterpret_cast<uint8*>(std::malloc(BufferBytes));

			if (GetLogicalProcessorInformationEx(RelationAll, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)BufferPtr, &BufferBytes))
			{
				uint8* InfoPtr = BufferPtr;

				while (InfoPtr < BufferPtr + BufferBytes)
				{
					PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX ProcessorInfo = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)InfoPtr;

					if (nullptr == ProcessorInfo)
					{
						break;
					}

					if (ProcessorInfo->Relationship == RelationProcessorCore)
					{
						if (bForceSingleNumaNode)
						{
							for (int GroupIdx = 0; GroupIdx < ProcessorInfo->Processor.GroupCount; ++GroupIdx)
							{
								if (FilterGroupAffinity.Group == ProcessorInfo->Processor.GroupMask[GroupIdx].Group)
								{
									KAFFINITY Intersection = FilterGroupAffinity.Mask & ProcessorInfo->Processor.GroupMask[GroupIdx].Mask;

									if (Intersection > 0)
									{
										OutCoreCount++;

										OutLogicalProcessorCount += std::popcount(Intersection);
									}
								}
							}
						}
						else
						{
							OutCoreCount++;

							for (int GroupIdx = 0; GroupIdx < ProcessorInfo->Processor.GroupCount; ++GroupIdx)
							{
								OutLogicalProcessorCount += std::popcount(ProcessorInfo->Processor.GroupMask[GroupIdx].Mask);
							}
						}
					}
					if (ProcessorInfo->Relationship == RelationNumaNode)
					{
						OutNumaNodeCount++;
					}

					if (ProcessorInfo->Relationship == RelationGroup)
					{
						OutGroupCount = ProcessorInfo->Group.ActiveGroupCount;
					}

					InfoPtr += ProcessorInfo->Size;
				}
			}

			std::free(BufferPtr);
		}
	}
}
int32 FWindowsPlatformProcess::NumberOfCoresIncludingHyperthreads()
{
	static int32 CoreCount = 0;
	if (CoreCount == 0)
	{
		uint32 NumGroups = 0;
		uint32 NumaNodeCount = 0;
		uint32 NumCores = 0;
		uint32 LogicalProcessorCount = 0;
		QueryCpuInformation(NumGroups, NumaNodeCount, NumCores, LogicalProcessorCount);

		CoreCount = LogicalProcessorCount;

		// Optionally limit number of threads (we don't necessarily scale super well with very high core counts)

		int32 LimitCount = 32768;
		//if (FCommandLine::IsInitialized() && FParse::Value(FCommandLine::Get(), TEXT("-corelimit="), LimitCount))
		{
			CoreCount = std::min(CoreCount, LimitCount);
		}
	}

	return CoreCount;
}

int32 FWindowsPlatformProcess::NumberOfCores()
{
	static int32 CoreCount = 0;
	if (CoreCount == 0)
	{
		uint32 NumGroups = 0; // CPU 组的数量（比如 Windows 下超多核的 CPU 会被分为多个组）。
		uint32 NumaNodeCount = 0; // NUMA 节点数量（用于内存访问优化）。现代，一组 CPU 拥有自己本地连接的内存（RAM）称为一个NUMA节点。比如：【节点0 CPU 0~3 16 GB】【节点1 CPU 4~7 16 GB】  
		uint32 NumCores = 0; // 物理核心数。
		uint32 LogicalProcessorCount = 0; // 逻辑处理器数，即包括超线程（Hyper-Threading）后的总线程数。
		QueryCpuInformation(NumGroups, NumaNodeCount, NumCores, LogicalProcessorCount);
		////if (FCommandLine::IsInitialized() && FParse::Param(FCommandLine::Get(), TEXT("usehyperthreading")))
		//{
		//	CoreCount = LogicalProcessorCount;
		//}
		//else
		{
			// 我们先默认不使用超线程，感觉是个坑
			CoreCount = NumCores;
		}
		int32 LimitCount = 32768;
		/*	if (FCommandLine::IsInitialized() && FParse::Value(FCommandLine::Get(), TEXT("-corelimit="), LimitCount))*/
		{
			CoreCount = std::min(CoreCount, LimitCount);
		}
	}

	return CoreCount;
}


class FEvent* FWindowsPlatformProcess::GetSynchEventFromPool(bool bIsManualReset)
{
	return bIsManualReset ? FEventPool<EEventPoolTypes::ManualReset>::Get().GetEventFromPool() : FEventPool<EEventPoolTypes::AutoReset>::Get().GetEventFromPool();
}

class FEvent* FWindowsPlatformProcess::CreateSynchEvent(bool bIsManualReset)
{
	const bool bIsMultithread = FPlatformProcess::SupportsMultithreading();
	FEvent* Event = nullptr;
	if (bIsMultithread)
	{
		Event = new FEventWin();
	}
	else
	{
		Event = new FSingleThreadEvent();
	}

	if (!Event->Create(bIsManualReset))
	{
		delete Event;
		Event = nullptr;
	}
	return Event;
}

class FRunnableThread* FWindowsPlatformProcess::CreateRunnableThread()
{
	return new FRunnableThreadWin();
}
