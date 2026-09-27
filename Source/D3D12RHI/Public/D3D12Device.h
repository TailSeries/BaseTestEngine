#pragma once
#include "D3D12RHIModule.h"
#include "D3D12RHIPrivate.h"
#include "D3D12Queue.h"
#include <vector>
#include <memory>
#include "D3D12Resources.h"
#include "RHIResources.h"
#include "GenericPlatform.h"
/*
 *UE 三层的所有权是这样分的
 * FD3D12Adapter  拥有 RootDevice(ID3D12Device) + DxgiFactory + DxgiAdapter
   └─ FD3D12Device  只持 Adapter 回指 + GPUIndex + Queues,GetDevice() 转发给 Adapter
        └─ FD3D12Queue  持 Device 回指 + D3DCommandQueue + Fence

class FD3D12Device final : public FD3D12SingleNodeGPUObject, public FNoncopyable, public FD3D12AdapterChild
三个基类的作用:FD3D12AdapterChild 提供 ParentAdapter + GetParentAdapter();FD3D12SingleNodeGPUObject 装 GPU 掩码(单节点);FNoncopyable 禁拷贝。我们简化:去掉三个基类,把它们的精华(Adapter 回指 + GPUIndex + 禁拷贝)直接内联进类。骨架/命名不变。
 */

class FD3D12CommandContext;
class FD3D12DescriptorHeap;
class FD3D12Adapter;
class FD3D12Buffer;
struct FRHIBufferDesc;
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

    // Device 持有默认 Context；调用者只借用，不负责释放。
    FD3D12CommandContext& GetDefaultCommandContext()
    {
        return *ImmediateCommandContext;
    }

    // RHICreateBuffer 经 DynamicRHI 转发到此处，当前后端使用 committed resource。
    TRefCountPtr<FD3D12Buffer> CreateBuffer(const FRHIBufferDesc& Desc, const void* InitialData = nullptr);

    // 按 Flags 分派 RGBA8 采样纹理与 D32 深度纹理；不支持的用途组合显式拒绝。
    TRefCountPtr<FD3D12Texture> CreateTexture(const FRHITextureDesc& Desc, const void* InitialData = nullptr);


    // 教学阶段的单个 shader-visible 资源堆。
	// 后续再拆分完整的描述符管理器和缓存。
    FD3D12DescriptorHeap* GetResourceDescriptorHeap() const
    {
        return ResourceDescriptorHeap.get();
    }
    FD3D12DescriptorHeap* GetRenderTargetDescriptorHeap() const
    {
        return RenderTargetDescriptorHeap.get();
    }
    FD3D12DescriptorHeap* GetDepthStencilDescriptorHeap() const
    {
        return DepthStencilDescriptorHeap.get();
    }
private:
    // 项目内部辅助函数，由统一 CreateTexture 入口分派。
	// 不是新增的 RHI 接口。
    TRefCountPtr<FD3D12Texture> CreateDepthBuffer(const FRHITextureDesc& TextureDesc);

private:
    FD3D12Adapter* Adapter = nullptr;  // UE: FD3D12AdapterChild::ParentAdapter
    uint32         GPUIndex = 0;         // UE: FD3D12SingleNodeGPUObject 的 GPU 掩码简化 对应的就是NodeMask
    // UE: TArray<FD3D12Queue, TFixedAllocator<Count>> Queues
	// FD3D12Queue non-movable，故存 unique_ptr（
    std::vector<std::unique_ptr<FD3D12Queue>> Queues;
    std::unique_ptr<FD3D12DescriptorHeap> ResourceDescriptorHeap;
    // CPU 附件堆；View 析构归还槽位，调用方负责 GPU 生命周期。
    std::unique_ptr<FD3D12DescriptorHeap> DepthStencilDescriptorHeap;
    std::unique_ptr<FD3D12DescriptorHeap> RenderTargetDescriptorHeap;
    // 逆序析构：Context 先于描述符堆和 Queue 销毁。
    std::unique_ptr<FD3D12CommandContext> ImmediateCommandContext;
};


