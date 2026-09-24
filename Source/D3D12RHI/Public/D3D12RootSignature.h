#pragma once
#include "D3D12RHIModule.h"
#include "D3D12RHIPrivate.h"
#include "D3D12Util.h"

#include <array>
#include <map>
#include <memory>

class FD3D12Device;
using Microsoft::WRL::ComPtr;

// UE 同名枚举中包含更多类型。
// 当前只需要这两个绑定位置。
enum ERootParameterKeys
{
	PS_SRVs,
	VS_RootCBVs,
	RPK_RootParameterKeyCount
};

// 负责把 QBSS 转换为原生根签名描述。根据布局需求组装 D3D12 描述
// 描述中的指针指向本对象的数组，因此禁止复制和移动
class D3D12RHIMODULE FD3D12RootSignatureDesc
{
public:
	explicit FD3D12RootSignatureDesc(
		const FD3D12QuantizedBoundShaderState& QBSS);

	FD3D12RootSignatureDesc(const FD3D12RootSignatureDesc&) = delete;
	FD3D12RootSignatureDesc& operator=(
		const FD3D12RootSignatureDesc&) = delete;


	const D3D12_ROOT_SIGNATURE_DESC& GetDesc() const
	{
		return RootDesc;
	}

	//某一类用途的 Shader 资源，在当前根签名中对应第几个 Root Parameter。
	int32 GetRootParameterSlot(ERootParameterKeys Key) const
	{
		return RootParameterSlots[Key];
	}

private:
	std::array<D3D12_ROOT_PARAMETER, 2> TableSlots{};
	std::array<D3D12_DESCRIPTOR_RANGE, 1> DescriptorRanges;
	std::array<D3D12_STATIC_SAMPLER_DESC, 1> StaticSamplers{};
	std::array<int32, RPK_RootParameterKeyCount> RootParameterSlots{};
	D3D12_ROOT_SIGNATURE_DESC RootDesc{};
};

//持有原生根签名和绑定位置映射
class D3D12RHIMODULE FD3D12RootSignature:public FD3D12AdapterChild
{
public:
	FD3D12RootSignature(FD3D12Adapter* InParent, const FD3D12QuantizedBoundShaderState& InQBSS);
	~FD3D12RootSignature();
	ID3D12RootSignature* GetRootSignature() const { return RootSignature.Get(); }
	int32 GetRootParameterSlot(ERootParameterKeys Key) const
	{
		return RootParameterSlots[Key];
	}
private:
	std::array<int32, RPK_RootParameterKeyCount> RootParameterSlots{};
	ComPtr<ID3D12RootSignature> RootSignature;
};

//按 Key 缓存、复用、持有根签名
class D3D12RHIMODULE FD3D12RootSignatureManager:public FD3D12AdapterChild
{
public:
	explicit FD3D12RootSignatureManager(FD3D12Adapter* InParent)
		: FD3D12AdapterChild(InParent)
	{}
	FD3D12RootSignatureManager(const FD3D12RootSignatureManager&) = delete;
	FD3D12RootSignatureManager& operator=(const FD3D12RootSignatureManager&) = delete;

	FD3D12RootSignature* GetRootSignature(const FD3D12QuantizedBoundShaderState& QBSS);

	void Destroy();
private:
	FD3D12RootSignature* CreateRootSignature(const FD3D12QuantizedBoundShaderState& QBSS);

	std::map<FD3D12QuantizedBoundShaderState, std::unique_ptr<FD3D12RootSignature>> RootSignatureMap;
};

