# UE 渲染体系学习路线（使用 → 理解 → 复刻）

> 目标：不是只会打开 UE 的渲染功能，也不是孤立地阅读源码；而是建立一条可验证的链路：
>
> ```text
> 视觉表现 → UE 编辑器设置 → GPU Pass / 资源 → UE 源码 → 自己的 RHI / Renderer 实现
> ```
>
> 两条学习线并行推进：
>
> - **原理 / 源码 / 复刻线**：理解 UE 为什么这样设计，并亲自实现其核心抽象。
> - **UE 使用 / 观察线**：知道一个功能如何启用、效果和代价是什么、它在 GPU 上实际做了什么。

---

## 一、总架构：两条线最终汇合

```text
A. 原理 / 源码 / 复刻线
DX12 → Core 基础设施 → RHI → D3D12RHI → Renderer → RDG → 独立 Renderer

B. UE 使用 / 观察线
编辑器 → 场景/资产 → 材质 → 光照 → 阴影 → 后处理 → 性能分析 → 渲染扩展
```

两条线分别回答不同问题：

| 学习线 | 核心问题 |
|---|---|
| 原理 / 复刻 | UE 的 RHI、RDG、Renderer 为什么有这些层，它们如何最终成为 GPU 命令？ |
| 使用 / 观察 | 某个 UE 渲染功能如何生效、视觉结果是什么、GPU 成本在哪里、如何调试？ |

> **RHI 不是 Renderer。**
>
> - RHI：抽象 GPU 资源与命令，并由 D3D12RHI 落地到 DX12。
> - Renderer：决定这一帧画什么、哪些物体可见、需要哪些 Pass、使用何种 Shader/PSO。
> - RDG：组织 Pass、资源依赖、Barrier 和 transient 生命周期。

---

# Part A：原理 / 源码 / 复刻线

## A0：纯 DX12 基础

### 目标

建立 GPU 执行模型，能解释并排查裸 DX12 的同步、资源和绑定错误。

### 已覆盖 / 应覆盖内容

```text
Swap Chain / Back Buffer / Present
Command Allocator / Command List / Command Queue
Fence 与 CPU-GPU 同步
FrameResource 环形缓冲
Vertex / Index Buffer
Input Layout / Primitive Topology
World / View / Projection
Root Signature / PSO / Shader
Descriptor Heap：CBV / SRV / UAV / RTV / DSV
Resource State / Barrier
```

### 验收

能够独立定位并解释：

```text
- 常量缓冲 GPU VA / 偏移计算错误
- Vertex / Index Buffer 的字节数或视图范围错误
- Root parameter 与 HLSL register 绑定错位
- PSO 状态未初始化
- Primitive Topology 未设置或不匹配
- FrameResource 被 GPU 使用时被 CPU 覆盖
- 遗漏 Present 或 Back Buffer 轮转
```

### 当前阶段

- Chapter 8 `LitWaves`：Object / Material / Pass 三层常量数据、光照、动态顶点缓冲、帧资源。
- 后续 Chapter 9：Texture、SRV、Sampler、描述符堆混合使用。

---

## A1：Core 基础设施

### 目标

先有承载 RHI 的可靠底座，而不是急着上多线程渲染。

```text
Core/
├─ Base/        基础类型、断言、日志、平台宏、引用计数
├─ Memory/      分配与资源生命周期基础
├─ Threads/     Thread / Event / Lock / TLS
├─ Task/        TaskGraph（后续逐步完善）
├─ Containers/  Array / Map / Queue / BitArray
└─ File/        文件、路径、后续异步 IO
```

### 三个核心契约

1. **CPU 所有权与 GPU 生命周期分离**

```text
CPU 不再持有资源
≠
GPU 已经不使用资源
```

资源释放必须受最后一次 GPU Fence 使用值保护，Fence 完成后才能真正释放底层 D3D12 resource、descriptor 或 upload allocation。

2. **引用计数与所有权**

```text
FRHIResource + TRefCountPtr 风格
```

要明确：C++ 对象是否活着，与 GPU 是否仍访问资源，是两个不同的问题。

