# DX12RHI 复刻计划表 v2（单线程先行 · 教学模式）

> 制定于 2026-08-05。参考 UE 源码：`G:\UnrealEngine\UE_Angelscript\Engine\Source\Runtime\{RHI, D3D12RHI}`

## 设计目标

- 在 `Core` 模块内新增**抽象层 `Core/RHI/` + 实现层 `Core/DX12RHI/`**，一个 DLL
- **`Source/DirectX12` 完全不动**（学习 DX12 本身的实验场）
- 协作方式：**Claude 教学**（讲设计、对照 UE 讲为什么、给验收标准），**代码自己写**
- 目标主要是**学习 UE 对 DX12 的封装方式**，核心路径裁剪版，不追求 UE 90 文件的完整规模

## 目录结构规划

```
Core/
├── RHI/          ← 抽象层（仿 UE Runtime/RHI/Public）
│   ├── RHIDefinitions.h    资源创建描述/状态枚举（DS/RS/BS/Sampler）
│   ├── RHIResources.h      FRHIResource 体系（引用计数基类 + Buffer/Texture/PSO/Shader）
│   ├── RHIContext.h        IRHICommandContext（命令录制接口）
│   ├── DynamicRHI.h        FDynamicRHI（设备/资源创建入口，全局 GDynamicRHI）
│   └── RHICommandList.h    FRHICommandList（命令录制 → RHIThread 提交）
│
└── DX12RHI/       ← D3D12 实现层（仿 UE Runtime/D3D12RHI/Private，精简版）
    ├── D3D12DynamicRHI.h/.cpp      FD3D12DynamicRHI
    ├── D3D12Adapter.h/.cpp         FD3D12Adapter（DXGI Factory/枚举适配器/多 GPU）
    ├── D3D12Device.h/.cpp          FD3D12Device + FD3D12Queue（命令队列管理）
    ├── D3D12Resources.h/.cpp       FD3D12Resource/FD3D12ResourceLocation + 分配
    ├── D3D12Descriptors.h/.cpp     FD3D12DescriptorHeap/在线离线堆
    ├── D3D12View.h/.cpp            SRV/UAV/RTV/DSV
    ├── D3D12PipelineState.h/.cpp   PSO + FD3D12RootSignature
    ├── D3D12State.h/.cpp           DS/RS/BS/Sampler 状态对象
    ├── D3D12Shader.h/.cpp          VS/PS/CS 编译缓存
    ├── D3D12CommandList.h/.cpp     FD3D12CommandList/Context/Allocator
    ├── D3D12Submission.h/.cpp      FD3D12Payload/FD3D12SyncPoint（RHIThread 提交）
    └── D3D12Util.h/.cpp            错误处理/HRESULT 检查/调试命名
```

## 阶段总览

```
阶段 0  抽象层骨架    RHI 接口设计（地基，慢慢做）
阶段 1  DX12 初始化   设备/队列/命令列表 → M1: 清屏
阶段 2  资源+描述符    Buffer/Texture/堆/视图 → M2
阶段 3  PSO+Shader    RootSig/Shader/PSO/状态 → M3: 画三角形
阶段 4  单线程渲染循环 命令/Barrier/提交/多帧 → M4
阶段 5  帧管线+可选    常量缓冲(单线程) → M5, 多线程改造留待 TaskGraph 完善
阶段 6  可选扩展       Query/DiskCache/Residency/Bindless
```

---

## 阶段 0 — 抽象层骨架（第 1~2 周，重点讲解，慢慢做）

| # | 任务 | 对照 UE | 讲解要点 |
|---|------|---------|---------|
| 0.1 | `RHI/RHIDefinitions.h`：资源创建描述、状态结构 | `RHIDefinitions.h` | UE 为什么用 Initializer 描述创建，而不是构造函数参数 |
| 0.2 | `RHI/RHIResources.h`：`FRHIResource`（原子引用计数基类）+ 资源/状态/PSO/Shader 抽象 | `RHIResources.h` | 引用计数设计；资源对象树怎么组织 |
| 0.3 | `RHI/DynamicRHI.h`：`FDynamicRHI` 抽象接口 + 全局 `GDynamicRHI` | `DynamicRHI.h` | 为什么全局单例 + 动态多态，而不是模板 |
| 0.4 | `RHI/RHIContext.h`：`IRHICommandContext` 纯虚录制接口 | `RHIContext.h` | 上下文 vs 命令列表的职责划分 |
| 0.5 | `RHI/RHICommandList.h`：命令录制封装 | `RHICommandList.h` | 命令"先录后执行"的思想 |

> **验收**：抽象层不含任何 D3D12 头，纯靠 Core 自带类型（`TRefCountPtr`/`FString`/`verify`）独立编译。这是地基，UE 里 RHI 层和 D3D12RHI 层是严格分离的。

## 阶段 1 — DX12 初始化/设备层（第 3~4 周）

| # | 任务 | 对照 UE | 讲解要点 |
|---|------|---------|---------|
| 1.1 | `D3D12Adapter`：DXGI Factory、适配器枚举、能力查询 | `D3D12Adapter.cpp` | UE 为什么 Adapter 和 Device 分离（多 GPU 支持） |
| 1.2 | `D3D12Device` + `FD3D12Queue`：设备、命令队列、Fence | `D3D12Device.h` | Queue 类型（Graphics/Compute/Copy）及用途 |
| 1.3 | `D3D12CommandList`（part1）：Allocator/CommandList 封装 | `D3D12CommandList.h` | Allocator 为什么独立于 CommandList（可复用） |
| 1.4 | `FD3D12DynamicRHI` + 交换链/视图管理 | `D3D12DynamicRHI.cpp`、`D3D12Viewport.h` | 驱动层的"入口"怎么组织 |
| | **里程碑 M1** | | `ConsoleMain.exe` 走 RHI：设备初始化 → 录制 Clear → 提交执行 |

