# UE RHI 封装 DX12 学习笔记

> **当前状态（2026-09-27）：阶段 A 已按既定教学范围实现并验收。下一阶段是 B：资源状态与 Barrier。**
> Debug / Release 均编译、运行通过：GPU gate 保护真实 draw 的在飞资源，描述符回收、24 次 Resize、24 次 DSV 重建、120 次换纹理、Clear/Load、非零顶点 Offset、图像回读及退出验证均通过，D3D12 调试队列无警告/错误。
> 具体改动、UE 对照、运行命令与保留的简化见 [StageA_Completion.md](StageA_Completion.md)。下方按日期保留的教学记录描述当时状态，不能覆盖此处当前结论。

## 教学约定
每章节节奏：**先讲 UE 源码 → 再对照实现自己的简化版**

## 核心原则：贴合 UE 骨架，不过度合并
复刻要**跟 UE 的类分层/命名/文件结构保持一致**，即使单 GPU、单线程也**不把多个 UE 类合并成一个**。
- 为"单 GPU"把 Adapter/Device/Queue 三层压成一个类：短期省事，但项目一大、命名结构对不上 UE，反而难管理、难对照，背离"对照 UE 学习"的初衷 → **已否决**。
- 简化**只针对内部细节**：接口多版本数组(Device1..12/Factory2..7 只留基础版)、多GPU数组、多线程提交管线(Payload/对象池/Timing)、间接绘制签名等。**类的骨架、层级、命名跟 UE 走。**

---

## 目录结构规划

### 新建两个模块（不动现有 DirectX12/）

```
Source/RHI/                        ← 对应 UE: Runtime/RHI/
  Public/
    RHI.h                          ← 伞形头
    RHIDefinitions.h               ← 枚举（EPixelFormat 等）
    RHIResources.h                 ← FRHIResource 基类, Buffer, Texture, Shader
    RHICommandList.h               ← FRHICommandList（单线程简化）
    DynamicRHI.h                   ← FDynamicRHI 抽象接口
  Private/
    RHI.cpp
  CMakeLists.txt

Source/D3D12RHI/                   ← 对应 UE: Runtime/D3D12RHI/
  Public/                          ← 本项目实际把文件都放在 Public/
    D3D12RHIPrivate.h              ← 总 include（Windows/d3d12/ComPtr/VERIFY_D3D12）
    D3D12Adapter.h / .cpp          ← 对应 UE D3D12Adapter（物理GPU+工厂+Device容器）
    D3D12Device.h / .cpp           ← 对应 UE D3D12Device（GPU节点，持有 Queues）
    D3D12Queue.h / .cpp            ← 对应 UE D3D12Queue.h（ED3D12QueueType 枚举 + FD3D12Queue + FD3D12Fence）
    D3D12Viewport.h / .cpp         ← 对应 UE D3D12Viewport（SwapChain）
    D3D12Resources.h / .cpp        ← 对应 UE D3D12Resources（资源基类）
    D3D12Descriptors.h / .cpp      ← 对应 UE D3D12Descriptors（Heap分配）
    D3D12PipelineState.h / .cpp    ← 对应 UE D3D12PipelineState + RootSignature
    D3D12Commands.cpp              ← 对应 UE D3D12Commands（CommandList实现）
  CMakeLists.txt
```

> **重要修正（原计划作废）**：早先想把 Adapter+Device 合并成一个类，已否决。
> 复刻要**贴合 UE 三层骨架**：`FD3D12Adapter`(物理GPU/工厂/Device容器) → `FD3D12Device`(GPU节点/Queues) → `FD3D12Queue`(D3DCommandQueue+Fence)，单 GPU 也不合并。
> 只精简内部细节：接口多版本数组→留基础版；多GPU数组→单个；多线程提交管线(Payload/对象池/Timing)→去掉。
>
> **文件位置简化（对照 UE 源码时注意）**：UE 里 `FD3D12Queue` 类在 `D3D12Device.h`（约 85 行起）、`FD3D12Fence` 在 `D3D12Submission.h`、`D3D12Queue.h` 本身只放 `ED3D12QueueType` 枚举 + 辅助函数（`GetD3DCommandListType` 等）。本项目把这三处**合并进一个 `D3D12Queue.h`**，属"文件位置简化"，类的层级/命名不变。