3. **单线程正确性优先**

```text
先：同一线程录制和提交
后：TaskGraph 成熟后再做 RHIThread / 并行提交
```

### 验收

```text
Core 不依赖 d3d12.h；
RHI 可依赖 Core；
Core 有足以承载资源、日志、引用计数和后续同步的基础能力。
```

---

## A2：RHI 抽象层

对应 `Core/RHI/`，先做抽象契约，不能包含任何 D3D12 类型。

```text
Core/RHI/
├─ RHIDefinitions.h
├─ RHIResources.h
├─ DynamicRHI.h
├─ RHIContext.h
└─ RHICommandList.h
```

### 学习重点

| 模块 | 目标 |
|---|---|
| `RHIDefinitions` | Buffer/Texture 创建描述、格式、使用方式、状态、Blend/Depth/Rasterizer/Sampler 抽象 |
| `RHIResources` | `FRHIResource` 及 Buffer、Texture、Shader、PSO 等对象树 |
| `DynamicRHI` | 抽象创建入口；Renderer 不知道当前后端是 DX12 还是别的 API |
| `RHIContext` | 录制命令的抽象接口 |
| `RHICommandList` | 命令流与 Context 的职责边界 |

### 验收

```text
Renderer/Test 层不依赖 D3D12；
RHI 层不包含 d3d12.h；
理论上可以增加 Vulkan 后端而不修改 Renderer。
```

---

## A3：DX12RHI 后端

对应 `Core/DX12RHI/`。

```text
Core/DX12RHI/
├─ D3D12DynamicRHI
├─ D3D12Adapter
├─ D3D12Device / Queue
├─ D3D12Resource / ResourceLocation
├─ D3D12DescriptorHeap / View
├─ D3D12RootSignature / PipelineState
├─ D3D12CommandContext / CommandList
└─ D3D12Submission
```

### 按垂直切片推进

| 里程碑 | 最小能力 |
|---|---|
| M1 | RHI 初始化 → Clear → Present |
| M2 | RHI 创建 Buffer / Texture / SRV / RTV / DSV |
| M3 | Renderer/Test 只调 RHI，画出三角形或静态 Mesh |
| M4 | 状态追踪、自动 Barrier、描述符管理、多帧同步、延迟释放 |
| M5 | 动态常量数据 / ring buffer；未来再接 RHIThread |

### 验收

```text
自己的 RHI API 能稳定渲染：
- 静态 Mesh
- 常量缓冲
- 纹理与采样器
- 多帧同步
- 正确资源状态转换
- Fence 保护下的延迟释放
```

---

## A4：最小 Renderer

RHI 完成“怎么画”；Renderer 完成“画什么”。

```text
Core/Renderer/
├─ Scene
├─ Camera
├─ StaticMesh
├─ Material
├─ Shader
├─ MeshPass
└─ RenderTarget
```

### 最小帧主线

```text
Game / Scene 数据
    ↓
收集 Visible Render Items
    ↓
按 Pass / PSO / Material / Mesh 分类或排序
    ↓
写入常量数据与资源绑定
    ↓
RHI Draw
    ↓
Present
```

### 与 LitWaves 的映射

```text
LitWaves RenderItem
    ↓
最小 Renderer 的 Render Item

Geo + Material + ObjectCBIndex
    ↓
Mesh + Material + Per-Object Parameters

DrawRenderItems
    ↓
Mesh Pass Submission
```

### 验收

```text
Scene 可包含多个 Mesh / Material / Camera / Directional Light；
Renderer 不直接调用 D3D12；
可绘制 opaque、纹理材质，后续再添加 transparent。
```

---

## A5：现代实时渲染能力

建议先完整走通 Forward，再按目的扩展。

### Forward 基础

```text
Directional / Point / Spot Lights
Shadow Map
Normal Map
Skybox / IBL 基础
HDR
Tone Mapping
Bloom
透明渲染与排序
```

### 现代路径

| 方向 | 学习价值 |
|---|---|
| Deferred | GBuffer、多 Render Target、全屏光照、带宽权衡 |
| Forward+ / Clustered | 光源剔除、Compute、透明友好 |
| Compute / GPU-driven | UAV、Indirect Draw、Hi-Z、GPU Culling、粒子 |

