# RHI 与 D3D12RHI UML 类图

> 依据 `Source/RHI/Public`、`Source/D3D12RHI/Public` 及 `Source/RHITest/main.cpp` 的当前实现整理。
>
> 图例：`<|--` 为继承；`*--` 为独占所有权（析构时一并释放）；`o--` 为聚合/共享持有；`-->` 为非拥有的使用/回指；`..>` 为创建或参数依赖。

## 一、资源抽象与 D3D12 实现

RHI 层只暴露基类型（无任何 D3D12 类型）；D3D12 层继承并持有原生对象。`FRHIGraphicsPipelineState` 是为让命令列表能以 RHI 类型接收 PSO 而加的薄基类，创建仍在 D3D12 层具体做。

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
    class FRHIGraphicsPipelineState {
        <<空基类，创建在 D3D12 层>>
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
        RRT_GraphicsPipelineState
    }
    class EBufferUsageFlags {
        <<enumeration>>
        Static / Dynamic
        VertexBuffer / IndexBuffer / ConstantBuffer
    }
    class EPixelFormat {
        <<enumeration>>
        PF_R8G8B8A8_UNORM
        PF_D32_FLOAT
    }
}

namespace D3D12RHI_Res {
    class FD3D12Buffer {
        - FD3D12Device* Parent
        - unique_ptr~FD3D12Resource~ ResourcePtr
        + GetResource() FD3D12Resource*
        + GetMappedData() void*
    }
    class FD3D12Texture {
        - FD3D12Device* Parent
        - unique_ptr~FD3D12Resource~ ResourcePtr
        - uint32 SRVSlot
        + GetResource() FD3D12Resource*
        + GetSRVSlot() uint32
    }
    class FD3D12PipelineState {
        - ComPtr~ID3D12PipelineState~ PSO
        - FD3D12RootSignature* RootSig
        + GetPipelineState() ID3D12PipelineState*
        + GetRootSignature() FD3D12RootSignature*
    }
    class FD3D12Resource {
        - FD3D12Device* Parent
        - ComPtr~ID3D12Resource~ Resource
        - D3D12_RESOURCE_STATES State
        - D3D12_HEAP_TYPE HeapType
        - D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddress
        + GetResource() ID3D12Resource*
        + GetGPUVirtualAddress() ...
    }
    class ID3D12Resource {
        <<D3D12 COM interface>>
    }
}

FRHIResource <|-- FRHIBuffer
FRHIResource <|-- FRHITexture
FRHIResource <|-- FRHIGraphicsPipelineState
FRHIBuffer *-- FRHIBufferDesc : by value
FRHITexture *-- FRHITextureDesc : by value
FRHIResource --> ERHIResourceType : type
FRHIBufferDesc --> EBufferUsageFlags : usage
FRHITextureDesc --> EPixelFormat : format

FRHIBuffer <|-- FD3D12Buffer
FRHITexture <|-- FD3D12Texture
FRHIGraphicsPipelineState <|-- FD3D12PipelineState
FD3D12Buffer *-- "1" FD3D12Resource : ResourcePtr
FD3D12Texture *-- "1" FD3D12Resource : ResourcePtr
FD3D12PipelineState --> FD3D12RootSignature : non-owning（UE：PSO 打包 rootsig）
FD3D12Resource *-- "1" ID3D12Resource : ComPtr
```

## 二、RHI 抽象层：DynamicRHI + CommandList（接口/实现分离）

上层只认 `FDynamicRHI`（资源创建）与 `FRHICommandList`（命令录制），不知道背后是 D3D12。`FD3D12DynamicRHI` 持有 `FD3D12Adapter`，是三层设备的拥有者；`FD3D12CommandContext` 持有帧资源（N 分配器 + CmdList + CB ring + 每帧 fence）、延迟删除队列，并借用 Viewport/DSV/SRV 堆。两层为将来多线程（命令缓存 + RHI 线程重放）留缝。

```mermaid
classDiagram
direction TB

namespace RHI_Abstract {
    class FDynamicRHI {
        <<abstract>>
        + Init()
        + Shutdown()
        + GetName() char*
        + RHICreateBuffer(Desc, Data) TRefCountPtr~FRHIBuffer~
        + RHICreateTexture(Desc, Data) TRefCountPtr~FRHITexture~
    }
    class GDynamicRHI {
        <<global FDynamicRHI ptr>>
        + RHICreateBuffer/Texture 自由函数转发到它
    }
    class IRHICommandContext {
        <<abstract>>
        + BeginFrame()
        + BeginRenderPass(ClearColor)
        + SetGraphicsPipelineState(FRHIGraphicsPipelineState*)
        + SetShaderConstants(RootParam, Data, Size)
        + SetTexture(RootParam, FRHITexture*)
        + SetStreamSource(Stream, FRHIBuffer*)
        + DrawIndexedPrimitive(FRHIBuffer* IB, Count)
        + EndRenderPass()
        + EndFrame()
        + WaitForGPU()
        + DeferredDelete(TRefCountPtr~FRHIResource~)
    }
    class FRHICommandList {
        - IRHICommandContext* Context
        + 逐方法转发（将来在此插 deferred 录制）
    }
}

