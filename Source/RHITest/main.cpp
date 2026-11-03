#include <Windows.h>
#include <fstream>
#include <string>
#include "RHITestPeriod1.h"
#include "TestPlatform.h"
#include "Logging/Log.h"

static bool g_Running = true;
static bool g_SwapTexture = false;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_DESTROY)
	{
		g_Running = false;
		PostQuitMessage(0);
		return 0;
	}
	if (msg == WM_KEYDOWN && wParam == VK_SPACE)   // ← 加：空格触发换纹理
	{
		g_SwapTexture = true;
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR CommandLine, int nCmdShow)
{
    const bool bValidate = std::string(CommandLine).find("--validate") != std::string::npos;
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	WNDCLASS wc = {};
	wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
	wc.lpszClassName = "RHITestWindow"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClass(&wc);
	int32 Width = bValidate ? 640 : 1920;
	int32 Height = bValidate ? 480 : 1080;
	RECT rc = { 0, 0, (LONG)Width, (LONG)Height };     // 想要的客户区
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE); // 加上标题栏+边框，算出整窗尺寸

	HWND hwnd = CreateWindowEx(0, "RHITestWindow", "BaseTestEngine RHI - Stage B Two Passes",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left,   // ← 用反算后的整窗宽
		rc.bottom - rc.top,   // ← 整窗高
		nullptr, nullptr, hInst, nullptr);
    ShowWindow(hwnd, bValidate ? SW_HIDE : nCmdShow);
    try
    {
        CoreLog::FLogOptions LogOptions;
        LogOptions.FilePath = "Logs/RHITest.log";
        LogOptions.Console = false;
        CoreLog::Initialize(LogOptions);
        RHIInit(CreateTestRHI(bValidate));
        CORE_LOG(LogTemp, Log, "Stage B: single Direct Queue, whole-resource transitions, offscreen + composite");
        {
            RHITestPeriod1 App;
            App.Initialize(hwnd, bValidate);
            if (bValidate)
            {
                App.Validate();
            }
            else
            {
                MSG Msg{};
                while (g_Running)
                {
                    while (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
                    {
                        TranslateMessage(&Msg);
                        DispatchMessage(&Msg);
                    }
                    if (g_Running)
                    {
                        if (g_SwapTexture) { App.RequestTextureSwap(); g_SwapTexture = false; }
                        App.DrawTriangle();
                    }
                }
            }
            App.WaitForGPU();
        } // 所有上层资源先释放，再关闭 RHI。
        CheckDebugMessages();
        RHIExit();
        if (bValidate) std::ofstream("stage-b-validation.txt") << "PASS: Stage B transitions, two-pass rendering, Stage A regression, 120 swaps, image readback, clean debug queue, shutdown\n";
        if (IsWindow(hwnd)) DestroyWindow(hwnd);
        return 0;
    }
    catch (const std::exception& Error)
    {
        CORE_LOG(LogTemp, Error, "Stage B failed: {}", Error.what());
        std::ofstream("stage-b-validation.txt") << "FAIL: " << Error.what() << '\n';
        if (!bValidate) MessageBoxA(hwnd, Error.what(), "RHI error", MB_OK | MB_ICONERROR);
        return 1;
    }
}
