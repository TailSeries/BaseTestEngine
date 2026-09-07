#pragma once
#include "D3D12RHIModule.h"
#include "D3D12RHIPrivate.h"
#include "D3D12Device.h"
/*
 * UE 选卡逻辑
 * ① 找卡 + 填 Desc(在 DynamicRHIModule 的 FindAdapter 里,用临时 factory 枚举挑 GPU)→ 造 FD3D12Adapter(Desc)
 * ② Adapter::InitializeDevices() → CreateRootDevice()(建持久 factory、D3D12CreateDevice)+ new FD3D12Device(...)(D3D12Adapter.cpp:1561)
 * 枚举每块 adapter → 跳过软件卡(WARP) → 用 D3D12CreateDevice 测试能否在 D3D_FEATURE_LEVEL_11_0 建设备 → 能就纳入候选
 */


// UE: struct FD3D12AdapterDesc（精简：只留 DXGI desc + AdapterIndex + 最高 FeatureLevel）
struct FD3D12AdapterDesc
{
	DXGI_ADAPTER_DESC Desc{}; // GPU 名 /显存
	int32 AdapterIndex = -1; // -1 表示没有找到
	D3D_FEATURE_LEVEL MaxSupportedFeature = (D3D_FEATURE_LEVEL)0;
	bool IsValid()
	{
		return AdapterIndex >= 0 && MaxSupportedFeature != (D3D_FEATURE_LEVEL)0;
	}
};

class D3D12RHIMODULE FD3D12Adapter
{
public:
	explicit FD3D12Adapter(const FD3D12AdapterDesc& InDesc);
	~FD3D12Adapter();
	FD3D12Adapter(const FD3D12Adapter&) = delete;
	FD3D12Adapter operator=(const FD3D12Adapter&) = delete;

	// 枚举 + 选 GPU + 填 Desc（UE 在 DynamicRHIModule::FindAdapter；Ch1 无模块，放这里）
	static bool FindAdapter(FD3D12AdapterDesc& OutDesc);
	// UE 同名：建 RootDevice + 创建 FD3D12Device 节点
	void InitializeDevices();


private:
	void CreateRootDevice();// UE 同名（去掉 bWithDebug / 调试层）
	FD3D12AdapterDesc Desc;
	ComPtr<IDXGIFactory4> DxgiFactory;// UE: DxgiFactory2..7，只留基础版
	ComPtr<IDXGIAdapter> DxgiAdapter;// / 选中的物理 GPU
	ComPtr<ID3D12Device> RootDevice;//  UE: RootDevice..12，只留基础版；命名保持 RootDevice
	FD3D12Device* Device = nullptr; // UE: Devices[MAX_NUM_GPUS]，单 GPU 只留一个
};