#include "D3D12Viewport.h"
#include "D3D12Adapter.h"
#include "BaseDefines.h"
#include <stdexcept>
#include <utility>
#include "D3D12Viewport.h"
#include "D3D12DynamicRHI.h"
#include "D3D12Device.h"
#include "D3D12View.h"
#include "D3D12CommandContext.h"

// 构造函数只存参数（对齐 UE：不建 swap chain）
FD3D12Viewport::FD3D12Viewport(FD3D12Adapter* InAdapter, HWND InWindowHandle, uint32 InSizeX, uint32 InSizeY, DXGI_FORMAT InFormat, uint32 InNumBackBuffers)
	:Adapter(InAdapter)
	, WindowHandle(InWindowHandle)
	, SizeX(InSizeX)
	, SizeY(InSizeY)
	, Format(InFormat)
	, NumBackBuffers(InNumBackBuffers)
{
	BackBuffers.resize(NumBackBuffers);
}

FD3D12Viewport::~FD3D12Viewport()
{

}
// UE 的 Init()：建 swap chain + 取后备缓冲
void FD3D12Viewport::Init()
{
	/*
	 * 1. 为什么 swap chain 只用 Direct 队列?
DXGI 规定 CreateSwapChainForHwnd 收的必须是 Direct(图形)命令队列(D3D12_COMMAND_LIST_TYPE_DIRECT)。
Present 这个动作要绑在能执行图形/呈现命令的队列上,Copy 队列和 Compute 队列都不行。所以这里固定取 ED3D12QueueType::Direct——不是"随便挑一个",是硬性要求。
	 */
	ID3D12CommandQueue* CommandQueue = Adapter->GetDevice()->GetQueue(ED3D12QueueType::Direct).GetD3DQueue();



	DXGI_SWAP_CHAIN_DESC1 Desc{};
	Desc.Width = SizeX;
	Desc.Height = SizeY;
	Desc.Format = Format;
	Desc.Stereo = false;// 立体 3D(左右眼两张图,给 3D 显示器/立体设备用)。TRUE 会创建每帧含左右眼两份的立体 swap chain。普通渲染填 FALSE。
	Desc.SampleDesc.Count = 1;//Flip 模型本身不支持 MSAA，但实现 MSAA 的“标准做法”是自己创建一个独立的、开启了多采样的渲染目标，在它上面完成所有渲染，然后在最后一步将数据“解析”（Resolve）到用于显示的单采样后备缓冲区（Back Buffer）上
	Desc.SampleDesc.Quality = 0;
	Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	Desc.BufferCount = NumBackBuffers;
	Desc.Scaling = DXGI_SCALING_NONE;//后备缓冲尺寸和窗口/输出尺寸不一致时怎么缩放:不缩放,1:1 像素映射。窗口变大时边缘区域未定义——要求你自己处理 resize(我们正是这么做的,所以用 NONE)
	Desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;//对于D3D12 我们强制指定flip
	Desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;//后备缓冲的 alpha 通道如何参与桌面合成(和 DWM/其他窗口叠加):IGNORE / PREMULTIPLIED / STRAIGHT / UNSPECIFIED。只对透明/分层窗口有意义;普通不透明 HWND 窗口填 UNSPECIFIED 即可。
	Desc.Flags = 0;
	ComPtr<IDXGISwapChain1> SwapChain1;

	//6. FullscreenDesc 为什么传 nullptr ? UE 传了那个结构体,是因为它要支持全屏切换——结构体里 Windowed = !bIsFullscreen 控制窗口/全屏,还带刷新率设置,目前 我们第1章只做窗口模式,nullptr 就够了。等要全屏再把结构体补上,属"简化内部细节"。
	VERIFY_D3D12(Adapter->GetDXGIFactory()->CreateSwapChainForHwnd(CommandQueue, WindowHandle, &Desc, nullptr, nullptr, &SwapChain1));
	/*
	 * 关掉 DXGI 默认的 Alt+Enter 自动全屏。 DXGI 会偷偷在你的窗口上装一个消息钩子,拦截 Alt+Enter 自动切全屏——这常和你自己的全屏逻辑打架。
	 * DXGI_MWA_NO_ALT_ENTER 禁掉它。必须在 swap chain 创建之后调(用创建它的那个 factory)。
	 */
	Adapter->GetDXGIFactory()->MakeWindowAssociation(WindowHandle, DXGI_MWA_NO_ALT_ENTER);
	/*
	 * 接口版本升级(QueryInterface)。 CreateSwapChainForHwnd 只返回基础的 IDXGISwapChain1,但我们要用 GetCurrentBackBufferIndex()——这方法是 IDXGISwapChain3 才加的。
	 * .As() 是 WRL ComPtr 的 QI 封装:对同一个对象做 COM 查询,拿到它的 IDXGISwapChain3 接口指针。
	 */
	VERIFY_D3D12(SwapChain1.As(&SwapChain)); // QI 到 IDXGISwapChain3

	ResizeInternal(); // 取回后备缓冲
}

