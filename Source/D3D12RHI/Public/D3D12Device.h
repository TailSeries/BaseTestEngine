#pragma once
#include "D3D12RHIModule.h"
#include "D3D12RHIPrivate.h"
#include "D3D12Queue.h"
#include <vector>
#include <memory>
/*
 *UE 三层的所有权是这样分的
 * FD3D12Adapter  拥有 RootDevice(ID3D12Device) + DxgiFactory + DxgiAdapter
   └─ FD3D12Device  只持 Adapter 回指 + GPUIndex + Queues,GetDevice() 转发给 Adapter
        └─ FD3D12Queue  持 Device 回指 + D3DCommandQueue + Fence

class FD3D12Device final : public FD3D12SingleNodeGPUObject, public FNoncopyable, public FD3D12AdapterChild
三个基类的作用:FD3D12AdapterChild 提供 ParentAdapter + GetParentAdapter();FD3D12SingleNodeGPUObject 装 GPU 掩码(单节点);FNoncopyable 禁拷贝。我们简化:去掉三个基类,把它们的精华(Adapter 回指 + GPUIndex + 禁拷贝)直接内联进类。骨架/命名不变。
 */

class FD3D12Adapter;
// UE: class FD3D12Device final : FD3D12SingleNodeGPUObject, FNoncopyable, FD3D12AdapterChild
// 简化：去掉三个基类，内联其精华（Adapter 回指 + GPUIndex + 禁拷贝）
class D3D12RHIMODULE FD3D12Device
{
public:
    FD3D12Device(FD3D12Adapter* InAdapter, uint32 InGPUIndex);
    ~FD3D12Device();
    FD3D12Device(const FD3D12Device&) = delete;
    FD3D12Device& operator=(const FD3D12Device&) = delete;

    // UE 同名：转发到 Adapter->GetD3DDevice()（设备对象归 Adapter 所有）
    ID3D12Device* GetDevice();

    FD3D12Adapter* GetParentAdapter() const { return Adapter; }
    uint32 GetGPUIndex() const { return GPUIndex; }
    FD3D12Queue& GetQueue(ED3D12QueueType QueueType) { return *Queues[(uint32)QueueType]; }
private:
    FD3D12Adapter* Adapter = nullptr;  // UE: FD3D12AdapterChild::ParentAdapter
    uint32         GPUIndex = 0;         // UE: FD3D12SingleNodeGPUObject 的 GPU 掩码简化 对应的就是NodeMask
    // UE: TArray<FD3D12Queue, TFixedAllocator<Count>> Queues
	// FD3D12Queue non-movable，故存 unique_ptr（
    std::vector<std::unique_ptr<FD3D12Queue>> Queues;

};