建议：先做 **Deferred**，更利于理解 UE 的传统帧结构：

```text
Depth / PrePass
→ BasePass / GBuffer
→ Lighting
→ Translucency
→ Post Process
→ Present
```

---

## A6：最小 Render Graph → UE RDG

RDG 的问题背景：大量 Pass 若各自手写资源创建、依赖、Barrier、生命周期与释放，复杂度会失控。

### 自己的最小 Render Graph 只做四件事

```text
1. RegisterExternalResource
2. CreateTransientTexture / Buffer
3. AddPass(ReadResources, WriteResources, Execute)
4. Compile + Execute
```

### 目标

```text
GBuffer Pass 写 GBuffer
Lighting Pass 读 GBuffer、写 SceneColor
ToneMap Pass 读 SceneColor、写 BackBuffer

→ 系统自动推导依赖、顺序与必要 Barrier。
```

之后再对照 UE：

```text
Runtime/RenderCore/
├─ RenderGraphBuilder.*
├─ RenderGraphResources.*
└─ RenderGraphUtils.*
```

---

## A7：UE 定向深读与高级系统

### 建议源码阅读顺序

```text
Runtime/RHI
    ↓
Runtime/RenderCore
    ↓
Runtime/D3D12RHI
    ↓
Runtime/Renderer
    ↓
Runtime/Engine 的 Scene / Material 接口
```

不要一开始从 `DeferredShadingRenderer.cpp` 深入，否则会失去类型与资源层的映射。

### 高级内容放在基础稳定之后

```text
GPU Scene
PSO Cache / Pipeline Library
Shader permutation / Shader Compile Worker / DDC
Descriptor Cache / Bindless
RHIThread / 并行 Command Recording
Nanite
Lumen
Virtual Shadow Maps
Virtual Textures
TSR
Residency / Multi-GPU
```

---

# Part B：UE 使用 / 观察线（只聚焦渲染）

> 目标不是泛学蓝图、网络、AI 或 UI；目标是让每个渲染能力都能被使用、观察、抓帧、定位。

```text
U0 工具与项目认知
 ↓
U1 场景、坐标、相机、资产
 ↓
U2 材质、纹理、Shader
 ↓
U3 光照、阴影、曝光
 ↓
U4 渲染路径、几何与可见性
 ↓
U5 后处理与屏幕空间效果
 ↓
U6 性能分析、RenderDoc / PIX
 ↓
U7 C++ / Plugin 渲染扩展点
```

---

## U0：编辑器与渲染调试入口

### 学习内容

```text
Project Settings / Rendering
Scalability / 平台 RHI 选择
World Outliner / Details / Content Browser
Level / Actor / Component / Transform
Viewport View Mode
Console Variables（CVar）/ Output Log
Show Flags
PIE / Standalone / Packaged 的差异
```

### 必须掌握的 View Mode

```text
Lit
Unlit
Wireframe
Detail Lighting
Lighting Only
Reflections
Shader Complexity
Quad Overdraw
Buffer Visualization
```

### 验收：建立固定实验关卡 `RenderingLab`

```text
Camera
Directional Light
Sky Light
Sky Atmosphere
Plane
Cube
Sphere
Post Process Volume
```

之后每个实验都基于这张固定关卡，保证结果可比较。

---

## U1：场景、坐标、相机与资产

### 学习内容

```text
UE 左手坐标系与厘米单位
Actor / SceneComponent：局部 Transform 与世界 Transform
Static Mesh / Skeletal Mesh 基础区别
Static Mesh Editor：LOD、UV Channel、法线/切线、Nanite 状态
Camera：FOV、曝光、焦距与景深
LOD、HLOD、视距
```

### 必做实验

```text
- 非均匀缩放对法线、阴影和材质的影响
- UV0 / UV1 的用途
- 相机移动时依赖 View / Proj 的效果如何变化
- LOD 切换造成的视觉与性能变化
```

### 与底层映射