## 阶段 2 — 资源与描述符（第 5~7 周）

| # | 任务 | 对照 UE | 讲解要点 |
|---|------|---------|---------|
| 2.1 | `D3D12Resources`：`FD3D12Resource`/`FD3D12ResourceLocation`/`FD3D12Heap` | `D3D12Resources.h`、`D3D12Allocation.h` | ResourceLocation 的"资源位置抽象"思想 |
| 2.2 | `D3D12Descriptors`：描述符堆封装、在线/离线堆、缓存 | `D3D12Descriptors.h`、`D3D12DescriptorCache.h` | 为什么描述符要离线保存 + 在线重绑定 |
| 2.3 | `D3D12View`：SRV/UAV/RTV/DSV | `D3D12View.h`、`D3D12SRV.cpp` 等 | 视图是"资源的视角" |
| 2.4 | Buffer/Texture 对接 RHI 抽象 | `D3D12Texture.cpp`、`D3D12Buffer.cpp` | 资源创建链路打通 |
| | **里程碑 M2** | | RHI 创建 Buffer/Texture + 四类视图 |

## 阶段 3 — 管线状态与 Shader（第 7~9 周）

| # | 任务 | 对照 UE | 讲解要点 |
|---|------|---------|---------|
| 3.1 | `D3D12RootSignature` | `D3D12RootSignature.h/.cpp` | 根签名的绑定层级设计 |
| 3.2 | `D3D12Shader` | `D3D12Shaders.cpp` | Shader 编译 + 反射，PSO 怎么关联 shader |
| 3.3 | `D3D12PipelineState` | `D3D12PipelineState.cpp` | PSO 缓存（key → PSO），为什么需要缓存 |
| 3.4 | `D3D12State`：DS/RS/BS/Sampler + 顶点声明 | `D3D12State.cpp`、`D3D12VertexDeclaration.cpp` | 状态对象化的意义 |
| 3.5 | 命令上下文补齐录制接口 | `D3D12CommandContext.cpp` | 录制 API 全貌 |
| | **里程碑 M3** | | **全程不直接调 D3D12 API 画三角形** |

## 阶段 4 — 单线程渲染循环（第 9~11 周）

| # | 任务 | 对照 UE | 讲解要点 |
|---|------|---------|---------|
| 4.1 | `D3D12CommandContext` 完整录制 + 状态缓存 | `D3D12CommandContext.cpp` | 状态缓存（避免重复 set）的思想 |
| 4.2 | Barrier 系统：`FD3D12BarriersFactory` + 状态追踪 | `D3D12BarriersFactory.h`、`D3D12LegacyBarriers.cpp` | 为什么 UE 把 Barrier 封装在资源层、按需生成 |
| 4.3 | 提交：单线程下"录制 → 直接提交 → 等 Fence" | `D3D12CommandListManager.cpp` | 先理解串行提交，为将来并行打基础 |
| 4.4 | 多帧同步 + GPU 资源延迟删除 | `D3D12RHICommon.h` | **延迟删除**是驱动正确性的核心 |
| | **里程碑 M4** | | 单线程 ≥100 帧无崩溃，无资源提前释放 |

> ⚠️ **4.3/4.4 先做单线程版**：不引入 RHIThread，录制和提交在同一线程。等 TaskGraph 完善后再加 RHI 线程。

## 阶段 5 — 帧管线与（未来）RHIThread

| # | 任务 | 对照 UE | 说明 |
|---|------|---------|------|
| 5.1 | 常量缓冲 + ring buffer 分配（单线程） | `D3D12ConstantBuffer.cpp` | 每帧动态数据更新 |
| 5.2 | 帧循环 GameThread 录制（当前即同步提交） | UE 帧循环 | 单线程版收尾 |
| 5.3 | **RHIThread 双线程提交（待 TaskGraph 完善后）** | `D3D12Submission.h` | 那时再讲 Payload/SyncPoint/延迟提交 |
| | **里程碑 M5** | | 单线程帧循环完整；多线程版本作为后续任务 |

## 阶段 6 — 可选扩展（有精力再推进）

Query/GPUProfiler → Transient 分配器 → Bindless → DiskCache → Residency

---

## 关键设计对照点（学习重点）

| 概念 | UE 怎么做 | 学习价值 |
|------|----------|---------|
| **引用计数** | `FRHIResource` + 原子 refcount，`TRefCountPtr` 管理 | 对齐 Core 已有的 `TRefCountPtr` |
| **延迟删除** | GPU 仍用资源时不能删，fence 完成才释放 | 驱动正确性核心 |
| **命令录制 vs 执行** | 先录命令列表，提交时才真正执行 | 理解 UE 多线程渲染的骨架（当前先单线程） |
| **状态追踪** | `FD3D12Resource` 维护资源状态，Barrier 按需生成 | UE 把 Barrier 封装进资源层 |
| **描述符缓存** | 离线堆保存 + 在线堆重绑定 | DX12 性能关键 |

## 注意项

1. **先实现单线程版本**，等 Core 的 TaskGraph 完善后，再实现真正的 RHIThread 双线程提交。
2. 目标主要是**学习 UE 对 DX12 的封装方式**，利于复刻；核心路径裁剪版，不追求 90 文件的完整规模。
3. `Source/DirectX12` 是学习 DX12 本身的实验场，**不要动**。
4. 协作模式：Claude 教学（讲解设计/对照 UE/验收标准），代码自己写。
