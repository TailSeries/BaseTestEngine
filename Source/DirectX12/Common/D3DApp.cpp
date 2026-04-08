#include "D3DApp.h"

#include <vector>
#include <windowsx.h>
#if defined(DEBUG) || defined(_DEBUG)
#include <d3d12sdklayers.h>
#endif


using Microsoft::WRL::ComPtr;
using namespace std;


//windows 通过这个函数分发消息
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	//todo
	return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

D3DApp* D3DApp::MApp = nullptr;

D3DApp::D3DApp(HINSTANCE hInstance)
	:MhAppInst(hInstance)
{
	MApp = this;
}

D3DApp::~D3DApp()
{
	if (MD3dDevice != nullptr)
		FlushCommandQueue();
}

D3DApp* D3DApp::GetApp()
{
	return MApp;
}

bool D3DApp::Initialize()
{
	if (!InitMainWindow())
		return false;

	if (!InitDirect3D())
		return false;
	// Do the initial resize code.这里会引用上swapchainbuffer
	OnResize();
	return true;
}

LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		// WM_ACTIVATE is sent when the window is activated or deactivated.  
		// We pause the game when the window is deactivated and unpause it 
		// when it becomes active.  
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;

		// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
		// Save the new client area dimensions.
		MClientWidth = LOWORD(lParam);
		MClientHeight = HIWORD(lParam);
		if (MD3dDevice)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{

				// Restoring from minimized state?
				if (mMinimized)
				{
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}

				// Restoring from maximized state?
				else if (mMaximized)
				{
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if (mResizing)
				{
					// If user is dragging the resize bars, we do not resize 
					// the buffers here because as the user continuously 
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from dragging
					// the resize bars.  So instead, we reset after the user is 
					// done resizing the window and releases the resize bars, which 
					// sends a WM_EXITSIZEMOVE message.
				}
				else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				{
					OnResize();
				}
			}
		}
		return 0;

		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		OnResize();
		return 0;

		// WM_DESTROY is sent when the window is being destroyed.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

		// The WM_MENUCHAR message is sent when a menu is active and the user presses 
		// a key that does not correspond to any mnemonic or accelerator key. 
	case WM_MENUCHAR:
		// Don't beep when we alt-enter.
		return MAKELRESULT(0, MNC_CLOSE);

		// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		else if ((int)wParam == VK_F2)
			Set4xMsaaState(!M4xMsaaState);

		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

int D3DApp::Run()
{
	MSG msg = { 0 };

	mTimer.Reset();

	while (msg.message != WM_QUIT)
	{
		// If there are Window messages then process them.
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, do animation/game stuff.
		else
		{
			mTimer.Tick();

			if (!mAppPaused)
			{
				CalculateFrameStats();
				Update(mTimer);
				Draw(mTimer);
#if defined(DEBUG) || defined(_DEBUG)
				LogD3D12Messages();
#endif
			}
			else
			{
				Sleep(100);
			}
		}
	}

	return (int)msg.wParam;
}