| UE 使用层 | DX12 / RHI 对应 |
|---|---|
| Actor Transform | `World` / Object Constants |
| Camera | `View`、`Proj` / Pass Constants |
| Static Mesh | Vertex/Index Buffer + Input Layout |
| LOD | 多份 Geometry / Index Buffer 的选择 |
| Component 层级 | Local-to-World 矩阵组合 |

---

## U2：材质、纹理与 Shader

### Material Editor 基础

```text
Material Domain
Blend Mode
Shading Model
Base Color / Metallic / Roughness / Specular / Normal / Emissive
Opacity / Opacity Mask / Two Sided
Texture Sample / Sampler Source
UV / Texture Coordinate / Panner / Custom Rotator
Material Instance 与参数化
```

### 最小实验集

每个实验只改变一个因素：

```text
1. 纯 Base Color
2. Metallic = 0 与 1 对照
3. Roughness 阶梯
4. 接 / 不接 Normal Map
5. Emissive 强度与 Bloom / Exposure
6. Opaque、Masked、Translucent 对照
```

### 材质到 GPU 的理解链

```text
Material Graph
→ Material Compile
→ Shader permutation
→ Material Instance 参数
→ Uniform Buffer / Texture / Sampler
→ PSO + Draw
```

### 必做观察

```text
改 Scalar Parameter：通常只变参数，不必重编 shader
改 Static Switch Parameter：会改变 permutation，通常触发 shader 编译
改 Texture：资源绑定变化，不等于 shader 逻辑变化
```

---

## U3：光照、阴影、曝光与颜色管理

### 推荐顺序

```text
Directional Light
→ Point Light
→ Spot Light
→ Sky Light
→ 环境反射基础
→ Exposure
→ HDR / Tone Mapping
→ Shadow 类型与质量
→ Lumen / 硬件光追（最后）
```

### 对应 Chapter 8 的概念

```text
Directional / Point / Spot Light
CalcAttenuation
Blinn-Phong
Object / Material / Pass 常量数据
```

### 必做实验

```text
- 固定场景，单独调整光源角度、强度、半径、Spot 内外锥角
- 对比 Emissive = 1 与 Emissive = 100 在 HDR + Exposure + Tone Mapping 下的表现
- 研究 Shadow Bias / Slope Bias / Contact Shadow
- 对比 Static / Stationary / Movable 物体与光源组合
- 对比 CSM 与 Virtual Shadow Maps（UE5）
```

### 要能回答

```text
阴影 acne、peter-panning、远距离阴影质量差、阴影闪烁，分别可能来自哪里？
```

---

## U4：渲染路径、几何系统与可见性

### 学习内容

```text
Deferred 与 Forward Shading 的取舍
GBuffer
Depth Prepass
Base Pass
Translucency Pass
Post Process
Nanite 基础
LOD / HLOD
Frustum Culling / Occlusion Culling
```

### 推荐实验

```text
实验 A：Deferred 与 Forward
- 相同场景、多个 Point Light
- 比较材质限制、MSAA、透明能力、GPU 时间

实验 B：Nanite
- 同一高面数 Static Mesh，切换 Nanite
- 观察 Wireframe、GPU Profiler、LOD 表现

实验 C：可见性
- 大量复制 Mesh
- 改变相机和遮挡关系
- 观察 draw 数、primitives 与 GPU 时间
```

> 先理解传统帧结构，再研究 Nanite 改变了几何剔除与提交链路的哪一部分。

---

## U5：后处理与屏幕空间效果

### 推荐顺序

```text
Post Process Volume
→ Bloom
→ Auto Exposure
→ Tone Mapping
→ Ambient Occlusion
→ Screen Space Reflections
→ Motion Blur
→ Depth of Field
→ TAA / TSR
→ Volumetric Fog
```

### 要建立的资源直觉

```text
SceneColor
SceneDepth
GBuffer
Velocity
AO
History Texture
```

例如 TAA / TSR 的核心数据关系：

```text
Current Frame Color
+ Previous Frame History
+ Motion Vector / Velocity
+ Depth
→ temporal anti-aliased output
```