// UE 的 ResizeInternal()：从当前 swap chain 取回 N 个后备缓冲 暂存
void FD3D12Viewport::ResizeInternal()
{

	// 当前 RHI 格式表只支持这一种交换链颜色格式。
	if (Format != DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		throw std::invalid_argument(
			"Only RGBA8 swap chain buffers are supported");
	}

	FD3D12Device* Device = Adapter->GetDevice();


	for (uint32 Index = 0; Index < NumBackBuffers; ++Index)
	{
		ComPtr<ID3D12Resource> NativeBackBuffer;
		//这里也没有调用 CreateCommittedResource()：资源已经由 SwapChain 创建。
		const HRESULT Result = SwapChain->GetBuffer(
			Index,
			IID_PPV_ARGS(NativeBackBuffer.GetAddressOf()));

		if (FAILED(Result))
		{
			throw std::runtime_error("Get swap chain buffer failed");
		}

		const D3D12_RESOURCE_DESC NativeDesc = NativeBackBuffer->GetDesc();


		FRHITextureDesc TextureDesc(static_cast<uint32>(NativeDesc.Width), NativeDesc.Height, PF_R8G8B8A8_UNORM);
		TextureDesc.Flags = ETextureCreateFlags::RenderTargetable;
		TextureDesc.ClearValue = FClearValueBinding(std::array<float, 4>{0.2f, 0.4f, 0.8f, 1.0f});

		TRefCountPtr<FD3D12Texture> Texture =
			std::make_shared<FD3D12Texture>(
				Device, TextureDesc);


		auto Resource = std::make_unique<FD3D12Resource>(
			Device,
			NativeBackBuffer.Get(),
			D3D12_RESOURCE_STATE_PRESENT,
			NativeDesc,
			D3D12_HEAP_TYPE_DEFAULT);

		Texture->SetResource(std::move(Resource));
        Texture->MarkAsBackBuffer();
        D3D12_RENDER_TARGET_VIEW_DESC RTVDesc{};
        RTVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        auto View = std::make_unique<FD3D12RenderTargetView>(
            Device, Device->GetRenderTargetDescriptorHeap());
        View->CreateView(Texture->GetResource(), RTVDesc);
        Texture->SetRenderTargetView(std::move(View));

		BackBuffers[Index] = std::move(Texture);
	}

}

void FD3D12Viewport::Resize(uint32 NewSizeX, uint32 NewSizeY)
{
	if (NewSizeX == SizeX && NewSizeY == SizeY)
	{
		return;
	}
	// 前提：GPU 已完成使用，且外部已释放旧 backbuffer 引用；下面释放 Viewport 持有的纹理包装。
	for (auto& BackBuffer : BackBuffers)
		BackBuffer.reset();
	// ResizeBuffers 会把旧的后备缓冲销毁、按新尺寸重建全新的资源,旧的 ID3D12Resource 全部作废。原因是 D3D12 里纹理资源的尺寸创建后不可变——想换大小,只能销毁旧的、造新的
	//所以我们必须重新调用 ResizeInternal来获取新的backbuffer资源指针
	VERIFY_D3D12(SwapChain->ResizeBuffers(NumBackBuffers, NewSizeX, NewSizeY, Format, 0));
	SizeX = NewSizeX;
	SizeY = NewSizeY;
	ResizeInternal();
}