### UE 源码参考路径
- 接口层：`F:\workspace\UnrealEngine58\Engine\Source\Runtime\RHI\` 或者可能在这儿 `F:\shakervon_engine_merge\Engine\Source\Runtime\RHI\`
- 实现层：`F:\workspace\UnrealEngine58\Engine\Source\Runtime\D3D12RHI\Private\` 或者可能在这儿 `F:\shakervon_engine_merge\Engine\Source\Runtime\D3D12RHI\Private\`
- UE 版本固定，目录路径可能变动（以用户告知为准）

---

## 实现章节顺序

| 章 | 内容 | 我们的文件 | UE 参考文件 |
|---|---|---|---|
| 1 | 设备初始化 | D3D12Adapter/Device/Queue.h/.cpp | D3D12Adapter.h/.cpp, D3D12Device.h, D3D12Queue.h |
| 2 | RHI 资源基类 | RHIResources.h, D3D12Resources.h | RHIResources.h |
| 3 | Buffer 封装 | RHIResources.h(Buffer), D3D12Resources.cpp | D3D12Buffer.cpp |
| 4 | Descriptor Heap | D3D12Descriptors.h/.cpp | D3D12Descriptors.h |
| 5 | Shader & PSO | D3D12PipelineState.h/.cpp | D3D12PipelineState.h, D3D12RootSignature.h |
| 6 | CommandList | RHICommandList.h, D3D12Commands.cpp | D3D12Commands.cpp |
| 7 | Texture 封装 | RHIResources.h(Texture) | D3D12Texture.h |
| 8 | 整合：画三角形 | — | — |

**约束（第1~8章）**：不考虑多线程（无 RHI Thread，无 CommandList 并行录制），Fence 用 inline 阻塞等待。

### 未来章节（进阶，第6章后）
| 章 | 内容 | 说明 |
|---|---|---|
| 9+ | Submission / 中断线程 | 复刻 UE 的 InterruptThread：GPU 完成的 event 等待外包给专职后台线程，业务线程不再 inline 阻塞，实现 CPU/GPU 重叠 |
| 9+ | 多线程 Present | 同属此套流水线：UE 的 Present 是排进 Submission 线程的 payload（SchedulePresent / PresentOnSubmissionThread / WaitForLastPresent / PresentEvent），不能独立提前加。第1~8章用 inline `FD3D12Viewport::Present()` 直调，对应 inline `WaitCPU()` 的同款简化，同章一起加回 |

**为什么放到第6章后**：中断线程的循环外壳很简单（Core 已有 `FRunnableThread`/`FRunnable`/`FEvent`，UE 的 `FD3D12Thread` 就是 `FRunnableThread` 薄封装），但它唤醒后要处理的「payload」——命令列表/分配器回池、资源延迟删除、query 解析、触发 `FGraphEvent` 唤醒等待任务——**依赖第2/3/6章的资源与命令列表系统**。没有这些，中断线程醒来无活可干。等第6章 CommandList + 资源延迟删除到位，它才有真正的「客户」，那时单开此章顺理成章。第1~8章先用 inline 阻塞 `Flush()`，但 fence-per-queue 骨架已为它留位。

---

## 第1章：设备初始化

### UE 的层级结构（D3D12Adapter.h 顶部注释）

```
RHI
 └── FD3D12Adapter（一块物理 GPU，可含多节点用于 LDA/SLI）
       ├── FD3D12Device（GPU 节点0）
       │     ├── FD3D12Queue (Direct)
       │     └── FD3D12Queue (Compute/Copy)
       └── FD3D12Device（GPU 节点1，SLI 场景）