这就是未来 RDG 中跨帧资源、Pass 依赖、读写关系的真实案例。

---

## U6：性能分析、GPU Capture 与源码映射

### 工具学习顺序

```text
第一梯队：
- stat unit
- stat gpu / ProfileGPU
- Unreal Insights（基础）
- Shader Complexity / Quad Overdraw / Buffer Visualization

第二梯队：
- RenderDoc
- PIX for Windows（DX12 重点）
- GPU Visualizer / RDG Events

第三梯队：
- GPU event ↔ RDG event ↔ Renderer 源码 ↔ RHI 命令映射
```

### 每个工具回答的问题

| 工具 | 要回答的问题 |
|---|---|
| `stat unit` | 瓶颈在 Game、Draw 还是 GPU？ |
| `ProfileGPU` | 哪个 GPU Pass 最慢？ |
| Shader Complexity | 是 shader 太重，还是 overdraw？ |
| Quad Overdraw | 透明、粒子、植被是否过度覆盖像素？ |
| Buffer Visualization | Depth / GBuffer / Velocity / AO 是否正确？ |
| RenderDoc / PIX | 一个 draw 的 shader、PSO、资源、descriptor、barrier 是什么？ |

### 核心练习：关功能 → 抓帧 → 对比 → 回源码

以 SSAO 为例：

```text
1. SSAO 开启，抓一帧。
2. SSAO 关闭，抓一帧。
3. 对比 GPU pass 列表。
4. 对比新增/删除的中间资源。
5. 对比 SceneColor / AO 输出。
6. 用 event/pass 名回查 UE 源码。
```

这条流程适用于 Shadow、Bloom、SSR、TAA/TSR、Lumen 等任何功能。

---

## U7：通过 C++ / Plugin 进入 UE 渲染扩展点

### 推荐顺序

```text
1. Gameplay / Actor C++：理解 GameThread 数据来源
2. SceneProxy：把自定义几何带入 RenderThread
3. RenderThread Command：理解线程边界
4. View Extension：扩展 View 相关行为
5. Global Shader：编写自己的 UE Shader
6. RDG Pass：注册自己的 Compute 或 Raster Pass
7. Custom Mesh Pass：最后再进入
```

不要从直接修改 `DeferredShadingRenderer.cpp` 开始。先使用 Plugin/Extension 控制影响范围。

### 推荐阶段项目

一个最小渲染 Plugin：

```text
输入：SceneColor / SceneDepth，或自建 Texture
处理：一个 Global Shader + 一个 RDG Fullscreen/Compute Pass
输出：Sobel 边缘检测、灰度、调试可视化或简单 AO
```

它能将两条线首次完整串联：

```text
UE 使用层效果
→ Global Shader
→ RenderThread
→ RDG 资源与 Pass
→ RHI
→ D3D12 命令
```

---

# Part C：两条线的同步推进方式

不要先学完“使用线”再学“复刻线”，也不要只读源码。围绕同一个主题双向推进。

| 主题 | UE 使用 / 观察线 | 原理 / 源码 / 复刻线 |
|---|---|---|
| 光照 | 调 Directional/Point/Spot、Exposure | LitWaves、Blinn-Phong、CB 分层 |
| 纹理 | Material、Texture、Sampler、Instance | SRV、Descriptor Heap、Sampler State |
| 阴影 | UE Shadow 参数、ProfileGPU、抓帧 | Shadow Map、DSV/SRV、深度比较 |
| 后处理 | PostProcess Volume、Bloom/TAA | HDR RT、Fullscreen Pass、History Buffer |
| 几何 | StaticMesh Editor、LOD、Nanite | VB/IB、Culling、Indirect Draw |
| Render Graph | GPU Visualizer / RDG Event / Capture | 最小 Render Graph、Barrier、Transient 资源 |

每个专题至少建立一张四层映射表：

