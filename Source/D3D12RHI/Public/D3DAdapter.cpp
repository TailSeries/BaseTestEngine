#include "D3DAdapter.h"

#include "../../Core/Base/GenericPlatform.h"

bool FD3D12Adapter::FindAdapter(FD3D12AdapterDesc& OutDesc)
{
	ComPtr<IDXGIFactory4> Factory;
	VERIFY_D3D12(CreateDXGIFactory2(0, IID_PPV_ARGS(&Factory)));
	const D3D_FEATURE_LEVEL MinFeatureLevel = D3D_FEATURE_LEVEL_11_0;
	ComPtr<IDXGIAdapter1> TempAdapter;
	for (uint32 i = 0; Factory->EnumAdapters1(i, &TempAdapter) != DXGI_ERROR_NOT_FOUND;i++)
	{
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
			return true;// 选第一块可用独显（UE 会按显存/GPU 偏好挑最佳，这里从简）
		}
	}

	return false;
}

//CreateRootDevice — 建持久 factory + 取回选中卡 + 真正建设备
void FD3D12Adapter::CreateRootDevice()
{
	VERIFY_D3D12(CreateDXGIFactory2(0, IID_PPV_ARGS(&DxgiFactory)));
	VERIFY_D3D12(DxgiFactory->EnumAdapters((uint32)Desc.AdapterIndex, &DxgiAdapter));
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
	if (Device != nullptr)
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