```

单 GPU 场景下只有 1 个 Adapter、1 个 Device、1 个 Direct Queue。

---

### UE FD3D12Adapter 关键成员解析

#### 1. 设备版本升级模式

```cpp
// UE 同时持有 ID3D12Device 到 ID3D12Device12 的所有版本
TRefCountPtr<ID3D12Device>   RootDevice;    // 基础版本，所有平台保证有
TRefCountPtr<ID3D12Device5>  RootDevice5;   // 支持 DXR（光线追踪）
TRefCountPtr<ID3D12Device10> RootDevice10;  // 支持 Work Graphs 等新特性
```

**设计意图**：不同硬件支持不同版本，QueryInterface 升级，访问新 API 时用高版本，
基础创建用低版本。我们只需要 `ID3D12Device` 基础版本。

#### 2. DXGI Factory 版本升级（同样模式）

```cpp
TRefCountPtr<IDXGIFactory2> DxgiFactory2;  // 基础
TRefCountPtr<IDXGIFactory4> DxgiFactory4;  // CreateSwapChainForHwnd
TRefCountPtr<IDXGIFactory6> DxgiFactory6;  // GPU 偏好（DXGI_GPU_PREFERENCE）
```

**设计意图**：Factory6 能按性能/省电偏好枚举 Adapter，旧机器 fallback 到 Factory4/2。
我们只需要 `IDXGIFactory4`（CreateSwapChainForHwnd 必须用 Factory2+）。

#### 3. 核心持有对象

```cpp
TRefCountPtr<IDXGIAdapter>       DxgiAdapter;          // 物理 GPU
TRefCountPtr<ID3D12Device>       RootDevice;            // 逻辑设备（工厂）
TArray<FD3D12Viewport*>          Viewports;             // 所有 SwapChain
TUniquePtr<FD3D12ManualFence>    FrameFence;            // 帧级 GPU 同步
FD3D12RootSignatureManager       RootSignatureManager;  // 根签名缓存
FD3D12PipelineStateCache         PipelineStateCache;    // PSO 缓存
FD3D12UploadHeapAllocator*       UploadHeapAllocator;   // Upload 堆分配器
TStaticArray<FD3D12Device*, MAX_NUM_GPUS> Devices;      // 每节点 Device
```

**我们简化版只需要**：DxgiAdapter、RootDevice。

> **FrameFence 澄清**：UE 的 `FrameFence`（`FD3D12ManualFence`）是"每帧 +1"的帧级同步，服务于交换链 BackBuffer 循环 / `EndFrame`；第1章已有 queue 级 `FD3D12Fence`，同步自洽。**FrameFence 第1章不建，留到第8章整合（画三角形）时再加**——所以下面 FD3D12Adapter 成员表里不含 FrameFence。

> **Viewports 澄清**：UE 的 `TArray<FD3D12Viewport*> Viewports` 是挂在 **Adapter 上**的注册表（`D3D12Adapter.h`），不是 Viewport 自管。第1章单 GPU 单 viewport，**先在 Adapter 上不建 Viewports 注册表，Viewport 由调用方持有**；PSO/RootSignature 后续章节再加。

#### 4. FD3D12AdapterDesc — 硬件能力描述

```cpp
struct FD3D12AdapterDesc
{
    DXGI_ADAPTER_DESC           Desc;                  // GPU 名称、显存大小
    D3D_FEATURE_LEVEL           MaxSupportedFeatureLevel;
    D3D_SHADER_MODEL            MaxSupportedShaderModel;
    D3D12_RESOURCE_BINDING_TIER ResourceBindingTier;   // Tier1/2/3 影响绑定方式
    D3D12_RESOURCE_HEAP_TIER    ResourceHeapTier;      // Tier1=Buffer/Texture分堆
    bool bUMA;                                          // 集显（CPU/GPU共享内存）
};
```

**设计意图**：把"这块 GPU 能做什么"和"Device 对象本身"分离，
方便枚举阶段先选 GPU、再创建 Device。

---

### 我们的实现设计（贴合 UE 三层，不合并）

保留 UE 的三层骨架与创建/回指关系（见顶部「核心原则」）：

```
FD3D12Adapter::Initialize()   建 Factory → 选 Adapter → 建 RootDevice → 填 Desc → new FD3D12Device
   └─ FD3D12Device(Adapter*, GPUIndex)   建 Queues[Direct/Copy/Async]
        └─ FD3D12Queue(Device*, QueueType)   建 D3DCommandQueue + FD3D12Fence
```

**FD3D12Adapter**（D3D12Adapter.h/.cpp）：
```cpp
ComPtr<IDXGIFactory4> DxgiFactory;   // UE: DxgiFactory2..7，只留基础版
ComPtr<IDXGIAdapter>  DxgiAdapter;   // 选中的物理 GPU（枚举时局部用 IDXGIAdapter1 拿 Desc1 过滤软件卡）
ComPtr<ID3D12Device>  RootDevice;    // UE: RootDevice..12，只留基础版；命名保持 RootDevice
FD3D12AdapterDesc     Desc;          // { DXGI_ADAPTER_DESC; D3D_FEATURE_LEVEL MaxSupportedFeatureLevel; }
FD3D12Device*         Device;        // UE: Devices[MAX_NUM_GPUS]，单 GPU 只留一个
```

**FD3D12Device**（D3D12Device.h/.cpp）：
```cpp
FD3D12Adapter* Adapter;     // 回指父 Adapter
uint32         GPUIndex;
TArray/数组 Queues[ED3D12QueueType::Count];   // Direct/Copy/Async 三条
```

**FD3D12Queue + FD3D12Fence**（D3D12Queue.h/.cpp）：
```cpp
enum class ED3D12QueueType { Direct=0, Copy, Async, Count }; // 对齐 UE D3D12Queue.h

struct FD3D12Fence {          // UE: struct，在 D3D12Submission.h
    FD3D12Queue* OwnerQueue;
    ComPtr<ID3D12Fence> D3DFence;
    uint64 NextCompletionValue = 1;
    // 单线程简化：UE 的 event 等待由中断线程做，我们自己加 HANDLE + Signal/Wait 使之自洽
    HANDLE FenceEvent;
};