```text
主题：例如 Shadow Map

1. 数学 / 图形学：
   光源视角投影；比较表面深度与阴影图深度。

2. 裸 DX12：
   创建 Depth Texture → DSV/SRV → Shadow Pass 写深度 → Main Pass 采样比较。

3. 自己的 RHI / Renderer：
   FRHITexture / DSV / SRV / BeginRenderPass / SetPipeline / Draw。

4. UE：
   ShadowDepth Pass / Shadow 资源 / RDG Pass / Renderer 中投影与采样路径。
```

---

# Part D：推荐的实际推进顺序

## 当前：Chapter 8 / LitWaves

```text
1. 完成 LitWaves，稳定理解 FrameResource、Material、Light、动态 VB。
2. 在 UE 创建 LightingLab：
   Directional / Point / Spot / Sky Light
   Exposure / Tone Mapping
   Material Instance 对照
3. 用 Unlit、Lighting Only、Buffer Visualization 区分问题来源。
```

## Chapter 9：纹理、SRV、Sampler

```text
1. 完成裸 DX12 Texture / SRV / Sampler / Descriptor Heap 学习。
2. 在 UE 建 MaterialLab：
   Texture / UV / Sampler
   Metallic / Roughness / Normal
   Opaque / Masked / Translucent
3. 观察 Material Instance、Static Switch、Texture 更换的差异。
```

## 之后：RHI 复刻阶段 0 → M3

```text
1. Core/RHI 抽象层。
2. 单线程 DX12RHI：M1 Clear → M2 Resource/View → M3 Textured Mesh。
3. 此时开始用 RenderDoc / PIX 抓 UE 的最简单 Opaque Mesh draw。
4. 对照：UE Capture 中一个 draw ↔ UE 源码 ↔ 你的 RHI 命令。
```

## Shadow / HDR / PostProcess 基础完成后

```text
1. 观察 UE 的 Depth / GBuffer / Lighting / PostProcess 帧结构。
2. 做最小 Render Graph。
3. 学 RDG 的架构与源码。
4. 写最小 Global Shader + RDG Pass Plugin。
```

## 最后进入 UE5 高阶系统

```text
Nanite
Lumen
Virtual Shadow Maps
TSR
GPU Scene
Bindless / Descriptor 管理
并行渲染与 RHIThread
```

---

# Part E：避免的误区

## 1. 会用编辑器不等于理解 Renderer

每个 UE 实验都必须补充三个问题：

```text
它增加了什么 GPU 工作？
读写了哪些资源？
成本主要在 CPU、带宽、几何还是像素着色？
```

## 2. 不从 Nanite / Lumen / TSR 开始

先吃透：

```text
Mesh → Depth → BasePass/GBuffer → Lighting → SceneColor → ToneMap
```

再理解高级系统替换或增强了这条链路的哪个环节。

## 3. 不只读源码，也不只抓帧

正确目标是走通：

```text
RenderDoc / PIX event
↔ RDG event 名
↔ Renderer pass
↔ RHI command
↔ D3D12 command list
```

---

# 最终里程碑

## 目标 1：DX12RHI 学习版

```text
自己的 RHI API
+ 单线程 DX12 后端
+ 静态 Mesh / CB / Texture / Shader / PSO
+ 稳定多帧与 Fence 生命周期
```

## 目标 2：独立小型渲染器

```text
RHI
+ Scene / Renderer / Material
+ Forward 或 Deferred
+ Shadow / HDR / ToneMap / Postprocess
+ 最小 Render Graph
```

## 目标 3：能够定向读懂并扩展 UE Renderer

```text
理解 RHI ↔ RenderCore ↔ Renderer ↔ RDG 的职责边界；
理解一个 Mesh Draw 如何变成 GPU 命令；
能通过 Capture 将 GPU pass 定位到源码；
能用 Global Shader / RDG Pass 编写受控渲染扩展；
再进入 Nanite、Lumen、VSM、TSR 等工业级系统。
```

---

# Part F：项目矩阵——每个阶段必须有可验证产物

> 不等待一个“完整游戏项目”。通过一组规模受控的 Renderer 实验项目逐层积累；每个项目只验证一个或一组紧密相关的能力。

## F1：三类项目的职责

