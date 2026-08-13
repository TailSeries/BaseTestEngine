# UE RHI 封装 DX12 学习笔记

## 教学约定
每章节节奏：**先讲 UE 源码 → 再对照实现自己的简化版**

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
  Private/
    D3D12RHIPrivate.h              ← 总 include
    D3D12Device.h / .cpp           ← 对应 UE D3D12Adapter（单GPU合并）
    D3D12Viewport.h / .cpp         ← 对应 UE D3D12Viewport（SwapChain）
    D3D12Resources.h / .cpp        ← 对应 UE D3D12Resources（资源基类）
    D3D12Descriptors.h / .cpp      ← 对应 UE D3D12Descriptors（Heap分配）
    D3D12PipelineState.h / .cpp    ← 对应 UE D3D12PipelineState + RootSignature
    D3D12Commands.cpp              ← 对应 UE D3D12Commands（CommandList实现）
  CMakeLists.txt
```

### UE 源码参考路径
- 接口层：`F:\shakervon_engine_merge\Engine\Source\Runtime\RHI\`
- 实现层：`F:\shakervon_engine_merge\Engine\Source\Runtime\D3D12RHI\Private\`
- UE 版本固定，目录路径可能变动（以用户告知为准）

---

## 实现章节顺序

| 章 | 内容 | 我们的文件 | UE 参考文件 |
|---|---|---|---|
| 1 | 设备初始化 | D3D12Device.h/.cpp | D3D12Adapter.h/.cpp |
| 2 | RHI 资源基类 | RHIResources.h, D3D12Resources.h | RHIResources.h |
| 3 | Buffer 封装 | RHIResources.h(Buffer), D3D12Resources.cpp | D3D12Buffer.cpp |
| 4 | Descriptor Heap | D3D12Descriptors.h/.cpp | D3D12Descriptors.h |
| 5 | Shader & PSO | D3D12PipelineState.h/.cpp | D3D12PipelineState.h, D3D12RootSignature.h |
| 6 | CommandList | RHICommandList.h, D3D12Commands.cpp | D3D12Commands.cpp |
| 7 | Texture 封装 | RHIResources.h(Texture) | D3D12Texture.h |
| 8 | 整合：画三角形 | — | — |

**约束**：不考虑多线程（无 RHI Thread，无 CommandList 并行录制）

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

### 我们的简化版设计

UE 把 Adapter（物理GPU信息+Device工厂） 和 Device（节点级对象） 分成两个类，
是为了支持 LDA 多节点。单 GPU 下我们合并成一个 `FD3D12Device`：

```
FD3D12Device（我们的）= UE 的 FD3D12Adapter + FD3D12Device 合并
```

**持有内容（第1章范围）**：

```cpp
class FD3D12Device
{
    // 枚举/工厂
    ComPtr<IDXGIFactory4>      DxgiFactory;
    ComPtr<IDXGIAdapter1>      DxgiAdapter;      // 选中的物理 GPU

    // 核心设备
    ComPtr<ID3D12Device>       Device;

    // 命令提交
    ComPtr<ID3D12CommandQueue> CommandQueue;     // DIRECT 类型

    // 帧同步
    ComPtr<ID3D12Fence>        FrameFence;
    uint64                     FenceValues[FrameCount];
    HANDLE                     FenceEvent;

    static constexpr uint32    FrameCount = 2;  // 双缓冲
};
```

SwapChain（Viewport）单独放在 `FD3D12Viewport`，第1章末尾一并创建，
但逻辑上它属于第1章（设备初始化的一部分）。

---

### 进度
- [x] 第1章 UE 源码讲解完成
- [ ] 第1章 实现：创建 CMakeLists.txt、D3D12Device.h、D3D12Device.cpp
- [ ] 第2章及后续

---

*切换机器后继续：直接从"第1章实现"开始，先建 Source/RHI/ 和 Source/D3D12RHI/ 的 CMakeLists.txt*
