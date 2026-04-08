#include <Windows.h>
#include <iostream>
#include <map>
#include <timeapi.h>
#include <vector>
#include "Common/LogAssert.h"
#include "DirectX12/Chapter/InitDirect3D.h"
#include "DirectX12/Common/MathHelper.h"

HWND CreateMainWindow(HINSTANCE hInstance);
HWND CreateChildWindow(HWND parentWnd, HINSTANCE processHinstance);

LRESULT WINAPI WndProc(HWND hWnd, UINT msgID, WPARAM sParam, LPARAM lParam)
{
	switch (msgID)
	{
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		break;

	}
	}; return DefWindowProc(hWnd, msgID, sParam, lParam);
}


#pragma region Test001
#include "Threads/Runnable.h"
#include "Threads/RunnableThread.h"

class TestThread :public FRunnable, public FSingleThreadRunnable
{
public:
	int a = 0;
	TestThread(int b) :a(b)
	{
		LogStringMsg("TestThread::TestThread a = %d", a);
	};
	virtual bool Init() override
	{
		a++;
		LogStringMsg("TestThread::Init a = %d", a);
		return true;
	}
	virtual uint32 Run()override
	{
		a++;
		LogStringMsg("TestThread::Run a = %d", a);
		return 0;

	};
	virtual void Stop()override
	{
		a++;
		LogStringMsg("TestThread::Stop a = %d", a);
	}
	virtual void Exit()override
	{
		a++;
		LogStringMsg("TestThread::Exit a = %d", a);
	}

	virtual void Tick()override
	{
		a++;
		LogStringMsg("TestThread::Tick a = %d", a);
	}

	//多线程框架不可用的时候该怎么办
	virtual class FSingleThreadRunnable* GetSingleThreadInterface()
	{
		return this;
	}
	virtual ~TestThread() {};

};

static void Test001()
{
	uint32 currentThreadID = FPlatformTLS::GetCurrentThreadId();
	TestThread* test = new TestThread(0);
	FRunnableThread* testThreadHandle = nullptr;
	testThreadHandle = FRunnableThread::Create(test, "TestThread");
}
#pragma endregion Test001

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{

	int MsgResult = 0;

#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	AllocConsole();
	AttachConsole(GetCurrentProcessId());
	freopen("CON", "w", stdout);
	{
		WarningStringMsg("EngineTest main Start");
		InitDirect3DApp D3DApp(hInstance);
		if (!D3DApp.Initialize())
		{
			ErrorStringMsg("InitDirect3DApp Failed!");
			return 0;
		}
		MsgResult = D3DApp.Run();
	}

	FreeConsole();
	return MsgResult;
}
HWND CreateMainWindow(HINSTANCE hInstance)
{
	WNDCLASS wc = { 0 };
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
	wc.hCursor = NULL;
	wc.hIcon = NULL;
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = "WinMain";
	wc.lpszMenuName = NULL;
	wc.style = CS_HREDRAW | CS_VREDRAW;

	RegisterClass(&wc);
	HWND hWnd = CreateWindow("WinMain", "WinMainTitle", WS_OVERLAPPEDWINDOW, 100, 100, 1440, 720, NULL, NULL, hInstance, NULL);
	//CreateChildWindow(hWnd, hInstance);
	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	MSG nMsg = { 0 };
	while (GetMessage(&nMsg, NULL, 0, 0))
	{
		TranslateMessage(&nMsg);
		DispatchMessage(&nMsg);
	}
	return hWnd;
};
HWND CreateChildWindow(HWND parentWnd, HINSTANCE processHinstance)
{
	WNDCLASS wc = { 0 };
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 3);
	wc.hCursor = NULL;
	wc.hIcon = NULL;
	wc.hInstance = processHinstance;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = "Child";
	wc.lpszMenuName = NULL;
	wc.style = CS_HREDRAW | CS_VREDRAW;

	RegisterClass(&wc);
	return CreateWindowEx(0, "Child", "cl", WS_CHILD | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		0, 0, 200, 200, parentWnd, NULL, processHinstance, NULL);
};