class FD3D12Queue {
    FD3D12Device* Device;
    ED3D12QueueType Type;
    ComPtr<ID3D12CommandQueue> D3DCommandQueue;
    FD3D12Fence Fence;
};
```

**去掉的多线程细节**：Payload 提交队列、命令分配器/列表对象池、Timing/DiagnosticBuffer。
**CommandAllocator/List 不在第1章**（第6章 CommandList）。SwapChain（Viewport）第1章末尾建，但归 FD3D12Viewport 自管。

---

### 进度（历史过程与当前里程碑）
- [x] 第1章 UE 源码讲解完成
- [x] 第1章 实现：`D3D12Queue.h/.cpp`（ED3D12QueueType / FD3D12Fence 纯数据struct / FD3D12Queue：带参构造 + Signal/Wait/WaitCPU）
  - 定案：FD3D12Fence 无成员函数，操作全在 Queue 上（贴 UE）；Queue non-movable，Device 用 `vector<unique_ptr<FD3D12Queue>>` 存储
- [x] 第1章 实现：`D3D12Device.h/.cpp`（Adapter回指 + GPUIndex + Queues；GetDevice() 转发 Adapter->GetD3DDevice()）
- [x] 第1章 实现：`D3D12Adapter.h/.cpp`（FD3D12AdapterDesc + FindAdapter + CreateRootDevice + InitializeDevices）
  - 选卡用 `IDXGIFactory6::EnumAdapterByGpuPreference(HIGH_PERFORMANCE)` 选独显；FindAdapter 与 CreateRootDevice 必须用同一枚举方式，否则 AdapterIndex 对不上（或改按 LUID 匹配更稳）
- [x] 第1章 实现：`D3D12Viewport.h/.cpp`（FD3D12Viewport：Init 建 SwapChain + ResizeInternal 取 BackBuffer + PresentInternal；两段式对齐 UE）
- [x] **第1章 闭环验证通过**：`RHITest.exe`（Win32 窗口 + 三层 + Viewport + Present），黑窗口不崩，选中 NVIDIA RTX 3060
- [x] 第2章：RHI 资源基类（`FRHIResource` + `ERHIResourceType`；`FD3D12Resource` 包 ID3D12Resource）
  - 定案（方案A）：`FRHIResource` 不做侵入式计数，生命周期交给 `TRefCountPtr(=shared_ptr)`，public 虚析构；等实现自己的侵入式 TRefCountPtr 再补 AddRef/Release
- [x] 第3章：Buffer（`EBufferUsageFlags` + `FRHIBufferDesc` + `FRHIBuffer`；`FD3D12Buffer` 持 `FD3D12Resource`；`FD3D12Device::CreateBuffer` = CreateCommittedResource + UPLOAD 堆 Map/memcpy 上传）
  - DEFAULT 堆 + 初始数据（staging + copy）留到有 CommandList 后
- [x] 第4章：Descriptor Heap（`FD3D12DescriptorHeap` 封装 + 线性 Allocate；Viewport 用 RTV 堆给 back buffer 建 RTV，固定槽映射）
- [x] 第6章：CommandList（`FD3D12CommandAllocator` + `FD3D12CommandList`；RHITest 每帧 barrier + ClearRenderTargetView + ExecuteCommandLists → **清屏成蓝色**）
  - 简化：单分配器 + 每帧 Flush（无帧重叠）；多缓冲（N 分配器 + 每帧 fence）留后面
- [x] 第5章：Shader & PSO
  - `D3D12Shader`（运行时 D3DCompile → 字节码 blob；UE 离线编译，我们简化）
  - `D3D12RootSignature`（序列化 + 创建；三角形用空签名 + `ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT`）
  - `D3D12PipelineState`（CreateGraphicsPipelineState；手填光栅/混合/深度关/RTV格式=swapchain）
- [x] **画三角形（整合）**：RHITest 顶点(pos+color) → VB → 编译VS/PS → RootSig → PSO → 每帧 clear + DrawInstanced，蓝底彩色三角形。**≈ 达成 A3-M3（Test 只调 RHI 画出图元）**
- [x] A3 里程碑（2026-09-27 按阶段 A 范围验收）：
  - [x] M2（3D 骨架）：**深度缓冲(DSV) + 透视相机 + 索引绘制 → 转动的 3D 立方体**
    - `FD3D12Device::CreateDepthBuffer`（第一个纹理资源：TEXTURE2D / D32_FLOAT / DEFAULT堆 / ALLOW_DEPTH_STENCIL / 优化清除值）+ DSV 堆 + CreateDepthStencilView(nullptr desc)
    - PSO 开深度（DepthEnable + DepthFunc=LESS + DSVFormat）；每帧 OMSetRenderTargets 带 DSV + ClearDepthStencilView(仅 DEPTH，D32 无 stencil)
    - 索引缓冲（`IndexBuffer` flag + `D3D12_INDEX_BUFFER_VIEW`/R16_UINT + `DrawIndexedInstanced`）
    - 透视相机：`WVP = World*View*Proj`（DirectXMath LH，行向量从左到右=空间转换顺序），转置上传
  - [x] M2 剩余：Texture + SRV + Sampler + descriptor table（给立方体贴图；第一次 CreateShaderResourceView，DEFAULT堆纹理需 staging+copy 上传）→ **棋盘格贴图立方体已跑通**
  - [x] M4：描述符回收、多帧同步、Fence 保护延迟释放完成；状态追踪和自动 Barrier 转阶段 B。
  - [x] M5（基础）：常量缓冲让三角形转起来
    - `CreateBuffer` 加 ConstantBuffer 分支（256 对齐）；`FD3D12Buffer::GetMappedData()` 持久映射
    - 根签名加 root CBV 参数（b0）；shader `cbuffer` + `mul(pos, WVP)`
    - 每帧：DirectXMath 旋转矩阵 + **转置上传**（HLSL 默认列主序坑）+ memcpy + `SetGraphicsRootConstantBufferView(index, GPU_VA)`
    - 关键认知：root CBV 不建描述符对象，直接传资源 GPU VA（view 坍缩成地址，只对 buffer 成立；纹理必须建真描述符进堆）
    - 单 CB 够用因每帧 Flush 无重叠；多帧重叠时才需 **ring buffer**（M5 进阶，随 M4 一起做）
  - [x] M4：阶段 A 范围完成
    - [x] **M4-a 多帧同步**：去每帧 Flush（N=2 分配器 + CB ring + 每帧 fence）。提交后只记 `FrameFenceValue[Slot]=Signal()` 不等；复用 slot 前 `WaitCPU(FrameFenceValue[Slot])`（稳态秒过，只在 CPU 领先≥N帧时反压）。一条 CmdList 每帧 Reset 到当帧 allocator。退出补 `WaitForGPU()`（不再每帧 Flush，否则销毁资源时 GPU 仍在用→崩）。简化：复用 Direct 队列 fence 当 FrameFence（UE 在 Adapter 上独立 ManualFence）；allocator 固定 N 个手动轮（UE 有池）。画面不变，收益在 CPU/GPU 重叠。
    - [x] M4-b 延迟释放：PendingDeletes → 实际提交 Fence → DeletionQueue；GPU gate 验证真实 draw 在飞时 SRV/纹理与槽位保持存活，完成后释放并复用。
    - [ ] 自动 Barrier + 状态追踪 → 阶段 B（RHICore）正式化
    - [x] 描述符管理：固定容量 SRV/RTV/DSV 堆空闲槽回收；完整分配器和扩容留后续。
  - [x] A2：`FDynamicRHI` 抽象（接口/实现分离 + 全局分发）
    - [x] **A2-a 资源创建抽象**：RHI/ 立 `FDynamicRHI` 纯虚接口（`RHICreateBuffer`/`RHICreateTexture`，只返回 FRHI 基类型）+ 全局 `GDynamicRHI`（定义在 RHI.cpp）+ 同名自由函数转发；D3D12RHI/ `FD3D12DynamicRHI : FDynamicRHI` 持有 Adapter，`Init()` 建三层，创建转发到 `Device->Create*`。RHITest 建 VB/IB/CB/Tex 改走 `RHICreateBuffer/RHICreateTexture`。UE 对照 DynamicRHI.h（`GDynamicRHI` + `RHICreateXxx` 系）。
    - 当前边界：普通初始化/绘制只使用 RHI；平台工厂和 Shader 编译隔离到 TestPlatform，专用 GPU 验证隔离到 BackendValidation。
    - [x] A2-b 命令列表抽象（`FRHICommandList`）：分 3 片
      - [x] **片1**：两层骨架 `FRHICommandList`(RHI/转发) → `IRHICommandContext`(RHI/抽象) → `FD3D12CommandContext`(D3D12RHI/实现，包 FD3D12CommandList + 多帧同步 + CB ring)。方法贴 UE：`BeginFrame`/`BeginRenderPass(clearColor)`/`SetGraphicsPipelineState(FRHIGraphicsPipelineState*)`/`SetShaderConstants`/`SetTexture(FRHITexture*)`/`SetStreamSource(FRHIBuffer*)`/`DrawIndexedPrimitive(FRHIBuffer* IB, count)`/`EndRenderPass`/`EndFrame`/`WaitForGPU`。**RHITest 的 draw loop 零裸 D3D12**；VB/IB/Tex 变 FRHI 基类型、`static_pointer_cast` 全消失；帧管理/CB ring/VBV/IBV/视口全搬进 context。
        - 当前 PSO 分层：`FD3D12GraphicsPipelineState : FRHIGraphicsPipelineState, FD3D12PipelineStateCommonData`；公共数据关联非拥有的根签名指针和拥有的底层 `FD3D12PipelineState`，后者只封装原生 PSO。
        - 两层为多线程留缝：将来在 `FRHICommandList→context` 间插「命令缓存 + RHI 线程重放」，两端不动（第9章）。
        - 保留简化：`SetShaderConstants`=root CBV+每帧一个 CB（非每 Draw 分配；UE UniformBuffer 后续接入）。PSO 已走 RHI 创建，根签名由 Adapter 管理；Context 初始化仍接收具体 Device/Queue/Viewport/DSVHeap/SRVHeap，SRV 绑定仍用 `Tex->GetSRVSlot()`。
        - 踩坑：① `FRHICommandList` 构造忘存 `Context` → 空指针崩；② `D3D12CommandContext.cpp` 漏 `#include "D3D12RootSignature.h"`（调 GetRootSignature 需完整类型）；③ 临时改造时 CB ring for 循环注释掉却留了用 `CmdAllocs[0]`(nullptr) 建 CmdList 的残行 → CreateCommandList 崩。
      - [x] 片2：PSO/动态根签名、RenderPass/Viewport、Context 后端初始化与即时命令列表获取均已接通。
        - [x] Shader、VertexDeclaration、Rasterizer/DepthStencil/Blend 状态均经 RHI 创建；`FGraphicsPipelineStateInitializer` 汇总 Shader、输入布局、固定状态、附件格式和采样数，后端转换为原生 PSO 描述。
        - [x] `RHICreateGraphicsPipelineState → FD3D12DynamicRHI`（入口位于 D3D12State.cpp）→ `Adapter::GetRootSignature → QBSS → RootSignatureManager`；RHITest 只持有 `TRefCountPtr<FRHIGraphicsPipelineState>`，不再手工创建根签名/底层 PSO。
        - [x] `FD3D12RootSignatureDesc` 生成描述；`FD3D12RootSignature` 保存原生对象和根参数位置映射；Adapter 持有 Manager，按布局缓存并共享根签名。新增 `FD3D12AdapterChild` 基类供根签名及 Manager 使用。
        - [x] Context 通过 `VS_RootCBVs` / `PS_SRVs` 查询实际 Root Parameter；`SetShaderConstants(0, ...)` 和 `SetTexture(0, ...)` 分别表示 VS b0 / PS t0，不再向上暴露根参数序号；BeginFrame 清除当前根签名记录。
        - 当前简化：显式填写 Shader ResourceCounts（UE 从编译产物读取）；仅 VS b0、PS t0/s0，space0，数量 0/1，暂不分档；point/wrap 静态采样器；根签名 1.0；单线程 std::map 缓存。底层 PSO 缓存、完整 Shader 编译系统、动态采样器尚未实现。
        - 对照基准：`F:\workspace\UnrealEngine58` 的 Windows D3D12 使用 `USE_STATIC_ROOT_SIGNATURE=0`；本项目沿动态根签名路径推进。
        - 2026-09-25 检查：RHITest 已改为 `PSO = RHICreateGraphicsPipelineState(Initialier)`；`cmake --build out/build/x64-Debug --target RHITest` 编译链接通过。本次未启动图形程序，画面及缓存命中仍待运行验证。
        - [x] 2026-09-26：Context 从当前 PSO 的 `StreamStrides` 获取顶点步长；SetStreamSource 贯通 Offset，VBV 地址加 Offset、大小减 Offset，支持空 Buffer 解绑。BeginFrame 清空当前 PSO；直接绑定模式下切换 PSO 后需重新绑定顶点流。RHITest Debug 编译链接通过，非零偏移及画面尚未运行验证。
      - [x] 片3：SRV 绑定和 View 分层完成，RHITest 普通绘制不接触 D3D12；通用 Barrier 留阶段 B。
    - 简化：`TRefCountPtr = std::shared_ptr`，故基类/派生转换用隐式上转型 + `static_pointer_cast` 下转型（UE 是侵入式引用计数 + `ResourceCast`）
  - [x] 第7章：纹理（DSV + Texture + SRV + Sampler + descriptor table，第一次 CreateShaderResourceView）
    - `FRHITexture`/`FD3D12Texture`（持 committed FD3D12Resource + SRV slot）；`EPixelFormat` 起步（PF_R8G8B8A8_UNORM/PF_D32_FLOAT）
    - `FD3D12Device::CreateTexture`：DEFAULT堆 TEXTURE2D（COPY_DEST）→ **staging(UPLOAD) + GetCopyableFootprints 逐行256对齐拷 + CopyTextureRegion + barrier→PIXEL_SHADER_RESOURCE + 阻塞提交**（一次性 init）
    - `CreateShaderResourceView`：shader-visible CBV_SRV_UAV 堆 Allocate 一槽 + `Shader4ComponentMapping` 默认映射
    - 绑定：当前布局含 root CBV(b0) + descriptor table(SRV t0) + **static sampler(s0)**（省 sampler 堆）；每帧设描述符堆，Context 通过根签名映射查询 SRV 表位置，再调用 `SetGraphicsRootDescriptorTable`。
    - 立方体 8→24 顶点（每面独立 UV，texture seam），Vertex 换 Pos+UV
    - 踩坑：① UPLOAD 堆 `D3D12_HEAP_PROPERTIES` 必须 `={}` 归零（CPUPageProperty/MemoryPool=UNKNOWN）否则 E_INVALIDARG；② 临时 CommandList ctor 建完是**关闭态**，录制前先 `Reset`；③ 整块 memcpy 会花屏（须逐行跳 RowPitch）；④ 漏 `Shader4ComponentMapping`=采样全0黑图
    - 未做（留后）：EPixelFormat→DXGI 映射函数（现仍硬编 R8G8B8A8）；真正的 sampler 描述符堆；图片文件加载（现用代码生成棋盘格）

