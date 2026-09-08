#include <Windows.h>
#include "D3D12Adapter.h"    // 链了 D3D12RHI，其 Public 目录已在 include 路径
#include "D3D12Viewport.h"

static bool g_Running = true;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY)
    {
        g_Running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // ---- 1. 建 Win32 窗口（应用层的活，不归 RHI）----
    const char* ClassName = "RHITestWindow";
    WNDCLASS wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = ClassName;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    const uint32 Width = 1280, Height = 720;
    HWND hwnd = CreateWindowEx(0, ClassName, "BaseTestEngine RHI - Ch1",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, Width, Height,
        nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hwnd, nCmdShow);

    // ---- 2. RHI 初始化：三层 + Viewport ----
    FD3D12AdapterDesc Desc;
    if (!FD3D12Adapter::FindAdapter(Desc))
    {
        MessageBox(hwnd, "No D3D12 adapter found", "Error", MB_OK);
        return -1;
    }

    FD3D12Adapter Adapter(Desc);
    Adapter.InitializeDevices();     // RootDevice -> Device -> 三条 Queue

    FD3D12Viewport Viewport(&Adapter, hwnd, Width, Height, DXGI_FORMAT_R8G8B8A8_UNORM, 2);
    Viewport.Init();                 // swap chain + back buffers

    // ---- 3. 消息循环 + Present（黑屏、不崩 = 成功）----
    MSG msg = {};
    while (g_Running)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Viewport.PresentInternal(1); // vsync
    }

    // ---- 4. 退出前等 GPU 空闲，再让局部对象析构（释放 swap chain）----
    FD3D12Queue& Q = Adapter.GetDevice()->GetQueue(ED3D12QueueType::Direct);
    Q.WaitCPU(Q.Signal(Q.Fence));    // 我们之前那对 Signal + WaitCPU

    return 0;
}
