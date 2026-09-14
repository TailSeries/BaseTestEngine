# RHI 与 D3D12RHI UML 类图

> 依据 `Source/RHI/Public`、`Source/D3D12RHI/Public` 及 `Source/RHITest/main.cpp` 的当前实现整理。
>
> 图例：`<|--` 为继承；`*--` 为独占所有权（析构时一并释放）；`o--` 为聚合；`-->` 为非拥有的使用/回指；`..>` 为创建或参数依赖。

## 资源抽象与 D3D12 实现

```mermaid
classDiagram
direction LR

namespace RHI {
    class FRHIResource {
        <<abstract>>
        - ERHIResourceType ResourceType
        + GetType() ERHIResourceType
    }
    class FRHIBuffer {
        - FRHIBufferDesc Desc
        + GetDesc() FRHIBufferDesc
        + GetSize() uint32
        + GetStride() uint32
        + GetUsage() EBufferUsageFlags
    }
    class FRHITexture {
        - FRHITextureDesc Desc
        + GetDesc() FRHITextureDesc
    }
    class FRHIBufferDesc {
        + uint32 Size
        + uint32 Stride
        + EBufferUsageFlags Usage
    }
    class FRHITextureDesc {
        + uint32 Width
        + uint32 Height
        + EPixelFormat Format
    }
    class ERHIResourceType {
        <<enumeration>>
        RRT_Buffer
        RRT_Texture
        RRT_VertexShader
        RRT_PixelShader
        RRT_GraphicsPipelineState
    }
    class EBufferUsageFlags {
        <<enumeration>>
        Static
        Dynamic
        VertexBuffer
        IndexBuffer
        ConstantBuffer
    }
    class EPixelFormat {
        <<enumeration>>
        PF_R8G8B8A8_UNORM
        PF_D32_FLOAT
    }
}

namespace D3D12RHI {
    class FD3D12Buffer {
        - FD3D12Device* Parent
        - unique_ptr~FD3D12Resource~ ResourcePtr
        - void* MappedData
        + GetResource() FD3D12Resource*
        + GetMappedData() void*
    }
    class FD3D12Texture {
        - FD3D12Device* Parent
        - unique_ptr~FD3D12Resource~ ResourcePtr
        - uint32 SRVSlot
        + GetResource() FD3D12Resource*
        + SetSRVSlot(uint32)
    }
    class FD3D12Resource {
        - FD3D12Device* Parent
        - ComPtr~ID3D12Resource~ Resource
        - D3D12_RESOURCE_STATES State
        - D3D12_RESOURCE_DESC Desc
        - D3D12_HEAP_TYPE HeapType
        - D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddress
        + GetResource() ID3D12Resource*
        + GetGPUVirtualAddress() D3D12_GPU_VIRTUAL_ADDRESS
    }
    class ID3D12Resource {
        <<D3D12 COM interface>>
    }
}

FRHIResource <|-- FRHIBuffer
FRHIResource <|-- FRHITexture
FRHIBuffer *-- FRHIBufferDesc : stores by value
FRHITexture *-- FRHITextureDesc : stores by value
FRHIResource --> ERHIResourceType : type
FRHIBufferDesc --> EBufferUsageFlags : usage
FRHITextureDesc --> EPixelFormat : format

FRHIBuffer <|-- FD3D12Buffer
FRHITexture <|-- FD3D12Texture
FD3D12Buffer *-- "1" FD3D12Resource : ResourcePtr
FD3D12Texture *-- "1" FD3D12Resource : ResourcePtr
FD3D12Resource *-- "1" ID3D12Resource : ComPtr
```

## D3D12 设备、提交与呈现