bool D3DApp::InitMainWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = MhAppInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = "MainWnd";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, "RegisterClass Failed.", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, MClientWidth, MClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	MhMainWnd = CreateWindow("MainWnd", MMainWndCaption.c_str(),
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, MhAppInst, 0);
	if (!MhMainWnd)
	{
		MessageBox(0, "CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(MhMainWnd, SW_SHOW);
	UpdateWindow(MhMainWnd);

	return true;
}

bool D3DApp::InitDirect3D()
{
	InitDirectX12DebugLayer();
	//1.创建d3d设备
	/*
	 * 创建DXGIFactory >> DXGI 创建device >>device创建fence 》》 device查询描述符大小 》》device检查msaa这个feature level的支持情况
	 */
	CreateDXGIFactory();
	CreateDevice();
	CreateFence();
	QueryDescriptorSize();
	CreateDescriptorHeap();
	CheckMSAASupportLevel();
	//2.创建命令队列，命令分配器，命令列表
	CreateCommandObjects();
	//3.创建交换链
	CreateSwapChain();



	return true;
}

void D3DApp::OnResize()
{
	//强刷一遍命令队列
	FlushCommandQueue();

	// 指向命令列表
	ThrowIfFailed(MCommandAllocator->Reset());
	ThrowIfFailed(MCommandList->Reset(MCommandAllocator.Get(), nullptr));

	//调整swapchain
	ResizeAndReleaseSwapChains();

	//调整depth
	ResizeDepthStencil();

	//执行命令
	ThrowIfFailed(MCommandList->Close());
	ID3D12CommandList* cmdLists[] = {MCommandList.Get()};
	MCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	//强刷，等待resize命令执行完
	FlushCommandQueue();
	MScreenViewPort.TopLeftX = 0;
	MScreenViewPort.TopLeftY = 0;
	MScreenViewPort.Width = static_cast<float>(MClientWidth);
	MScreenViewPort.Height = static_cast<float>(MClientHeight);
	MScreenViewPort.MinDepth = 0.0f;
	MScreenViewPort.MaxDepth = 1.0f;
	MScissorRect = {0, 0, MClientWidth, MClientHeight};


}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView()const
{
	// 从dsv的heap中返回第一个可用的DSV
	return MDSVHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3DApp::Set4xMsaaState(bool value)
{
	if (M4xMsaaState != value)
	{
		M4xMsaaState = value;
		// 交换链的内容和msaa密切相关，我们改了msaa的时候要重新创建交换链
		CreateSwapChain();
		OnResize();
	}
}

void D3DApp::FlushCommandQueue()
{
	CurrentFence++;

	//设置GPU 要执行到的fence
	auto value0 = MD3dFence->GetCompletedValue();
	ThrowIfFailed(MCommandQueue->Signal(MD3dFence.Get(), CurrentFence));
	auto value = MD3dFence->GetCompletedValue();
	if (MD3dFence->GetCompletedValue() < CurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		//当gpu达到这个fence的时候触发这个eventhandle
		ThrowIfFailed(MD3dFence->SetEventOnCompletion(CurrentFence, eventHandle));
		//我们等待GPU完整这个时间
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void D3DApp::InitDirectX12DebugLayer()
{
#if defined(DEBUG) || defined(_DEBUG)
	ComPtr<ID3D12Debug> debugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
	debugController->EnableDebugLayer();
#endif
}



void D3DApp::CreateDXGIFactory()
{
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&MDxgiFactory)))
}

void D3DApp::CreateDevice()
{
	//nullptr 默认选择第一块显卡
	/*
	 * 不能直接获取 ID3D12Device10 应该要获取基础版本然后再As，
	 * ComPtr<ID3D12Device> base;
D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&base));

ComPtr<ID3D12Device10> d10;
ComPtr<ID3D12Device9> d9;
...

if (SUCCEEDED(base.As(&d10))) OutputDebugString(L"支持 ID3D12Device10\n");
else if (SUCCEEDED(base.As(&d9))) OutputDebugString(L"支持 ID3D12Device9\n");
	 */

	HRESULT hardwareResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&MD3dDevice));
	if (FAILED(hardwareResult))
	{
		// 如果硬件显卡找不到，我们这里尝试枚举所有显卡，然后拿第一个来用，可能是软件的
		ComPtr<IDXGIAdapter4> WrapAdapter;
		ThrowIfFailed(MDxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&WrapAdapter)));
		ThrowIfFailed(D3D12CreateDevice(WrapAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&MD3dDevice)))
	}
#if defined(DEBUG) || defined(_DEBUG)
	SetDxDebugInfoDetailsSetting();
#endif

}

void D3DApp::CreateFence()
{
	//创建fence
	//第一个参数是fence的初始值，一般是0.第二个参数用来控制Fence是不是跨进程，跨显卡使用，一般用 D3D12_FENCE_FLAG_NONE 就行
	ThrowIfFailed(MD3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&MD3dFence)));
}