**阶段 A 收尾（2026-09-27）**：原剩余的 View/资源绑定、RenderPass/Viewport/Context、描述符回收与在飞生命周期验证已完成。Debug/Release 的 GPU 验收记录在 `out/stage-a/Debug` 和 `out/stage-a/Release`，详细实现说明见 `StageA_Completion.md`。

完整自动 Barrier/状态追踪归阶段 B；RHI 线程、完整 Shader 编译系统及完整 PSO 缓存不作为阶段 A 结束门槛。

**新增通用工具**：`Core/Base/EnumClassFlags.h`（`ENUM_CLASS_FLAGS` 宏 + EnumHasAnyFlags 等，仿 UE）。

**踩坑记录**：
- 曾用 UTF-8 with BOM 规避 cp936 GBK 误解析；后改为**全仓库 UTF-8 无 BOM + 根 CMakeLists 加 `/utf-8`**（MSVC 按 UTF-8 读源码）。`.editorconfig` 设 `charset = utf-8`。**`/utf-8` 现为硬依赖，勿删**。
- 空壳模块（无导出符号）不生成 `.lib`，链接它会 LNK1104。第2章 `FRHIResource` 导出后 RHI.lib 生成，已把 `RHI` 加回 D3D12RHI 链接。
- NodeMask 是"一个 device 内多 node（LDA/SLI）"，不是多物理卡；单卡填 1。选物理卡在枚举 adapter 阶段。

