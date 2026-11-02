#pragma once


/*UE 里原始的继承链
 *class FD3D12ShaderResourceView_RHI
    : public FRHIShaderResourceView
    , public FD3D12ShaderResourceView
    , public FD3D12DeferredInitView<FD3D12ShaderResourceView_RHI>

 *我们保留前两条继承链，暂时省略延迟初始化、多 GPU、资源重命名监听：
 * FD3D12View
    ↑
TD3D12View<FD3D12ShaderResourceView, D3D12_SHADER_RESOURCE_VIEW_DESC>
    ↑
FD3D12ShaderResourceView
    ↑
FD3D12ShaderResourceView_RHI ← 同时继承 FRHIShaderResourceView
 */
#include "D3D12RHIModule.h"
#include "D3D12RHIPrivate.h"
#include "RHIResources.h"

class FD3D12Device;
class FD3D12DescriptorHeap;
class FD3D12Resource;
class D3D12RHIMODULE FD3D12View
{
public:
    FD3D12View(const FD3D12View&) = delete;
    FD3D12View& operator=(const FD3D12View&) = delete;

    bool IsInitialized() const
    {
        return bInitialized;
    }

    /*
     * GetCPUHandle() 用于 CPU 描述符访问；GetGPUHandle() 仅允许 shader-visible View（当前 SRV）。RTV 和 DSV 只有 CPU Handle。UE 完整实现涉及 offline descriptor、描述符复制与缓存，
     * 我们暂时不把这一整套搬进来，也不将这里的句柄命名为 OfflineCpuHandle，避免混淆。
     */
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle() const;

protected:
    FD3D12View(
        FD3D12Device* InDevice,
        FD3D12DescriptorHeap* InHeap);

    virtual ~FD3D12View();

    FD3D12Device* ParentDevice = nullptr;

    // 非拥有：描述符堆必须比使用它的 View 活得更久。
    FD3D12DescriptorHeap* Heap = nullptr;

    D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle{};

    uint32 DescriptorSlot = 0;
    bool bInitialized = false;
};

// 保留 UE 的模板层：保存不同类型 View 的原生描述。
// SRV、RTV 与 DSV 共用，之后 UAV 等可以复用。
template<typename ViewType, typename ViewDescType>
class TD3D12View : public FD3D12View
{
public:
    const ViewDescType& GetDesc() const
    {
        return Desc;
    }

protected:
    TD3D12View(
        FD3D12Device* InDevice,
        FD3D12DescriptorHeap* InHeap)
        : FD3D12View(InDevice, InHeap)
    {}

    ViewDescType Desc{};
};


// 后端颜色附件 View，由 FD3D12Texture 持有；backbuffer 创建时一并建立。
// 当前教学范围仅 RGBA8 Texture2D、单层、单 mip、单采样。
class D3D12RHIMODULE FD3D12RenderTargetView final : public TD3D12View<FD3D12RenderTargetView, D3D12_RENDER_TARGET_VIEW_DESC>
{
public:
    FD3D12RenderTargetView(FD3D12Device* InDevice, FD3D12DescriptorHeap* InHeap);
    void CreateView(FD3D12Resource* InResource, const D3D12_RENDER_TARGET_VIEW_DESC& InDesc);
};

// 由后端纹理持有；只支持可写 D32、单采样、mip 0。
class D3D12RHIMODULE FD3D12DepthStencilView final : public TD3D12View<FD3D12DepthStencilView, D3D12_DEPTH_STENCIL_VIEW_DESC>
{
public:
    FD3D12DepthStencilView(FD3D12Device* InDevice, FD3D12DescriptorHeap* InHeap);
    void CreateView(FD3D12Resource* InResource, const D3D12_DEPTH_STENCIL_VIEW_DESC& InDesc);
};

class D3D12RHIMODULE FD3D12ShaderResourceView : public TD3D12View<FD3D12ShaderResourceView, D3D12_SHADER_RESOURCE_VIEW_DESC>
{
public:
    FD3D12ShaderResourceView(
        FD3D12Device* InDevice,
        FD3D12DescriptorHeap* InHeap);

    void CreateView(FD3D12Resource* InResource, const D3D12_SHADER_RESOURCE_VIEW_DESC& InDesc);
};


// RHI 资源身份和所有权 + D3D12 描述符实现。
class D3D12RHIMODULE FD3D12ShaderResourceView_RHI : public FRHIShaderResourceView, public FD3D12ShaderResourceView
{
public:
    FD3D12ShaderResourceView_RHI(
        FD3D12Device* InDevice,
        FD3D12DescriptorHeap* InHeap,
        TRefCountPtr<FRHIViewableResource> InResource,
        const FRHIViewDesc& InViewDesc);

    void CreateView();
};