namespace D3D12RHI_Impl {
    class FD3D12DynamicRHI {
        - unique_ptr~FD3D12Adapter~ Adapter
        - FD3D12Device* Device
        + Init()  // FindAdapter + InitializeDevices
        + GetAdapter() FD3D12Adapter*
        + GetDevice() FD3D12Device*
    }
    class FD3D12CommandContext {
        - FD3D12Device* Device
        - FD3D12Queue* Queue
        - FD3D12Viewport* Viewport
        - FD3D12DescriptorHeap* DSVHeap / SRVHeap
        - unique_ptr~FD3D12CommandAllocator~ CmdAllocs[2]
        - unique_ptr~FD3D12CommandList~ CmdList
        - TRefCountPtr~FD3D12Buffer~ CBs_ring[2]
        - uint64 FrameFenceValue[2]
        - uint32 FrameIndex / Slot
        - FD3D12DeferredDeletionQueue DeletionQueue
        + Init(Device, Queue, Viewport, DSVHeap, SRVHeap)
    }
    class FD3D12DeferredDeletionQueue {
        - vector~FEntry~ Pending
        + Enqueue(Resource, FenceValue)
        + ReleaseCompleted(CompletedValue)
    }
}

FDynamicRHI <|-- FD3D12DynamicRHI
IRHICommandContext <|-- FD3D12CommandContext
FRHICommandList --> IRHICommandContext : 转发（非拥有）
GDynamicRHI --> FDynamicRHI : 指向实现
FD3D12CommandContext *-- "1" FD3D12DeferredDeletionQueue
FD3D12DynamicRHI *-- "1" FD3D12Adapter : 拥有三层设备
FD3D12CommandContext ..> FD3D12CommandAllocator : 每帧一个
FD3D12CommandContext ..> FD3D12CommandList : 一条，每帧 Reset 到当帧 allocator
```

## 三、D3D12 设备、提交与呈现

```mermaid
classDiagram
direction TB

namespace D3D12RHI_Device {
    class FD3D12Adapter {
        - FD3D12AdapterDesc Desc
        - ComPtr~IDXGIFactory4~ DxgiFactory
        - ComPtr~ID3D12Device~ RootDevice
        - FD3D12Device* Device
        + FindAdapter(out Desc) bool
        + InitializeDevices()
        + GetDevice() FD3D12Device*
    }
    class FD3D12Device {
        - FD3D12Adapter* Adapter
        - vector~unique_ptr~FD3D12Queue~~ Queues
        + GetQueue(Type) FD3D12Queue&
        + CreateBuffer(Desc, Data) FD3D12Buffer
        + CreateTexture(Desc, Data) FD3D12Texture
        + CreateDepthBuffer(W, H) FD3D12Resource
        + CreateShaderResourceView(Tex, Heap)
    }
    class ED3D12QueueType {
        <<enumeration>>
        Direct / Copy / Async
    }
    class FD3D12Queue {
        + ComPtr~ID3D12CommandQueue~ D3DCommandQueue
        + FD3D12Fence Fence
        + Signal(Fence) uint64
        + Wait(Fence, Value)
        + WaitCPU(Value)
    }
    class FD3D12Fence {
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
        - ComPtr~ID3D12DescriptorHeap~ Heap
        - uint32 NumDescriptors / NextFreeSlot
        + GetCPUHandle(slot) / GetGPUHandle(slot)
        + Allocate() uint32
    }
    class FD3D12Viewport {
        - ComPtr~IDXGISwapChain3~ SwapChain
        - vector~ComPtr~ID3D12Resource~~ BackBuffers
        - unique_ptr~FD3D12DescriptorHeap~ RTVHeap
        + Init() / PresentInternal(SyncInterval)
        + GetBackBuffer() / GetCurrentBackBufferRTV()
    }
    class FD3D12RootSignature {
        - ComPtr~ID3D12RootSignature~ RootSignature
        + GetRootSignature() ID3D12RootSignature*
    }
}