**工程约定补记**：所有 CMakeLists 的 `FILE(GLOB_RECURSE ...)` 已加 `CONFIGURE_DEPENDS`。跨模块 include 用 `Core/Base/...` 或 `Base/...`（靠 `..` / Core 暴露），不用 `../../`。

---

*切换机器后继续：先阅读当前状态和 StageA_Completion.md，从阶段 B 接续，不要重新开始第 1 章。*
### SRV 抽象接入检查（2026-09-26）

- 已完成：FRHIViewableResource、FRHIViewDesc、FRHIView、FRHIShaderResourceView；后端 FD3D12View → TD3D12View → FD3D12ShaderResourceView 与 FD3D12ShaderResourceView_RHI 包装。
- RHICreateShaderResourceView 在 D3D12SRV.cpp 创建并初始化包装对象；Device 持有 ResourceDescriptorHeap，Context 从同一 Device 获取该堆。
- RHITest 经 RHI 创建 SRV，以 SetShaderResourceViewParameter 绑定；Texture 的 SRVSlot 与旧 Device 创建函数已删除。换纹理时 DeferredDelete 旧 SRV，由其引用保住旧纹理。
- 验证：RHITest Debug 编译链接通过；本次没有运行窗口、验证换纹理或 GPU 在飞释放。
- 当前范围：Texture2D、RGBA8、mip0、PS t0；显式 SRV 绑定是教学接口。8 槽线性分配仍未回收，不能超过容量；纹理上传仍阻塞。SRV 基础创建与绑定已完成，完整 View 类型、动态采样器、描述符缓存不在本次完成范围。
- 下一步：RenderPass 的 Load/Store 动作描述，随后接入附件、Viewport/Context 抽象。阶段 A 主线剩 RenderPass/Viewport/Context 抽象、描述符回收与生命周期验证两块，以及整体验收。

