# UE RHI 封装 DX12 学习笔记

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

### UE 源码参考路径
- 接口层：`F:\shakervon_engine_merge\Engine\Source\Runtime\RHI\`
- 实现层：`F:\shakervon_engine_merge\Engine\Source\Runtime\D3D12RHI\Private\`
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

**我们简化版只需要**：DxgiAdapter、RootDevice、FrameFence。
Viewports 挪到 D3D12Viewport 自己管，PSO/RootSignature 后续章节再加。

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

### 进度
- [x] 第1章 UE 源码讲解完成
- [ ] 第1章 实现：创建 CMakeLists.txt、D3D12Device.h、D3D12Device.cpp
- [ ] 第2章及后续

---

*切换机器后继续：直接从"第1章实现"开始，先建 Source/RHI/ 和 Source/D3D12RHI/ 的 CMakeLists.txt*