/*
 * SyncInterval = Present 时等待多少个垂直同步间隔(vblank)再把这一帧显示出来,就是控制垂直同步(vsync)的:
 *  值  │         行为         │                     效果                     │
  ├─────┼──────────────────────┼──────────────────────────────────────────────┤
  │ 0   │ 不等 vblank,尽快呈现 │ 不锁帧(可能撕裂,需配合 tearing 标志才真撕裂) │
  ├─────┼──────────────────────┼──────────────────────────────────────────────┤
  │ 1   │ 等 1 个 vblank       │ vsync 开,锁到刷新率(60Hz→60fps),无撕裂       │
  ├─────┼──────────────────────┼──────────────────────────────────────────────┤
  │ 2   │ 等 2 个 vblank       │ 半刷新率(60Hz→30fps)                         │
  ├─────┼──────────────────────┼──────────────────────────────────────────────┤
  │ 3/4 │ 等 3/4 个 vblank     │ 1/3、1/4 刷新率                              │
  └─────┴──────────────────────┴──────────────────────────────────────────────┘
  flip 模型下有个细节要注意

我们用的是 DXGI_SWAP_EFFECT_FLIP_DISCARD,SyncInterval = 0 时行为和老 BLT 模型不同:

- SyncInterval >= 1:帧排队,vblank 时翻页显示,永不撕裂
- SyncInterval = 0:不阻塞、尽快提交,但光这样在 flip 模型下还是不撕裂(桌面合成器接管)——fps 能超过刷新率,但多余的帧会被丢弃(DISCARD 语义)

要真正的"不锁帧 + 撕裂"(跑分/低延迟那种),得三样齐备:
1. swap chain 创建时 Desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
2. Present 的 SyncInterval = 0
3. Present 的第二参 Flags |= DXGI_PRESENT_ALLOW_TEARING
 */
void FD3D12Viewport::PresentInternal(int32 SyncInterval)
{
	VERIFY_D3D12(SwapChain->Present((UINT)SyncInterval, 0));
}

//这条接口只获取当前纹理，不执行 Present，也不切换 backbuffer
TRefCountPtr<FRHITexture> FD3D12DynamicRHI::RHIGetViewportBackBuffer(FRHIViewport* Viewport)
{
	if (!Viewport)
	{
		throw std::invalid_argument("Viewport must not be null");
	}

	auto* D3DViewport = static_cast<FD3D12Viewport*>(Viewport);

	return D3DViewport->GetBackBufferRef();
}


TRefCountPtr<FRHIViewport> FD3D12DynamicRHI::RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PixelFormat)
{
	if (!GetAdapter() || !GetDevice())
	{
		throw std::logic_error("RHI has not been initialized");
	}

	if (!WindowHandle || SizeX == 0 || SizeY == 0)
	{
		throw std::invalid_argument(
			"Viewport requires a window and nonzero dimensions");
	}

	if (bIsFullscreen)
	{
		throw std::invalid_argument(
			"Fullscreen viewports are not supported yet");
	}

	if (PixelFormat != PF_R8G8B8A8_UNORM)
	{
		throw std::invalid_argument(
			"Only RGBA8 viewports are supported");
	}

	constexpr uint32 NumBackBuffers = 2;

	TRefCountPtr<FD3D12Viewport> Viewport =
		std::make_shared<FD3D12Viewport>(
			GetAdapter(),
			static_cast<HWND>(WindowHandle),
			SizeX,
			SizeY,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			NumBackBuffers);

	Viewport->Init();

	return Viewport;
}

void FD3D12DynamicRHI::RHIEndDrawingViewport(FRHICommandListImmediate& RHICmdList,
    FRHIViewport* Viewport, const FRHIPresentArgs& PresentArgs)
{
    (void)RHICmdList;
    // 单线程简化：EndFrame 已完成提交；UE 在此调度 flush/present payload。
    if (!Viewport || Device->GetDefaultCommandContext().IsFrameOpen())
        throw std::logic_error("EndFrame before presenting a valid viewport");
    if (PresentArgs.bPresent)
        static_cast<FD3D12Viewport*>(Viewport)->PresentInternal(PresentArgs.bLockToVsync ? 1 : 0);
}
