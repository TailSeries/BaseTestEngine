#include "D3D12View.h"
#include "D3D12Device.h"
#include "D3D12Descriptors.h"
#include "D3D12Resources.h"

#include <stdexcept>
#include <utility>

FD3D12View::FD3D12View(FD3D12Device* InDevice, FD3D12DescriptorHeap* InHeap)
	: ParentDevice(InDevice)
	, Heap(InHeap)
{
	if (!ParentDevice || !Heap)
	{
		throw std::invalid_argument(
			"View requires a device and descriptor heap");
	}

	// 具体堆类型由派生 View 校验；RTV 和 DSV 不使用 shader-visible 堆。
}


D3D12_CPU_DESCRIPTOR_HANDLE FD3D12View::GetCPUHandle() const
{
	if (!bInitialized)
	{
		throw std::logic_error("View has not been initialized");
	}

	return CPUHandle;
}


D3D12_GPU_DESCRIPTOR_HANDLE FD3D12View::GetGPUHandle() const
{
	if (!bInitialized)
	{
		throw std::logic_error("View has not been initialized");
	}

	if ((Heap->GetHeap()->GetDesc().Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) == 0)
	{
		throw std::logic_error("View has no shader-visible GPU handle");
	}
	return GPUHandle;
}

FD3D12ShaderResourceView::FD3D12ShaderResourceView(
	FD3D12Device* InDevice,
	FD3D12DescriptorHeap* InHeap)
	: TD3D12View<
	FD3D12ShaderResourceView,
	D3D12_SHADER_RESOURCE_VIEW_DESC>(InDevice, InHeap)
{
	const auto HeapDesc = Heap->GetHeap()->GetDesc();
	if (HeapDesc.Type != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
		(HeapDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) == 0)
	{
		throw std::invalid_argument("SRV requires a shader-visible resource heap");
	}
}
void FD3D12ShaderResourceView::CreateView(FD3D12Resource* InResource, const D3D12_SHADER_RESOURCE_VIEW_DESC& InDesc)
{
	if (bInitialized)
	{
		throw std::logic_error("View has already been initialized");
	}

	if (!InResource || !InResource->GetResource())
	{
		throw std::invalid_argument("SRV resource must not be null");
	}

	// 所有参数检查应在分配槽位之前完成。
	const uint32 Slot = Heap->Allocate();
    DescriptorSlot = Slot;

	Desc = InDesc;
	CPUHandle = Heap->GetCPUHandle(Slot);
	GPUHandle = Heap->GetGPUHandle(Slot);

	ParentDevice->GetDevice()->CreateShaderResourceView(
		InResource->GetResource(),
		&Desc,
		CPUHandle);

	bInitialized = true;
}

FD3D12ShaderResourceView_RHI::FD3D12ShaderResourceView_RHI(FD3D12Device* InDevice, FD3D12DescriptorHeap* InHeap, TRefCountPtr<FRHIViewableResource> InResource, const FRHIViewDesc& InViewDesc)
	: FRHIShaderResourceView(std::move(InResource), InViewDesc)
	, FD3D12ShaderResourceView(InDevice, InHeap)
{}


