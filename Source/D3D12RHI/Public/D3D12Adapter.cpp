#include "D3D12Adapter.h"
#include "Core/Base/GenericPlatform.h"

bool FD3D12Adapter::FindAdapter(FD3D12AdapterDesc& OutDesc)
{
	ComPtr<IDXGIFactory4> Factory;
	VERIFY_D3D12(CreateDXGIFactory2(0, IID_PPV_ARGS(&Factory)));
	ComPtr<IDXGIFactory6> Factory6;//用 IDXGIFactory6::EnumAdapterByGpuPreference 按"高性能"偏好枚举, 独显就会排在最前。这也正是 UE 的做法
	const bool bHasFactory6 = SUCCEEDED(Factory.As(&Factory6));
	const D3D_FEATURE_LEVEL MinFeatureLevel = D3D_FEATURE_LEVEL_11_0;
	ComPtr<IDXGIAdapter1> TempAdapter;
	for (uint32 i = 0; ;i++)
	{
		// 高性能偏好枚举；老系统无 Factory6 时 fallback 到默认顺序
		HRESULT hr = bHasFactory6
			? Factory6->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&TempAdapter))
			: Factory->EnumAdapters1(i, &TempAdapter);
		if (hr == DXGI_ERROR_NOT_FOUND)
			break;


		DXGI_ADAPTER_DESC1 Desc1{};
		TempAdapter->GetDesc1(&Desc1);

		//跳过软件光栅化卡（WARP
		if (Desc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		// 测试能否创建 D3D12 设备：最后一参传 nullptr = 只测试、不真正创建
		// （成功时返回 S_FALSE，仍属 SUCCEEDED）

		if (SUCCEEDED(D3D12CreateDevice(TempAdapter.Get(), MinFeatureLevel, __uuidof(ID3D12Device), nullptr)));
		{
			DXGI_ADAPTER_DESC AdapterDesc{};
			TempAdapter->GetDesc(&AdapterDesc);

			OutDesc.Desc = AdapterDesc;
			OutDesc.AdapterIndex = (int32)i;
			OutDesc.MaxSupportedFeature = MinFeatureLevel;

			{
				wchar_t Info[512];
				swprintf_s(Info,
					L"[D3D12] Selected Adapter #%u\n"
					L"  Name           : %s\n"
					L"  Dedicated VRAM : %llu MB\n"
					L"  Shared Sys Mem : %llu MB\n"
					L"  VendorId=0x%04X  DeviceId=0x%04X\n",
					i,
					AdapterDesc.Description,
					(uint64)AdapterDesc.DedicatedVideoMemory / (1024ull * 1024ull),
					(uint64)AdapterDesc.SharedSystemMemory / (1024ull * 1024ull),
					AdapterDesc.VendorId, AdapterDesc.DeviceId);

				OutputDebugStringW(Info);   // 调试器 Output 窗口 / DebugView 可见
				// MessageBoxW(nullptr, Info, L"Selected Adapter", MB_OK);  // 想直接弹窗就用这个
			}


			return true;// 选第一块可用独显（UE 会按显存/GPU 偏好挑最佳，这里从简）
		}
	}

	return false;
}

//CreateRootDevice — 建持久 factory + 取回选中卡 + 真正建设备
void FD3D12Adapter::CreateRootDevice()
{
	VERIFY_D3D12(CreateDXGIFactory2(0, IID_PPV_ARGS(&DxgiFactory)));
	// 用和 FindAdapter 相同的枚举方式，保证 AdapterIndex 指向同一块卡
	ComPtr<IDXGIAdapter1> Adapter1;
	ComPtr<IDXGIFactory6> Factory6;
	if (SUCCEEDED(DxgiFactory.As(&Factory6)))
	{
		VERIFY_D3D12(Factory6->EnumAdapterByGpuPreference(
			(uint32)Desc.AdapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&Adapter1)));
	}
	else
	{
		VERIFY_D3D12(DxgiFactory->EnumAdapters1((uint32)Desc.AdapterIndex, &Adapter1));
	}
	DxgiAdapter = Adapter1;   // IDXGIAdapter1 → IDXGIAdapter（上转，隐式）

	VERIFY_D3D12(D3D12CreateDevice(DxgiAdapter.Get(), Desc.MaxSupportedFeature, IID_PPV_ARGS(&RootDevice)));
}

//InitializeDevices — 建设备 + 创建 Device 节点
void FD3D12Adapter::InitializeDevices()
{
	if (!RootDevice)
	{
		CreateRootDevice();
	}
	// RootDevice 就绪后才能造 Device（Device 构造里建 Queue 会用到 GetD3DDevice）
	if (Device == nullptr)
	{
		Device = new FD3D12Device(this, 0);
	}
}

// 构造 / 析构
FD3D12Adapter::FD3D12Adapter(const FD3D12AdapterDesc& InDesc)
	: Desc(InDesc)
{}

FD3D12Adapter::~FD3D12Adapter()
{
	// 先删 Device（连带析构 Queues，释放从 RootDevice 建的 CommandQueue/Fence）
	// 之后 ComPtr 成员按声明逆序自动释放 RootDevice → DxgiAdapter → DxgiFactory
	delete Device;
	Device = nullptr;
}

/*
 *  串起来怎么用
 *	FD3D12AdapterDesc Desc;
 *	if (FD3D12Adapter::FindAdapter(Desc))   // 1. 挑卡
 *	{
 *	    FD3D12Adapter Adapter(Desc);        // 2. 造 Adapter
 *	    Adapter.InitializeDevices();        // 3. 建 RootDevice → Device → 三条 Queue
 *	    // 至此三层齐活：Adapter → Device → Queue[Direct/Copy/Async]
 *	}
 */