```text
1. DX12 学习 Demo
   目标：验证 API、数学和 GPU 行为。

2. 自己的 RHI / Renderer
   目标：验证抽象、资源生命周期、Pass 和架构。

3. UE RenderingLab
   目标：验证 UE 功能、GPU Capture 与源码映射。
```

| 项目类别 | 主要问题 |
|---|---|
| `Source/DirectX12` | DX12 到底如何工作？ |
| `Core/RHI + Core/DX12RHI` | UE 为什么需要这样分层和封装？ |
| `UERenderingLab` | UE 这一帧实际有什么 Pass、资源和性能代价？ |

## F2：自己的 Renderer 项目阶梯

| 阶段 | 项目 | 视觉验收 | 架构验收 |
|---|---|---|---|
| DX12 | `Shapes` / `LitWaves` / `TextureLab` | 光照、动态水面、纹理材质 | FrameResource、CB、SRV、Sampler、同步正确 |
| RHI M1 | `RHI Clear` | 清屏并 Present | Test/Renderer 不直接调用 D3D12 |
| RHI M2-M3 | `Textured Mesh Viewer` | 纹理 Mesh、相机、方向光 | Buffer/Texture/View/Shader/PSO 经 RHI 建立 |
| Forward | `Mini Forward Renderer` | Shadow、HDR、Bloom、透明 | Pass、Resource、PSO、Material 分层 |
| Deferred | `Mini Deferred Renderer` | GBuffer、多光源、后处理 | MRT、全屏 Pass、RTV/SRV 切换、Barrier 正确 |
| RDG | `RenderGraph Migration` | 与改造前输出一致 | Pass 读写、依赖、Barrier、Transient 生命周期自动推导 |
| Temporal | `TAA Lab` | 运动场景抗锯齿稳定 | Velocity、History、Jitter、Camera Cut、Resize 处理 |
| GPU-driven | `Visibility Lab` | 大量 Instance 正确剔除 | Compute、UAV、Visible List、Indirect Draw |
| UE 使用线 | `UERenderingLab` | 每项功能可见对照 | Capture 能定位 UE Pass / 资源 / 源码 |

### 项目 1：RHI Clear

```text
能力：
BeginFrame → BeginRenderPass/Clear → EndFrame → Present

验证：
FDynamicRHI、Device、Queue、SwapChain、Fence、Viewport 的职责边界。
```

### 项目 2：Textured Mesh Viewer

作为第一个长期维护的 RHI 验证载体：

```text
画面：
一个 Mesh + 一张 Texture + 可移动 Camera + Directional Light

能力：
FRHIBuffer / FRHITexture
SRV / Sampler
Vertex Declaration / Shader / PSO
Object / Material / Pass 参数绑定
```

不要在这里加入 UI、游戏逻辑或复杂资产管线。

### 项目 3：Mini Forward Renderer

在 Mesh Viewer 上逐步加入：

```text
Directional / Point / Spot Light
Alpha Test / Transparent Pass
Skybox
Shadow Map
HDR SceneColor
Tone Mapping
Bloom
```

可与 UE 的最小场景使用相同模型、贴图、相机和方向光抓帧对照；目标是比较资源链和 Pass 链，而不是要求像素级一致。

### 项目 4：Mini Deferred Renderer

中期核心项目。建议场景规模：

```text
10~30 个 Static Mesh
3~10 个 Material
多个动态光源
环境贴图或 Skybox
```

帧结构：

```text
Depth Prepass（可选）
→ GBuffer Pass
→ Deferred Lighting Pass
→ Sky / Transparent Pass
→ HDR SceneColor
→ Tone Mapping / Post Process
→ Back Buffer
```

完成它后可以建立直接映射：

```text
自己的 GBuffer Pass      ↔ UE BasePass
自己的 Lighting Pass     ↔ UE Deferred Lighting
自己的 HDR SceneColor    ↔ UE SceneColor
自己的 ToneMap Pass      ↔ UE Post Processing
```

### 项目 5：Render Graph Migration

只有当 Renderer 手写 Pass 达到约 5 个以上、资源/Barrier/释放管理开始痛苦时再引入。