### RenderPass 前置数据检查（2026-09-26）

- Load/Store、深度模板动作、FRHIRenderPassInfo 已定义。Backbuffer 已包装为 FD3D12Texture，深度创建返回 FD3D12Texture，上层持有 FRHITexture。
- FClearValueBinding 已接入纹理描述，backbuffer 绑定背景色，深度优化清除值来自同一份纹理元数据。Debug 编译链接通过；本次没有运行图形验证。
- BeginRenderPass 仍使用旧的 ClearColor 参数并无条件清除；下一步才接入附件和动作解释，不能将描述结构完成视为执行路径完成。

### RenderPass 执行接入检查（2026-09-27）

- BeginRenderPass 已接收 FRHIRenderPassInfo 和 Name；校验当前 backbuffer、固定 D32 附件、尺寸/格式和动作，按 LoadAction 选择清除并读取纹理 ClearValue。
- 支持 Clear_Store / Load_Store；仅一个颜色附件和固定深度附件，无 stencil、resolve、MRT 或任意离屏附件。Pass 配对检查已加入；结束转 PRESENT 仍是当前 backbuffer 专用策略。
- 深度优化清除值读取调用已补回；RHITest Debug 编译链接通过，本次未运行图形验证。下一步接 FRHIViewport 与 RHIGetViewportBackBuffer，Viewport 创建和 Context 获取仍待抽象。

### Viewport 创建抽象检查（2026-09-27）

