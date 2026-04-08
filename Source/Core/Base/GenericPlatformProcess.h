#pragma once
#include "Core.h"
#if PLATFORM_CPU_X86_FAMILY
/*
 * 一个头文件，属于 Intel 提供的 SIMD（单指令多数据）指令集支持库之一，专门用于访问 SSE2 指令集（Streaming SIMD Extensions 2）。这个头文件中定义了大量的与 SSE2 向量操作相关的 intrinsic 函数，通常用于实现数据级并行加速。
 * 提供了基于 __m128d、__m128i 类型的指令封装，主要用于操作 双精度浮点数 或 128 位整数向量
 */
#include <emmintrin.h>
#endif

#if !defined(__clang__)
	/*
	 * <intrin.h> 是微软编译器（MSVC）提供的一个头文件，包含了所有平台相关的内建（intrinsic）函数声明，常用于访问底层 CPU 指令（如汇编替代）、原子操作、SIMD、位操作、内存屏障等。
	 */
#	include <intrin.h>
#	if defined(_M_ARM)
#		include <armintr.h>
#	elif defined(_M_ARM64)
#		include <arm64intr.h>
#	endif
#endif


class FEvent;

struct COREMODULE FGenericPlatformProcess
{
	//是否支持多线程
	static bool SupportsMultithreading();
	//将event放回池子
	static void ReturnSynchEventToPool(FEvent* Event);
	static const uint64 GetTaskGraphThreadMask();
	static const uint64 GetTaskGraphHighPriorityTaskMask();
	static const uint64 GetTaskGraphBackgroundTaskMask();


};



//我们当前只有windows，直接用
struct COREMODULE FWindowsPlatformProcess : public FGenericPlatformProcess
{
	//创建windows线程
	static class FRunnableThread* CreateRunnableThread();
	//创建同步事件
	static class FEvent* CreateSynchEvent(bool bIsManualReset = false);
	static class FEvent* GetSynchEventFromPool(bool bIsManualReset = false);
	// 设置线程绑定的cpu核心掩码
	static void SetThreadAffinityMask(uint64 AffinityMask);
	// 设置线程名字
	static void SetThreadName(const char* ThreadName);
	// 设置内存屏障
	static void SetMemoryBarrier() { _mm_sfence(); }

	static void Sleep(float Seconds);
	// 创建多少工作线程
	static int32 NumberOfWorkerThreadsToSpawn();
	// 包含超线程在内的逻辑核心数量
	static int32 NumberOfCoresIncludingHyperthreads();
	// 枚举核心数量
	static int32 NumberOfCores();

	// TaskGraph 系统的的亲和掩码


};


// 
#if PLATFORM_WIN
typedef FWindowsPlatformProcess FPlatformProcess;
#endif
