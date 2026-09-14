#include "D3D12Device.h"

#include "BaseDefines.h"
#include "D3D12Adapter.h"
#include "D3D12CommandList.h"
FD3D12Device::FD3D12Device(FD3D12Adapter* InAdapter, uint32 InGPUIndex)
    : Adapter(InAdapter)
    , GPUIndex(InGPUIndex)
{
	// 直接建立三条队列，（Direct copy Async），这时候Adapter的RootDevice必须已经创建了出来
    Queues.reserve(static_cast<uint32>(ED3D12QueueType::Count));
    for (uint32 i = 0; i < static_cast<uint32>(ED3D12QueueType::Count); i++)
    {
        Queues.push_back(std::make_unique<FD3D12Queue>(this, static_cast<ED3D12QueueType>(i)));
    }
}
FD3D12Device::~FD3D12Device() = default;


ID3D12Device* FD3D12Device::GetDevice()
{
    return Adapter->GetD3DDevice();
}

/*
 * - committed resource:一次调用同时建"资源 + 它独占的一块隐式堆"。对应还有 placed resource(资源放进你预先分配的大堆里,可子分配——就是 UE FD3D12ResourceLocation 干的,我们跳过)。
 * - UPLOAD vs DEFAULT 堆:UPLOAD = CPU 可写、GPU 可读的共享内存,用 Map+memcpy 直接传数据,适合常量缓冲/动态数据;DEFAULT = GPU 本地显存,访问快但 CPU 碰不到,要传数据得经 staging + copy(第6章)。
 * - UPLOAD 堆必须 GENERIC_READ:这是 D3D12 硬性规定,建 UPLOAD 资源初始状态只能是它。
 * - buffer 资源描述套路:Width 放字节数,Height/Depth/MipLevels=1,Format=UNKNOWN,Layout=ROW_MAJOR——buffer 全这么填,和 texture 完全不同。
 * - Map 的 D3D12_RANGE{0,0}:告诉驱动"我不读现有内容"(纯写),避免不必要的缓存同步开销。Unmap 第二参 nullptr 表示"整个范围都可能被写过"
 * ① 256 对齐:常量缓冲的 size 必须是 256 倍数(之前讲的硬件要求)。
 */
