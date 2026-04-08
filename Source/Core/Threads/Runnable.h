#pragma once
#include "Core.h"
/*
而FRunnable是一个接口，用于定义可运行的线程类。它定义了线程的执行逻辑和任务，并提供了线程同步和生命周期管理的功能。
开发者可以通过继承FRunnable接口来实现自定义的可运行线程类，并在FRunnableThread中创建和管理该线程。
*/
class COREMODULE FRunnable
{
public:
	virtual bool Init()
	{
		return true;
	}
	virtual uint32 Run() = 0;
	virtual void Stop() {}
	virtual void Exit() {}

	//多线程框架不可用的时候该怎么办
	virtual class FSingleThreadRunnable* GetSingleThreadInterface()
	{
		return nullptr;
	}
	virtual ~FRunnable(){};
};