void D3DApp::QueryDescriptorSize()
{
	//查询几种类型的描述符的大小
	// GetDescriptorHandleIncrementSize 用来查询指定类型的描述符堆中相邻两个元素之间的偏移量，就是该类型描述符的大小
	MRTVDescriptorSize = MD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	MDSVDescriptorSize = MD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	/*
	 * CBV/SRV/UAV 都属于“着色器资源视图”的范畴，它们的使用方式非常相似。 GPU 驱动厂商通常会在底层对这几种资源使用相同格式的硬件描述符结构。
	 * 这三种描述符共享一个堆类型D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV。
	 */
	MCBVSRVUAVDescriptorSize = MD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void D3DApp::CheckMSAASupportLevel()
{
#if defined(DEBUG) || defined(_DEBUG)
	LogAdapters();
#endif

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS MsQualityLevels;
	MsQualityLevels.Format = MBackBufferFormat;
	MsQualityLevels.SampleCount = 4;
	MsQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE; // 一般是 D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE；还有种可能是针对Tiled资源做查询 D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_TILED_RESOURCE

	//NumQualityLevels是输出：在指定的格式（Format）和采样数（SampleCount）下，GPU 驱动支持的 MSAA“采样质量等级”数量。
	/*
	 * 比如 Format = DXGI_FORMAT_R8G8B8A8_UNORM SampleCount = 4 时 NumQualityLevels 值为0.意思是在此种格式和samplecount下，显卡支持3种不同的采样模式（质量等级）
	 * 0：	基础采样模式（质量一般）；1：更优化的采样模式（采样点分布更合理）；2：最佳质量（可能是中心对称或抖动分布）。这些是硬件厂商定义的
	 */
	MsQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(MD3dDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &MsQualityLevels, sizeof(MsQualityLevels)));
	M4xMSAAQuality = MsQualityLevels.NumQualityLevels;
	WarningStringMsg("[D3DApp::CheckMSAASupportLevel] M4xMSAAQuality = %d", M4xMSAAQuality);
}

void D3DApp::CreateCommandObjects()
{
	//创建命令队列，命令分配器，命令列表
	D3D12_COMMAND_QUEUE_DESC QueueDesc;
	QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;//命令列表的类型Direct:直接提交的万能类型，一般都是这个。 Bundle 命令列表不能被直接提交而是由direct类型的列表调用，Compute专门给计算着色器用，copy资源复制等命令
	QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;//一般都是None，timeout选项一般是用来给离线工具的。可以禁用默认情况下的WindowsTDR 机制（如果 GPU 某个命令执行时间过长（通常约 2 秒），系统会认为 GPU 挂了，自动重置驱动）。
	QueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	QueueDesc.NodeMask = 0;
	ThrowIfFailed(MD3dDevice->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(MCommandQueue.GetAddressOf())));

	ThrowIfFailed(MD3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&MCommandAllocator)));

	//CreateCommandList1版本无需在此时绑定分配器, 我们在reset的时候重新绑定allocator
	ThrowIfFailed(MD3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, MCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&MCommandList)));

	//初始状态是 idle（非 recording） 但 Direct3D 12 的规范要求：只有在 command list 处于 "closed" 状态时才能调用 Reset()
	ThrowIfFailed(MCommandList->Close())
}

void D3DApp::CreateSwapChain()
{
	MSwapChain.Reset();
	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferDesc.Width = MClientWidth;
	sd.BufferDesc.Height = MClientHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = MBackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;//定义扫描方式，逐行或者隔行之类的，我们这里直接交给系统
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;//屏幕显示是居中还是拉伸，我们交给系统来决定
	sd.SampleDesc.Count =  M4xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = M4xMsaaState ? (M4xMSAAQuality - 1) : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = SwapChainBufferCount;//交换链上缓冲区的数量
	sd.OutputWindow = MhMainWnd;//输出的主窗口
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;//页面翻转的类型，一般都是用这个，翻转后后台缓冲区内容被丢弃 //flipmodel 不支持msaa，书上这里有问题.直接的discard模式又会报警告 因此 Count 必须为1 Quality 必须为0
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(MDxgiFactory->CreateSwapChain( MCommandQueue.Get(), &sd, MSwapChain.GetAddressOf()));
}