void FD3D12ShaderResourceView_RHI::CreateView()
{
	// 两条继承链各有一个 GetDesc：
	// FRHIView::GetDesc() 返回 RHI 描述；
	// TD3D12View::GetDesc() 返回原生 D3D12 描述。
	const FRHIViewDesc& ViewDesc = FRHIView::GetDesc();
	const auto& SRVDesc = ViewDesc.Texture.SRV;


	FRHITexture* Texture = FRHIView::GetTexture();
	const FRHITextureDesc& TextureDesc = Texture->GetDesc();

	if (SRVDesc.Dimension != FRHIViewDesc::EDimension::Texture2D)
	{
		throw std::invalid_argument("Only Texture2D SRVs are supported");
	}

	if (SRVDesc.MipRange.First != 0 || SRVDesc.MipRange.Num != 1)
	{
		throw std::invalid_argument(
			"Current textures support only mip 0");
	}

	// PF_Unknown 表示沿用底层纹理格式。
	const EPixelFormat ViewFormat = SRVDesc.Format == PF_Unknown ? TextureDesc.Format : SRVDesc.Format;

	// 当前资源使用具体 RGBA8 格式创建。
	// 暂不支持 typeless、格式重解释或深度 SRV。

	if (TextureDesc.Format != PF_R8G8B8A8_UNORM || ViewFormat != TextureDesc.Format)
	{
		throw std::invalid_argument(
			"Only matching RGBA8 texture SRVs are supported");
	}

	if (!EnumHasAnyFlags(TextureDesc.Flags, ETextureCreateFlags::ShaderResource))
	{
		throw std::invalid_argument("Texture was not created for shader resource access");
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC D3DDesc{};


	D3DDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	D3DDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	D3DDesc.Shader4ComponentMapping =
		D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	D3DDesc.Texture2D.MostDetailedMip = SRVDesc.MipRange.First;
	D3DDesc.Texture2D.MipLevels = SRVDesc.MipRange.Num;
	D3DDesc.Texture2D.PlaneSlice = 0;
	D3DDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	auto* D3DTexture = static_cast<FD3D12Texture*>(Texture);
	/* 两个 CreateView 的职责要区分开：
	 *
	 * FD3D12ShaderResourceView_RHI::CreateView()
	 * 读取 FRHIViewDesc
	 * 检查纹理、格式、mip 范围
	 * 转换成 D3D12_SHADER_RESOURCE_VIEW_DESC
	 *     ↓
	 * FD3D12ShaderResourceView::CreateView(Resource, D3DDesc)
	 * 分配描述符槽位
	 * 调用 ID3D12Device::CreateShaderResourceView
	 * 保存 CPU / GPU Handle
	 */
	FD3D12ShaderResourceView::CreateView(D3DTexture->GetResource(), D3DDesc);

}

FD3D12DepthStencilView::FD3D12DepthStencilView(FD3D12Device* InDevice, FD3D12DescriptorHeap* InHeap)
    : TD3D12View<FD3D12DepthStencilView, D3D12_DEPTH_STENCIL_VIEW_DESC>(InDevice, InHeap)
{
    const auto HeapDesc = Heap->GetHeap()->GetDesc();
    if (HeapDesc.Type != D3D12_DESCRIPTOR_HEAP_TYPE_DSV || HeapDesc.Flags != D3D12_DESCRIPTOR_HEAP_FLAG_NONE)
    {
        throw std::invalid_argument("DSV requires a non-shader-visible DSV heap");
    }
}

void FD3D12DepthStencilView::CreateView(FD3D12Resource* InResource, const D3D12_DEPTH_STENCIL_VIEW_DESC& InDesc)
{
    if (bInitialized)
    {
        throw std::logic_error("View has already been initialized");
    }
    if (!InResource || !InResource->GetResource() || InResource->GetParentDevice() != ParentDevice)
    {
        throw std::invalid_argument("DSV requires a resource on this device");
    }
    const auto ResourceDesc = InResource->GetResource()->GetDesc();
    if (ResourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        ResourceDesc.Format != DXGI_FORMAT_D32_FLOAT || ResourceDesc.DepthOrArraySize != 1 ||
        ResourceDesc.MipLevels != 1 || ResourceDesc.SampleDesc.Count != 1 ||
        (ResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) == 0)
    {
        throw std::invalid_argument("Unsupported depth resource");
    }
    if (InDesc.Format != DXGI_FORMAT_D32_FLOAT || InDesc.ViewDimension != D3D12_DSV_DIMENSION_TEXTURE2D ||
        InDesc.Flags != D3D12_DSV_FLAG_NONE || InDesc.Texture2D.MipSlice != 0)
    {
        throw std::invalid_argument("Only writable D32 Texture2D mip-0 DSVs are supported");
    }
    // 校验完成后分配 CPU 描述符；不获取 GPU Handle。
    const uint32 Slot = Heap->Allocate();
    DescriptorSlot = Slot;
    Desc = InDesc;
    CPUHandle = Heap->GetCPUHandle(Slot);
    ParentDevice->GetDevice()->CreateDepthStencilView(InResource->GetResource(), &Desc, CPUHandle);
    bInitialized = true;
}
FD3D12RenderTargetView::FD3D12RenderTargetView(FD3D12Device* InDevice, FD3D12DescriptorHeap* InHeap)
    : TD3D12View<FD3D12RenderTargetView, D3D12_RENDER_TARGET_VIEW_DESC>(InDevice, InHeap)
{
    const auto HeapDesc = Heap->GetHeap()->GetDesc();
    if (HeapDesc.Type != D3D12_DESCRIPTOR_HEAP_TYPE_RTV ||
        (HeapDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) != 0)
    {
        throw std::invalid_argument("RTV requires a non-shader-visible RTV heap");
    }
}

void FD3D12RenderTargetView::CreateView(FD3D12Resource* InResource, const D3D12_RENDER_TARGET_VIEW_DESC& InDesc)
{
    if (bInitialized)
    {
        throw std::logic_error("View has already been initialized");
    }
    if (!InResource || !InResource->GetResource())
    {
        throw std::invalid_argument("RTV resource must not be null");
    }
    if (InResource->GetParentDevice() != ParentDevice)
    {
        throw std::invalid_argument("RTV and resource must belong to the same device");
    }

    // 这是当前教学实现的范围，D3D12 本身也支持其他格式、数组和多采样 RTV。
    const D3D12_RESOURCE_DESC& ResourceDesc = InResource->GetDesc();
    if (ResourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        ResourceDesc.Format != DXGI_FORMAT_R8G8B8A8_UNORM ||
        ResourceDesc.DepthOrArraySize != 1 || ResourceDesc.MipLevels != 1 ||
        ResourceDesc.SampleDesc.Count != 1 ||
        (ResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) == 0)
    {
        throw std::invalid_argument(
            "RTV requires a single-mip, single-sample RGBA8 render target");
    }
    if (InDesc.Format != DXGI_FORMAT_R8G8B8A8_UNORM ||
        InDesc.ViewDimension != D3D12_RTV_DIMENSION_TEXTURE2D)
    {
        throw std::invalid_argument("Only RGBA8 Texture2D RTVs are supported");
    }
    if (InDesc.Texture2D.MipSlice != 0 || InDesc.Texture2D.PlaneSlice != 0)
    {
        throw std::invalid_argument("Current RTV supports only mip 0 and plane 0");
    }

    // 所有检查完成后才消耗槽位；View 析构时归还，GPU 使用者须保活到 Fence 完成。
    const uint32 Slot = Heap->Allocate();
    DescriptorSlot = Slot;
    Desc = InDesc;
    CPUHandle = Heap->GetCPUHandle(Slot);

    // OMSetRenderTargets 使用 CPU 描述符句柄，RTV 不需要 GPU Handle。
    ParentDevice->GetDevice()->CreateRenderTargetView(
        InResource->GetResource(), &Desc, CPUHandle);
    bInitialized = true;
}

FD3D12View::~FD3D12View()
{
    // 当前显式 DeferredDelete 协议保证在飞 SRV 不会提前析构。
    // RTV/DSV 的纹理也必须存活至 GPU 完成；堆由 Device 持有。
    if (bInitialized) Heap->Free(DescriptorSlot);
}