TRefCountPtr<FD3D12Buffer> FD3D12Device::CreateBuffer(const FRHIBufferDesc& Desc, const void* InitialData)
{
    ID3D12Device* D3DDevice = GetDevice();

    uint32 AllocSize = Desc.Size;
    if (EnumHasAnyFlags(Desc.Usage, EBufferUsageFlags::ConstantBuffer))
    {
	    // ① 256 对齐:常量缓冲的 size 必须是 256 倍数(之前讲的硬件要求)。
        AllocSize = (AllocSize + 255) & ~255u;
    }


    //  1. 堆类型：Dynamic → UPLOAD（CPU 可写，Map），否则 DEFAULT（GPU 本地显存）
    const bool bDynamic = EnumHasAnyFlags(Desc.Usage, EBufferUsageFlags::Dynamic);
    const D3D12_HEAP_TYPE HeapType = bDynamic ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;

    // 2. 初始状态：UPLOAD 堆强制 GENERIC_READ(因为 Upload Heap 本质上是：GPU可读，CPU可写)；DEFAULT 用 COMMON
    const D3D12_RESOURCE_STATES InitalState = bDynamic ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON;

    //3. 堆属性
    D3D12_HEAP_PROPERTIES HeapProps = {};
    HeapProps.Type = HeapType;
    HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;//指定 CPU 页属性,基本不用管，交给驱动决定
    HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;//UMA（核显）和 NUMA（独显）才有区别。交给驱动决定
    HeapProps.CreationNodeMask = 1;//在哪个 GPU 上创建资源,单卡直接1
    HeapProps.VisibleNodeMask = 1;//那些GPU可以看见这个资源，单卡直接1

    // buffer的资源描述
    D3D12_RESOURCE_DESC ResDesc;
    ResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    ResDesc.Alignment = 0;//资源对齐要求。0 表示使用默认对齐，buffer默认是64kb
    ResDesc.Width = AllocSize; // 对于buffer width = size,对于texture width = 像素宽度
    ResDesc.Height = 1;// buffer 没有固定高度，直接来个1
    ResDesc.DepthOrArraySize = 1;//buffer 没有深度
    ResDesc.MipLevels = 1;// buffer 没有mipmap
    ResDesc.Format = DXGI_FORMAT_UNKNOWN; //Buffer 本身没有格式, 格式属于 View, 而不是资源本体
    ResDesc.SampleDesc.Count = 1;// buffer 不支持msaa
    ResDesc.SampleDesc.Quality = 0;
    ResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;// 资源布局，buffer本就是一段字节流必须是 D3D12_TEXTURE_LAYOUT_ROW_MAJOR。不会把它跟贴图一样进行Tile划分
    ResDesc.Flags = D3D12_RESOURCE_FLAG_NONE; // UAV？目前标志就是普通buffer，几乎对于普通 Vertex/Index/Structured Buffer

    // 5. 建立committed resource
    ComPtr<ID3D12Resource> D3DResource;
    // 通常直接用 D3D12_HEAP_FLAG_NONE ，表示这个buffer没有特殊作用
    VERIFY_D3D12(D3DDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &ResDesc, InitalState, nullptr, IID_PPV_ARGS(&D3DResource)));


    // 6. 有初始数据 + UPLOAD：Map + memcpy（不需要 command list）
    if (InitialData && bDynamic)
    {
        void* Mapped = nullptr;
        D3D12_RANGE ReadRange = {0, 0}; //  pReadRange(输入):只是个提示,告诉驱动"我打算读哪些字节"。它只影响读,不影响写,也不影响映射范围。
        VERIFY_D3D12(D3DResource->Map(0, &ReadRange, &Mapped));
        // Mapped是一份指向upload堆的cpu空间下的虚拟地址，但请注意Upload堆实际位于L0（系统ram上）
        memcpy(Mapped, InitialData, Desc.Size);
        D3DResource->Unmap(0, nullptr);// 只是取消映射，并不会导致资源被销毁
    }
    // 注：DEFAULT 堆 + 初始数据 需 staging + copy command list（第6章），本章先只支持 UPLOAD 上传

    // 7. 包装：FD3D12Resource + FD3D12Buffer
    std::unique_ptr<FD3D12Resource> Res = std::make_unique<FD3D12Resource>(this, D3DResource.Get(), InitalState, ResDesc, HeapType);
    TRefCountPtr<FD3D12Buffer> Buffer = std::make_unique<FD3D12Buffer>(this, Desc);
    Buffer->SetResource(std::move(Res));
    return Buffer;
}

std::unique_ptr<FD3D12Resource> FD3D12Device::CreateDepthBuffer(uint32 Width, uint32 Height)
{
    ID3D12Device* D3DDevice = GetDevice();

    //堆Default，深度缓冲位于VRAM，cpu不需要写
    D3D12_HEAP_PROPERTIES HeapProps = {};
    HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    HeapProps.CreationNodeMask = 1;
    HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;//UMA（核显）和 NUMA（独显）才有区别。交给驱动决定


    D3D12_RESOURCE_DESC Desc = {};
    Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.DepthOrArraySize = 1;
    Desc.MipLevels = 1; // 深度缓冲没有mip，强行指定超过1会发生崩溃
    Desc.Format = DXGI_FORMAT_D32_FLOAT;
    Desc.SampleDesc.Count = 1; // 决定这个 Depth Buffer 是普通深度缓冲还是 MSAA 深度缓冲。
    Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;         // ← 纹理让驱动定布局（buffer 是 ROW_MAJOR）
    Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;// // ← 允许当深度目标

    // 优化清除值：深度/RT 资源建议提供，且必须和 ClearDepthStencilView 用的值一致
    D3D12_CLEAR_VALUE ClearValue = {};
    ClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    ClearValue.DepthStencil.Depth = 1.0f;
    ClearValue.DepthStencil.Stencil = 0;

    ComPtr<ID3D12Resource> D3DResource;
    VERIFY_D3D12(D3DDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &ClearValue, IID_PPV_ARGS(&D3DResource)));
    return std::make_unique<FD3D12Resource>(this, D3DResource.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, Desc, D3D12_HEAP_TYPE_DEFAULT);
}

