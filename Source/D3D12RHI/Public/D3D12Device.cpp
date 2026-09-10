#include "D3D12Device.h"
#include "D3D12Adapter.h"
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
