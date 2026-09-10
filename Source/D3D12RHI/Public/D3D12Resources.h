#pragma once
#include "D3D12RHIModule.h"
#include "D3D12RHIPrivate.h"
#include "RHIResources.h"
class FD3D12Device;
using Microsoft::WRL::ComPtr;
/*
 * FD3D12Resource是对 ID3D12Resource 的低层封装——不是 FRHIResource(那是 RHI 抽象层;FD3D12Resource 是 D3D12 内部对象,后面 FD3D12Buffer/FD3D12Texture 会 持有 它)。
我们保留:ID3D12Resource + Device 回指 + Desc + HeapType + GPU 虚拟地址 + 一个当前状态。
砍掉:residency(显存驻留)、aftermath(崩溃调试)、reserved tiles(稀疏资源)、UAV 别名、per-subresource 状态位——这些都是进阶/多线程细节。
状态跟踪简化:UE 新版用 ED3D12Access 抽象 + 复杂 barrier 系统,我们先用一个裸 D3D12_RESOURCE_STATES,转换时手动更新(够用到第6章 CommandList)。
DeviceChild 内联:和之前 FD3D12Device 内联 AdapterChild 一样,这里把 FD3D12DeviceChild 内联成 FD3D12Device* Parent。
 */

class D3D12RHIMODULE FD3D12Resource
{
public:
	FD3D12Resource(FD3D12Device* InParent, ID3D12Resource* InResource, D3D12_RESOURCE_STATES InInitialSatte, const D3D12_RESOURCE_DESC& InDesc, D3D12_HEAP_TYPE InHeapType);
	~FD3D12Resource();

	ID3D12Resource* GetResource()          const { return Resource.Get(); }
	const D3D12_RESOURCE_DESC& GetDesc()              const { return Desc; }
	D3D12_HEAP_TYPE            GetHeapType()          const { return HeapType; }
	D3D12_GPU_VIRTUAL_ADDRESS  GetGPUVirtualAddress() const { return GPUVirtualAddress; }
	FD3D12Device* GetParentDevice()      const { return Parent; }  // UE: FD3D12DeviceChild
	

private:
	//回指Device
	FD3D12Device* Parent = nullptr;// UE: FD3D12DeviceChild::ParentDevice（内联）
	//真正的资源句柄
	ComPtr<ID3D12Resource> Resource;//  UE: TRefCountPtr<ID3D12Resource>
	//资源默认状态
	D3D12_RESOURCE_STATES State = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DESC       Desc{}; //  资源描述
	//资源堆类型
	D3D12_HEAP_TYPE HeapType = D3D12_HEAP_TYPE_DEFAULT;

	//GPU侧显存地址
	D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddress = 0;
};

/*
 * UE 建 buffer 的关键点(D3D12Buffer.cpp):CD3DX12_RESOURCE_DESC::Buffer(Size) + HeapType = bIsDynamic ? UPLOAD : DEFAULT + Adapter.CreateBuffer 里包 CreateCommittedResource。
 * 我们不引 d3dx12.h 那套辅助宏,手写原始结构体(更能看清底层)
 */
 // UE: class FD3D12Buffer : FRHIBuffer, FD3D12BaseShaderResource, FD3D12LinkedAdapterObject<...>
 // 简化：直接持一个 FD3D12Resource（committed，一 buffer 一资源），跳过 ResourceLocation 子分配/池化
class D3D12RHIMODULE FD3D12Buffer:public FRHIBuffer
{
public:
	FD3D12Buffer(FD3D12Device* InParent, const FRHIBufferDesc& InDesc)
		:FRHIBuffer(InDesc),
		Parent(InParent)
	{}
	~FD3D12Buffer() = default;
	FD3D12Resource* GetResource()     const { return ResourcePtr.get(); }
	FD3D12Device* GetParentDevice() const { return Parent; }
	void SetResource(std::unique_ptr<FD3D12Resource> InResource) { ResourcePtr = std::move(InResource); }

private:
	FD3D12Device* Parent = nullptr;
	std::unique_ptr<FD3D12Resource> ResourcePtr;// UE: FD3D12ResourceLocation 里的 TRefCountPtr<FD3D12Resource>
};