TRefCountPtr<FD3D12Texture> FD3D12Device::CreateTexture(const FRHITextureDesc& InDesc, const void* InitialData)
{
    ID3D12Device* D3DDevice = GetDevice();
    D3D12_HEAP_PROPERTIES HeapProps = {};
    HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    HeapProps.CreationNodeMask = 1;//在哪个 GPU 上创建资源,单卡直接1
    HeapProps.VisibleNodeMask = 1;//那些GPU可以看见这个资源，单卡直接1

    D3D12_RESOURCE_DESC Desc = {};
    Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    Desc.Width = InDesc.Width;
    Desc.Height = InDesc.Height;
    Desc.DepthOrArraySize = 1;
    Desc.MipLevels = 1;
    Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    Desc.SampleDesc.Count = 1;
    Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;// 纹理让驱动来决定布局
    Desc.Flags = D3D12_RESOURCE_FLAG_NONE;//// 只采样，不当 RT/DS

    // 1.先建出Default堆上的资源
    ComPtr<ID3D12Resource> D3DResource;
    // 初始状态 COPY_DEST：等着被 staging copy 进来（Step2 上传后转 PIXEL_SHADER_RESOURCE）
    VERIFY_D3D12(D3DDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&D3DResource)));
    auto Res = std::make_unique<FD3D12Resource>(this, D3DResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, Desc, D3D12_HEAP_TYPE_DEFAULT);
	
    TRefCountPtr<FD3D12Texture> Texture = std::make_unique<FD3D12Texture>(this, InDesc);
    Texture->SetResource(std::move(Res));

    // 2. 填充InitialData，注意：RowPitch 已 256 对齐，因此拷贝字节必须一行行拷贝,而不能简单把InitialData塞里面
    if (InitialData)
    {
        // 1. 问驱动 subresource0 的 footprint（RowPitch 已 256 对齐）: 这个结构是在 CPU 侧的线性缓冲区(Buffer)和 GPU 侧的纹理(Texture)之间做数据拷贝时,描述内存布局的关键结构
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint = {};
        uint32 NumRows = 0;
        uint64 RowSizeInBytes = 0;
        uint64 TotalBytes = 0;
        /*NumRows —— 真实行数(无对齐概念)
         *RowSizeInBytes —— 每一行的真实大小(不含 padding)
         *Footprint.Footprint.RowPitch —— 对齐后大小
         *TotalBytes —— 对齐后的总大小 ≠ RowPitch × NumRows 最后一行通常不补 padding,所以一般是: TotalBytes = RowPitch × (NumRows - 1) + RowSizeInBytes
         */
        D3DDevice->GetCopyableFootprints(&Desc, 0, 1, 0, &Footprint, &NumRows, &RowSizeInBytes, &TotalBytes);

        //2 upload staging buffers,这一块必须按footprint对齐padding
        D3D12_HEAP_PROPERTIES UpProps;
        UpProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        UpProps.CreationNodeMask = 1;
        UpProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC BufDesc = {};
        BufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        BufDesc.Width = TotalBytes;
        BufDesc.Height = 1;
        BufDesc.DepthOrArraySize = 1;
        BufDesc.MipLevels = 1;
        BufDesc.Format = DXGI_FORMAT_UNKNOWN;
        BufDesc.SampleDesc.Count = 1;
        BufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> Staging;
        VERIFY_D3D12(D3DDevice->CreateCommittedResource(&UpProps, D3D12_HEAP_FLAG_NONE, &BufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&Staging)));

        //3. 逐行copy：dst 每行跳 RowPitch(对齐)，src 每行 SrcRowPitch(紧密)，最后一行只有 RowSizeInBytes
        uint8* Mapped = nullptr;
        D3D12_RANGE Rd = {0, 0};
        VERIFY_D3D12(Staging->Map(0, &Rd, reinterpret_cast<void**>(&Mapped)));




    }



    return Texture;
}