```text
先：GBuffer → Lighting → AO → Bloom → ToneMap
后：AddGBufferPass / AddLightingPass / AddSSAOPass / AddBloomPass / AddToneMapPass
```

目标不是先复刻 UE RDG 的全部功能，而是让自己亲自遇到并解决资源依赖和生命周期问题。

### 项目 6：TAA Lab

TAA 是规模可控但覆盖广泛的高级练习：

```text
Current SceneColor
+ Previous History
+ Depth
+ Velocity
+ Camera Jitter
→ TAA Output
```

它覆盖跨帧资源、History Texture、Motion Vector、Camera Cut、Viewport Resize 和全屏 Pass，是理解 TSR、Lumen temporal filter 等系统的共同前置。

### 项目 7：GPU-driven Visibility Lab

在 Nanite / GPU Scene 之前，先实现最小 GPU 可见性链路：

```text
CPU 创建大量 Instance
→ GPU Compute Frustum Culling
→ Visible Instance List
→ Indirect Draw
```

后续按需要加入：

```text
Hi-Z Occlusion Culling
Instance Data Buffer
Indirect Arguments
```

目标不是复刻 Nanite，而是能指出 Nanite 在 GPU Culling/Indirect Draw 基础上额外解决了哪些 cluster、raster、streaming 和可见性问题。

## F3：UE RenderingLab 工程

建立单独 UE 工程 `UERenderingLab`；每一个 Map 是一个可诊断实验，而不是追求最终画面。

```text
Maps/
├─ 00_Baseline
├─ 01_MaterialLab
├─ 02_LightingLab
├─ 03_ShadowLab
├─ 04_TransparencyLab
├─ 05_PostProcessLab
├─ 06_DeferredLab
├─ 07_NaniteLab
├─ 08_LumenLab
├─ 09_VSMLab
└─ 10_TemporalLab
```

### 固定标准资产

```text
Plane / Cube / Sphere
高频棋盘格贴图、低频渐变贴图、Normal Map
高面数与低面数模型
透明卡片 / Foliage
强 Emissive 物体
移动物体
遮挡墙
可反射的金属球
不同 Roughness 的球
```

| 实验主题 | 关键测试物 |
|---|---|
| Texture / Mip / UV | 高频棋盘格、渐变贴图 |
| PBR | 金属球、粗糙度球 |
| Normal Map | 斜面 Plane 或球 |
| Transparency / Overdraw | 透明卡片、Foliage |
| Shadow Bias | Plane、Cube、掠射角光源 |
| Nanite / LOD | 高低面数模型 |
| Occlusion | 遮挡墙、大量 Instance |
| TAA / TSR | 快速移动物体、高频纹理 |
| Lumen / GI | 室内盒子、彩色墙面 |
| VSM | 大尺度场景、近距离细节物体 |

## F4：高阶 UE 系统的前置项目

高阶系统不以“完整复刻 UE 功能”为目标，而以建立可解释的前置实现为目标。

| UE 系统 | 不直接复刻 | 先完成的前置项目 |
|---|---|---|
| Nanite | 虚拟几何完整系统 | GPU Culling + Indirect Draw + Cluster 基础 |
| Lumen | 完整动态全局光照系统 | Shadow Map → SSAO → SSR → 简单 SDF/Probe GI |
| VSM | 虚拟页表阴影系统 | Cascaded Shadow Map / Atlas / Shadow Cache |
| TSR | UE 的复杂超分重建 | TAA + Velocity + History + Jitter |
| GPU Scene | 完整 GPU Primitive 数据库 | Instance Buffer + Compute Culling |
| RDG | UE 全量 Render Graph | Read/Write 声明 + Barrier 推导 + Transient 生命周期 |
| Material System | 材质图和全量 permutation | 参数材质 + Texture/Sampler + 少量 Static Switch |
| Shader DDC | 分布式编译与缓存管线 | Shader Key + 本地 Bytecode Cache |

高阶学习的进度标准：

```text
不是：我是否复刻了 UE 的 Nanite / Lumen？

而是：我能否说清它相对于自己的前置实现多解决了哪些问题，
以及为什么它需要额外的资源、Pass、数据结构和同步系统？
```
