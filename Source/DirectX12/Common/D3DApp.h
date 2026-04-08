#pragma once
#if defined(DEBUG) || defined(_DEBUG)
#define  _CRTDBG_MAP_ALLOC //这会重定义 malloc、calloc、realloc、free 等函数，使它们可以被 CRT（C Runtime）调试机制追踪。启用它后，分配内存的位置（例如在哪一行 malloc 的）可以被记录。
#include <crtdbg.h>
#endif
#include "D3DUtil.h"
#include "Common/LogAssert.h"
#include "ModulePublic/DXModule.h"
#include "GameTimer.h"

//单例类
class DXMODULE D3DApp
{
protected:
	D3DApp(HINSTANCE hInstance);
	virtual ~D3DApp();
public:
	D3DApp(const D3DApp&) = delete;
	D3DApp operator=(const D3DApp&) = delete;
public:
	static D3DApp* GetApp();
	virtual bool Initialize();
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	int Run();
protected:
	bool InitMainWindow();
	bool InitDirect3D();

	virtual void OnResize();
	virtual void Update(const GameTimer& gt) {};
	virtual void Draw(const GameTimer& gt) {};
	//一些鼠标输入的处理
	virtual void OnMouseDown(WPARAM btnState, int x, int y) { }
	virtual void OnMouseUp(WPARAM btnState, int x, int y) { }
	virtual void OnMouseMove(WPARAM btnState, int x, int y) { }
	void Set4xMsaaState(bool value);
	//强刷一遍命令队列
	void FlushCommandQueue();
	ID3D12Resource* CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;
private:

	//0 初始化dx12相关的debug信息
	void InitDirectX12DebugLayer();
	void CreateDXGIFactory();

	//1.创建device
	void CreateDevice();
	void CreateFence();
	void QueryDescriptorSize();
	void CheckMSAASupportLevel();

	//2.命令队列，命令列表，命令分配器
	void CreateCommandObjects();

	//3.交换链
	void CreateSwapChain();

	//4.创建描述符堆
	void CreateDescriptorHeap();


	//调整swapchain
	void ResizeAndReleaseSwapChains();
	//调整depth
	void ResizeDepthStencil();





private:
	void LogAdapters();
	void CalculateFrameStats();

	//可以控制更加详细的日志输出
	void SetDxDebugInfoDetailsSetting() const;
	void LogD3D12Messages() const;




private:
	static D3DApp* MApp;
	HINSTANCE MhAppInst{nullptr};
	HWND MhMainWnd{ nullptr };
	bool      mAppPaused = false;  // is the application paused?
	bool      mMinimized = false;  // is the application minimized?
	bool      mMaximized = false;  // is the application maximized?
	bool      mResizing = false;   // are the resize bars being dragged?
	bool      mFullscreenState = false;// fullscreen enabled
	GameTimer mTimer;
protected:
	//dxgi，它可以用来创建device以及图形资源等
	Microsoft::WRL::ComPtr<IDXGIFactory4> MDxgiFactory;
	//d3d device
	Microsoft::WRL::ComPtr<ID3D12Device> MD3dDevice;
	//d3d Fence
	Microsoft::WRL::ComPtr<ID3D12Fence1> MD3dFence;
	UINT64 CurrentFence = 0;

	//描述符大小
	UINT MRTVDescriptorSize{ 0 };
	UINT MDSVDescriptorSize{ 0 };
	UINT MCBVSRVUAVDescriptorSize{ 0 };
	//描述符堆
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> MRTVHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> MDSVHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> MDepthStencilBuffer;

	// 后台缓冲区的默认输出格式
	DXGI_FORMAT MBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	// depthstencil buffer的格式
	DXGI_FORMAT MDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 对当前格式支持的MSAALevel的情况 0表示该格式不支持所请求的 MSAA 级别
	UINT M4xMSAAQuality{ 0 };
	bool M4xMsaaState{ false };

	// 命令队列
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> MCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> MCommandAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> MCommandList;

	//交换链
	Microsoft::WRL::ComPtr<IDXGISwapChain> MSwapChain;
	int MClientWidth = 1440;
	int MClientHeight = 720;
	static const int SwapChainBufferCount = 2;
	int mCurrBackBuffer = 0;//当前的backbufferindex
	Microsoft::WRL::ComPtr<ID3D12Resource> MSwapChainBuffer[SwapChainBufferCount];

	//视口,裁剪信息
	D3D12_VIEWPORT MScreenViewPort;
	D3D12_RECT MScissorRect;


private:
	std::string MMainWndCaption = "Lovely Engine";


};