void D3DApp::CreateDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDesc;
	RTVHeapDesc.NodeMask = 0;
	RTVHeapDesc.NumDescriptors = SwapChainBufferCount;//这里我们有两个缓冲区资源，因此这里需要两个描述符
	RTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	RTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;//控制这个堆是否对GPU可见，比如需要描述符堆绑定到根签名的描述符表的时候，必须设置为SHADER_VISIBLE
	ThrowIfFailed(MD3dDevice->CreateDescriptorHeap(&RTVHeapDesc, IID_PPV_ARGS(&MRTVHeap)));

	D3D12_DESCRIPTOR_HEAP_DESC DSVHeapDesc;
	DSVHeapDesc.NodeMask = 0;
	DSVHeapDesc.NumDescriptors = 1;//我们只需要一个深度模版缓冲区
	DSVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	ThrowIfFailed(MD3dDevice->CreateDescriptorHeap(&DSVHeapDesc, IID_PPV_ARGS(&MDSVHeap)));

}

void D3DApp::ResizeAndReleaseSwapChains()
{
	// 释放掉我们每次都要重新创建的一些资源
	for (int i = 0; i < SwapChainBufferCount; ++i)
	{
		MSwapChainBuffer[i].Reset();
	}
	//swap chain 我们要resize
	ThrowIfFailed(MSwapChain->ResizeBuffers(SwapChainBufferCount, MClientWidth, MClientHeight, MBackBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));
	//获取描述符堆中第一个 CPU 可访问描述符句柄 
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(MRTVHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; ++i)
	{
		// 创建swapchain的时候对应的buffer已经创建好，不用我们创建。
		ThrowIfFailed(MSwapChain->GetBuffer(i, IID_PPV_ARGS(&MSwapChainBuffer[i])));
		//第三个参数nullptr，表示使用默认的描述符信息
		MD3dDevice->CreateRenderTargetView(MSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, MRTVDescriptorSize);
	}
	mCurrBackBuffer = 0;
}