FD3D12Adapter *-- "0..1" FD3D12Device : new/delete
FD3D12Device --> "1" FD3D12Adapter : non-owning parent
FD3D12Device *-- "3" FD3D12Queue : Direct / Copy / Async
FD3D12Queue *-- "1" FD3D12Fence
FD3D12Queue --> ED3D12QueueType : Type
FD3D12CommandAllocator ..> FD3D12Device : native allocator
FD3D12CommandList ..> FD3D12CommandAllocator : Reset / ctor
FD3D12DescriptorHeap --> FD3D12Device : non-owning parent
FD3D12Viewport --> FD3D12Adapter : non-owning parent
FD3D12Viewport *-- "1" FD3D12DescriptorHeap : RTVHeap
FD3D12RootSignature ..> FD3D12Device : creation
FD3D12Device ..> FD3D12Buffer : creates
FD3D12Device ..> FD3D12Texture : creates
FD3D12Device ..> FD3D12Resource : depth resource
```

## 四、当前调用入口

`RHITestPeriod1` 是示例层，不属于 RHI 模块。经过 A2/A2-b 后它的 draw loop **零裸 D3D12 调用**：资源经 `RHICreateBuffer/RHICreateTexture` 建立，命令经 `RHICmdList` 录制。`FD3D12DynamicRHI` 现在是三层设备的拥有者（Adapter 从示例层挪进它）；`FD3D12CommandContext` 拥有帧资源与延迟删除队列。示例层仍持有的具体 D3D12 对象（RootSig/PSO/SRV 堆/DSV 堆/深度资源/Viewport）属过渡态，将在 A2-b 片2/片3 逐步抽掉。

```mermaid
classDiagram
direction LR

class RHITestPeriod1 {
    - unique_ptr~FD3D12DynamicRHI~ RHI
    - unique_ptr~FD3D12Viewport~ Viewport
    - FD3D12Device* Device        // borrowed
    - FD3D12Queue* Queue          // borrowed Direct
    - TRefCountPtr~FRHIBuffer~ VB / IB
    - TRefCountPtr~FRHITexture~ Tex
    - unique_ptr~FD3D12RootSignature~ RootSig
    - unique_ptr~FD3D12PipelineState~ PSO
    - unique_ptr~FD3D12DescriptorHeap~ DSVHeap / SRVHeap
    - unique_ptr~FD3D12Resource~ DepthBuffer
    - unique_ptr~FD3D12CommandContext~ Context
    - unique_ptr~FRHICommandList~ RHICmdList
    + InitializedD3D12Device(HWND)
    + DrawTriangle()   // BeginFrame→BeginRenderPass→Set*→Draw→End*
    + MaybeSwapTexture()  // 空格换纹理，验证延迟删除
    + WaitForGPU()
}

RHITestPeriod1 *-- FD3D12DynamicRHI : 拥有（内含 Adapter/三层设备）
RHITestPeriod1 *-- FD3D12Viewport
RHITestPeriod1 *-- FD3D12CommandContext
RHITestPeriod1 *-- FRHICommandList
RHITestPeriod1 --> FD3D12Device : borrowed（RHI->GetDevice）
RHITestPeriod1 --> FD3D12Queue : borrowed Direct
RHITestPeriod1 *-- FD3D12RootSignature
RHITestPeriod1 *-- FD3D12PipelineState
RHITestPeriod1 *-- FD3D12DescriptorHeap : DSV / SRV heap
RHITestPeriod1 *-- FD3D12Resource : depth buffer
RHITestPeriod1 o-- FRHIBuffer : VB / IB（经 RHICreateBuffer）
RHITestPeriod1 o-- FRHITexture : Tex（经 RHICreateTexture）
FRHICommandList --> FD3D12CommandContext : 转发
```

## 五、几点说明（当前简化 / 欠账）

- `TRefCountPtr` 当前等价于 `std::shared_ptr`（基础类型别名），故缓冲/纹理为共享持有；延迟删除队列靠"多持一份 shared_ptr 拷贝"保活到 GPU 越过 fence（UE 用侵入式引用计数 + `MarkForDelete`）。
- **多帧同步（M4-a）**：`FD3D12CommandContext` 用 N=2 的分配器/CB ring + 每帧 fence，提交后只记 `FrameFenceValue[Slot]` 不等，复用前 `WaitCPU`；退出走 `WaitForGPU`。
- **延迟释放（M4-b）**：`DeferredDelete` 入队用"当前帧将 signal 的 fence 值"（保守上界）；`BeginFrame` 每帧 drain 已完成的。资源级 `LastUsedFrameFence` 精确追踪未做。
- **barrier 仍手写**（在 `BeginRenderPass/EndRenderPass` 内），状态自动追踪属阶段 B（RHICore），未做。
- **SRV 绑定 / 描述符槽回收**：SRV 靠 `Tex->GetSRVSlot()`；`FD3D12DescriptorHeap::Allocate` 线性只增、不回收（换纹理会泄漏槽）。SRV 创建/绑定的抽象与槽回收留 A2-b 片3 / 描述符管理。
- **仍具体（过渡）**：PSO/RootSignature/RenderPass 的**创建**、`SetShaderConstants`（root CBV + CB ring，UE 用 UniformBuffer）。留 A2-b 片2。
