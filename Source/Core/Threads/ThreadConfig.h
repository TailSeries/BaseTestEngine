#pragma once
#include "Core.h"
/*
 * 线程优先级，越大越高
 */
enum COREMODULE EThreadPriority
{
	TPri_TimeCritical = 0x10,
	TPri_Highest = 0x20,
	TPri_AboveNormal = 0x30,
	TPri_Normal = 0x40,
	TPri_SlightlyBelowNormal = 0x50,
	TPri_BelowNormal = 0x60,
	TPri_Lowest = 0x70,
	TPri_Num,
};


enum class COREMODULE EThreadCreateFlags : int8
{
	None = 0,// 默认线程创建行为
	SMTExclusive = (1 << 0), // 	指定线程不与其他线程共享同一物理核心的超线程逻辑核
};

//无所谓哪个cpu核心
COREMODULE const uint64 GetNoAffinityMask();