void D3DApp::ResizeDepthStencil()
{
	//创建dsv和对应的buffer资源
	MDepthStencilBuffer.Reset();
	D3D12_RESOURCE_DESC DepthStencileDesc;
	DepthStencileDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//资源维度，通常是buffer，
	DepthStencileDesc.Alignment = 0;
	DepthStencileDesc.Width = MClientWidth;
	DepthStencileDesc.Height = MClientHeight;
	DepthStencileDesc.DepthOrArraySize = 1;
	DepthStencileDesc.MipLevels = 1;
	DepthStencileDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	DepthStencileDesc.SampleDesc.Count = M4xMsaaState ? 4 : 1;
	DepthStencileDesc.SampleDesc.Quality =  M4xMsaaState ? (M4xMSAAQuality - 1) : 0; // 这个地方depthstencil的大小和采样必须和backbuffer的stencil的大小和采样相对应，不然会出错。
	DepthStencileDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;//内存布局：通常为 UNKNOWN 或 ROW_MAJOR（缓冲区必须是后者）。
	DepthStencileDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE OptClear;
	OptClear.Format = MDepthStencilFormat;
	OptClear.DepthStencil.Depth = 1.0f;
	OptClear.DepthStencil.Stencil = 0;

	CD3DX12_HEAP_PROPERTIES DepthStencilDeaultHeap(D3D12_HEAP_TYPE_DEFAULT);
	//D3D12_RESOURCE_STATE_COMMON 是在指定资源的状态
	ThrowIfFailed(MD3dDevice->CreateCommittedResource(&DepthStencilDeaultHeap, D3D12_HEAP_FLAG_NONE, &DepthStencileDesc, D3D12_RESOURCE_STATE_COMMON, &OptClear, IID_PPV_ARGS(&MDepthStencilBuffer)));


	//创建深度缓冲视图
	D3D12_DEPTH_STENCIL_VIEW_DESC DSVDesc;
	DSVDesc.Flags = D3D12_DSV_FLAG_NONE;
	DSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;//如果资源是MSAA的，这里必须使用D3D12_DSV_DIMENSION_TEXTURE2DMS
	DSVDesc.Format = MDepthStencilFormat;
	DSVDesc.Texture2D.MipSlice = 0;
	MD3dDevice->CreateDepthStencilView(MDepthStencilBuffer.Get(), &DSVDesc, DepthStencilView());

	//通过资源屏障，将depthstencilbuffer指定为可写状态
	CD3DX12_RESOURCE_BARRIER DepthStencileBufferCommonToDepthWriteBarrier = CD3DX12_RESOURCE_BARRIER::Transition(MDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	MCommandList->ResourceBarrier(1, &DepthStencileBufferCommonToDepthWriteBarrier);
}

ID3D12Resource* D3DApp::CurrentBackBuffer() const
{
	return MSwapChainBuffer[mCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(MRTVHeap->GetCPUDescriptorHandleForHeapStart(), mCurrBackBuffer, MRTVDescriptorSize);
}

void D3DApp::LogAdapters()
{
	UINT i = 0;
	IDXGIAdapter* Adapter = nullptr;
	std::vector<IDXGIAdapter*> AdapterVector;
	while (MDxgiFactory->EnumAdapters(i, &Adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc;
		Adapter->GetDesc(&desc);
		AdapterVector.push_back(Adapter);
		char Output[256];
		sprintf(Output, "%ws", desc.Description);
		i++;
		WarningStringMsg("D3DApp::LogAdapters Adapter Index:%d, Adapter:%s", i, Output);
	}
}

void D3DApp::CalculateFrameStats()
{
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period.
	if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

		std::string fpsStr = to_string(fps);
		std::string mspfStr = to_string(mspf);

		std::string windowText = MMainWndCaption +
			"    fps: " + fpsStr +
			"   mspf: " + mspfStr;

		SetWindowText(MhMainWnd, windowText.c_str());
		// Reset for next average.
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

void D3DApp::SetDxDebugInfoDetailsSetting() const
{
	ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(MD3dDevice->QueryInterface(IID_PPV_ARGS(&infoQueue))))
	{
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE); // 可选：在调试器中中断
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE); // 可选：在调试器中中断
		D3D12_INFO_QUEUE_FILTER filter = {};
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_MESSAGE,D3D12_MESSAGE_SEVERITY_INFO };//忽略message和info级别，保留warning及以上的信息
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		infoQueue->PushStorageFilter(&filter);
	}
}

void D3DApp::LogD3D12Messages() const
{

	ComPtr<ID3D12InfoQueue> infoQueue;
	if (FAILED(MD3dDevice->QueryInterface(IID_PPV_ARGS(&infoQueue))))
		return;

	const UINT64 numMessages = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();

	for (UINT64 i = 0; i < numMessages; ++i)
	{
		SIZE_T messageLength = 0;
		infoQueue->GetMessage(i, nullptr, &messageLength);

		std::vector<char> messageData(messageLength);
		D3D12_MESSAGE* message = reinterpret_cast<D3D12_MESSAGE*>(messageData.data());
		infoQueue->GetMessage(i, message, &messageLength);
		switch (message->Severity)
		{
		case D3D12_MESSAGE_SEVERITY_CORRUPTION:
			ErrorStringMsg("[CORRUPTION] %s\n", message->pDescription);
			break;
		case D3D12_MESSAGE_SEVERITY_ERROR:
			ErrorStringMsg("[ERROR] %s\n", message->pDescription);
			break;
		case D3D12_MESSAGE_SEVERITY_WARNING:
			WarningStringMsg("[WARNING] %s\n", message->pDescription);
			break;
		case D3D12_MESSAGE_SEVERITY_INFO:
			DebugStringMsg("[INFO] %s\n", message->pDescription);
			break;
		case D3D12_MESSAGE_SEVERITY_MESSAGE:
			LogStringMsg("[MESSAGE] %s\n", message->pDescription);
			break;
		}
	}

	infoQueue->ClearStoredMessages();
}