- FRHIViewport、RHICreateViewport、RHIGetViewportBackBuffer 已接通；测试持有 RHI Viewport，每帧获取 backbuffer 的共享引用。Context 内部转换为具体类型。
- main.cpp 已直接包含 D3D12Descriptors.h，避免依赖旧 Viewport 头文件间接引入 DSV 堆定义；RHITest Debug 编译链接通过。未运行图形验证。
- 下一步：深度纹理经统一 RHICreateTexture 创建；DSV 管理和 Context 获取仍待收拢。
### DSV 后端封装完成（2026-09-27）

- Device 持有非 shader-visible DSV 堆；CreateTexture 的深度分支创建 FD3D12DepthStencilView，由 FD3D12Texture 持有。View 先于纹理资源析构。
- Context 从 RenderPass 深度纹理获取 DSV，验证同一 Device 和 View 初始化状态，不再依赖外部固定 DSV 槽位。Init 只接收 Device、Queue、Viewport。
- RHITest 已删除 DSV 堆、深度纹理下转型和原生 DSV 创建。深度格式仍限 D32、单层/mip0/单采样，可写 DSV；资源维持 DEPTH_WRITE，尚无通用状态追踪。
- SRV 堆约束移入 SRV 构造器；公共 GPU Handle 访问拒绝非 shader-visible View。两个描述符堆仍各为 8 槽线性分配，未实现回收。
- RHITest Debug 编译链接通过；本次未运行图形程序，画面、换纹理、异常路径及 GPU 在飞释放仍待运行验证。
- 下一步收拢 Context 获取与初始化；阶段 A 的描述符回收、生命周期验证及整体验收仍未完成。

### 默认 Context 所有权归位（2026-09-27）

- FD3D12Device 创建并持有 ImmediateCommandContext，通过 GetDefaultCommandContext 返回非拥有引用；成员顺序保证 Context 先于描述符堆与 Queue 析构。
- RHITest 不再拥有或创建 Context，仅借用 Device 的默认 Context 构造 FRHICommandList；退出前仍通过 WaitForGPU 等待 GPU 完成。
- 当前仅迁移所有权：Context::Init(Device, Queue, Viewport) 仍由测试调用，尚未完成初始化与视口解耦，也尚未收拢上层命令列表获取入口。
- 验证：RHITest Debug 编译链接通过；本次未运行图形程序。
- 下一步：解除 Context 初始化对 Viewport 的依赖，将初始化收回后端；描述符回收、生命周期验证及阶段 A 整体验收仍待完成。

### RTV 类型补齐（2026-09-27）

- 已增加 FD3D12RenderTargetView，复用 TD3D12View；校验非 shader-visible RTV 堆、资源所属 Device、RGBA8 Texture2D 单层/单 mip/单采样及 mip0/plane0 视图，创建 CPU 描述符。
- 已同步公共 View 注释。RHITest Debug 编译链接通过；未运行图形验证。
- 本步仅增加类型，尚未接入纹理所有权或 backbuffer 创建；Viewport 仍管理原 RTV 堆，Context 仍通过 Viewport 取得 RTV 和执行 Present。
- 下一步将 RTV 接到 FD3D12Texture，再迁移 backbuffer RTV 创建与 RenderPass 绑定，随后解除 Context 初始化对 Viewport 的依赖。

### 纹理 RTV 所有权接口完成（2026-09-27）

- FD3D12Texture 新增 RenderTargetView 所有权及 GetRenderTargetView / SetRenderTargetView；当前仅单个 mip0、非数组 RTV，未创建时返回 nullptr。
- View 成员位于 ResourcePtr 之后，确保先于资源析构；注释说明创建阶段接入和 GPU 使用期间不可替换的约束。
- RHITest Debug 编译链接通过；未运行图形验证。backbuffer 尚未调用新 setter，现有 RTV 创建与绑定路径仍在 Viewport。
- 下一步迁移 backbuffer RTV 创建，并让 RenderPass 从颜色纹理取得 RTV。

### 阶段 A 最终验收（2026-09-27）

- RTV 路径已接通，Device 持有三种描述符堆及默认 Context；Context 不再依赖 Viewport。ImmediateCommandList / Executor、RHIInit / RHIExit、独立 EndDrawingViewport 入口完成。
- 描述符固定容量空闲槽回收、显式 DeferredDelete 协议和 GPU gate 生命周期验证完成；常量缓冲扩展为每帧 64 KiB、每次绑定 256 字节对齐分配。
- Debug 与 Release 均编译运行通过；每种配置执行 120 次换纹理、24 次 Resize、24 次 DSV 创建销毁。640×480 图像回读中 27286 个非背景像素，背景值匹配，画面已检查；D3D12 调试队列无警告/错误。
- 主要绘制代码无 D3D12 类型/头文件；平台引导、编译与后端专用诊断的例外明确隔离。完整改动和保留限制见 StageA_Completion.md。
- 下一步：阶段 B，替换固定 backbuffer 屏障，学习 ERHIAccess / FRHITransition 和资源状态追踪。