```mermaid
classDiagram
direction TB

namespace D3D12RHI {
    class FD3D12AdapterDesc {
        + DXGI_ADAPTER_DESC Desc
        + int32 AdapterIndex
        + D3D_FEATURE_LEVEL MaxSupportedFeature
        + IsValid() bool
    }
    class FD3D12Adapter {
        - FD3D12AdapterDesc Desc
        - ComPtr~IDXGIFactory4~ DxgiFactory
        - ComPtr~IDXGIAdapter~ DxgiAdapter
        - ComPtr~ID3D12Device~ RootDevice
        - FD3D12Device* Device
        + FindAdapter(out Desc) bool
        + InitializeDevices()
        + GetDevice() FD3D12Device*
        + GetD3DDevice() ID3D12Device*
    }
    class FD3D12Device {
        - FD3D12Adapter* Adapter
        - uint32 GPUIndex
        - vector~unique_ptr~FD3D12Queue~~ Queues
        + GetDevice() ID3D12Device*
        + GetQueue(Type) FD3D12Queue&
        + CreateBuffer(Desc, Data) FD3D12Buffer
        + CreateTexture(Desc, Data) FD3D12Texture
        + CreateDepthBuffer(Width, Height) FD3D12Resource
    }
    class ED3D12QueueType {
        <<enumeration>>
        Direct
        Copy
        Async
    }
    class FD3D12Queue {
        + FD3D12Device* Device
        + ED3D12QueueType Type
        + ComPtr~ID3D12CommandQueue~ D3DCommandQueue
        + FD3D12Fence Fence
        + Signal(Fence) uint64
        + Wait(Fence, Value)
        + WaitCPU(Value)
    }
    class FD3D12Fence {
        + FD3D12Queue* OwnerQueue
        + ComPtr~ID3D12Fence~ D3DFence
        + uint64 NextCompletionValue
        + HANDLE FenceEvent
    }
    class FD3D12CommandAllocator {
        - ComPtr~ID3D12CommandAllocator~ Allocator
        + Reset()
    }
    class FD3D12CommandList {
        - ComPtr~ID3D12GraphicsCommandList~ CommandList
        + Reset(Allocator)
        + Close()
    }
    class FD3D12DescriptorHeap {
        - FD3D12Device* Parent
        - ComPtr~ID3D12DescriptorHeap~ Heap
        - uint32 NumDescriptors
        - uint32 NextFreeSlot
        + GetCPUHandle(slot) Handle
        + GetGPUHandle(slot) Handle
        + Allocate() uint32
    }
    class FD3D12Viewport {
        - FD3D12Adapter* Adapter
        - ComPtr~IDXGISwapChain3~ SwapChain
        - vector~ComPtr~ID3D12Resource~~ BackBuffers
        - unique_ptr~FD3D12DescriptorHeap~ RTVHeap
        + Init()
        + Resize(Width, Height)
        + PresentInternal(SyncInterval)
        + GetBackBuffer() ID3D12Resource*
        + GetCurrentBackBufferRTV() Handle
    }
    class FD3D12RootSignature {
        - ComPtr~ID3D12RootSignature~ RootSignature
    }
    class FD3D12PipelineState {
        - ComPtr~ID3D12PipelineState~ PSO
    }
    class CompileShader {
        <<function>>
        + CompileShader(Source, EntryPoint, Target) ComPtr~ID3DBlob~
    }
}

FD3D12Adapter *-- "1" FD3D12AdapterDesc : Desc
FD3D12Adapter *-- "0..1" FD3D12Device : new/delete
FD3D12Device --> "1" FD3D12Adapter : non-owning parent
FD3D12Device *-- "3" FD3D12Queue : Direct / Copy / Async
FD3D12Queue *-- "1" FD3D12Fence
FD3D12Fence --> "1" FD3D12Queue : OwnerQueue
FD3D12Queue --> ED3D12QueueType : Type

FD3D12CommandAllocator ..> FD3D12Device : creates native allocator
FD3D12CommandAllocator --> ED3D12QueueType : command-list type
FD3D12CommandList ..> FD3D12Device : creates native command list
FD3D12CommandList ..> FD3D12CommandAllocator : Reset / constructor
FD3D12CommandList --> ED3D12QueueType : command-list type

FD3D12DescriptorHeap --> FD3D12Device : non-owning parent
FD3D12Viewport --> FD3D12Adapter : non-owning parent
FD3D12Viewport *-- "1" FD3D12DescriptorHeap : RTVHeap
FD3D12Viewport *-- "2" ID3D12Resource : swap-chain back buffers
FD3D12RootSignature ..> FD3D12Device : creation
FD3D12PipelineState ..> FD3D12Device : creation
FD3D12Device ..> FD3D12Buffer : creates
FD3D12Device ..> FD3D12Texture : creates
FD3D12Device ..> FD3D12Resource : creates depth resource
```

## 当前调用入口

`RHITestPeriod1` 是示例层而不是 RHI 模块的一部分。它拥有 `FD3D12Adapter`、`FD3D12Viewport`、根签名、PSO、命令分配器/列表、深度资源和描述符堆；`Device` 与 Direct `Queue` 只是从 `Adapter` 取得的非拥有指针。

```mermaid
classDiagram
direction LR

class RHITestPeriod1 {
    - unique_ptr~FD3D12Adapter~ Adapter
    - unique_ptr~FD3D12Viewport~ Viewport
    - FD3D12Device* Device
    - FD3D12Queue* Queue
    - shared_ptr~FD3D12Buffer~ VB / CB / IB
    + InitializedD3D12Device(HWND)
    + DrawTriangle()
}

RHITestPeriod1 *-- FD3D12Adapter
RHITestPeriod1 *-- FD3D12Viewport
RHITestPeriod1 --> FD3D12Device : borrowed
RHITestPeriod1 --> FD3D12Queue : borrowed Direct queue
RHITestPeriod1 *-- FD3D12CommandAllocator
RHITestPeriod1 *-- FD3D12CommandList
RHITestPeriod1 *-- FD3D12RootSignature
RHITestPeriod1 *-- FD3D12PipelineState
RHITestPeriod1 *-- FD3D12DescriptorHeap : DSV heap
RHITestPeriod1 *-- FD3D12Resource : depth buffer
RHITestPeriod1 o-- FD3D12Buffer : VB / CB / IB
FD3D12Adapter --> FD3D12Device
FD3D12Device --> FD3D12Queue
```

`TRefCountPtr` 在当前代码中等价于 `std::shared_ptr`（在项目的基础类型定义中别名）；上图因此将缓冲区表示为共享拥有关系。`FD3D12Resource` 的资源状态目前只在类内保存，尚未公开状态转换接口，示例程序中的 back-buffer barrier 仍直接操作 D3D12 原生资源。
