#pragma once
#include "Core.h"
class FTaskThreadBase;
class FRunnableThread;
/**
 * 用来封装每个工作线程（Worker Thread）状态的轻量结构体
 * 这是一个小型辅助结构体，用来集中管理单个线程所需的几个关键成员变量。
 */
struct COREMODULE FWorkerThread
{
	// 实际用来执行任务的线程逻辑对象
	FTaskThreadBase* TaskGraphWorker{nullptr};

	// 表示这个线程是否是通过 FRunnableThread::Create(...) 创建的内部线程。
	//仅内部线程使用；如果是外部线程（如 GameThread、RenderThread），这个指针为 nullptr。
	FRunnableThread* RunnableThread{nullptr};

	// 仅用于外部线程（如GameThread）。线程初始化之后需要Attach 到 Task Graph 系统,用这个布尔标志判断是否已完成Atach。
	bool bAttached{false};
};
