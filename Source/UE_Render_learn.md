# UE 渲染体系学习与复刻（UE_Render_learn）

> **目标**：不只是会用 UE 的渲染功能，也不孤立地读源码，而是建立一条可验证的链路：
>
> ```text
> 视觉表现 → UE 编辑器设置 → GPU Pass / 资源 → UE 源码 → 自己的 RHI / Renderer 实现
> ```
>
> 由两条线并行推进、最终汇合：
> - **复刻线（原理 / 源码 / 复刻）**：理解 UE 为什么这样设计，并亲手实现其核心抽象。（主路线图，原 `Plan-Render.md`）
> - **使用 / 观察线**：知道一个功能如何启用、效果和代价是什么、它在 GPU 上实际做了什么。（原 `UE_Rendering_Learning_Roadmap.md`）
>
> 本文件是两线的「总览 / 路线图」。每个阶段的**详细执行文档另开**：阶段 A（RHI）由 `RHI_Learning.md` 承担。

---

## 零、两条线总览

```text
A. 复刻线（原理 / 源码 / 复刻）—— 自底向上，一层不悬空再上一层
   DX12 基础 → Core → RHI/D3D12RHI → RHICore(屏障) → RenderCore → RDG → Renderer → 整合 → UE 高级系统

B. 使用 / 观察线（只聚焦渲染）—— 从外到内
   编辑器 → 场景/资产 → 材质 → 光照/阴影 → 渲染路径/可见性 → 后处理 → 性能分析/抓帧 → C++/Plugin 渲染扩展

           两线围绕同一主题双向推进（见「五、两线并行推进法」），最终汇合于自己的 Mini 渲染器 + 读懂并扩展 UE Renderer。
```

| 学习线 | 核心问题 |
|---|---|
| 复刻线 | UE 的 RHI / RDG / Renderer 为什么分这些层，它们如何最终变成 GPU 命令？ |
| 使用 / 观察线 | 某个 UE 渲染功能如何生效、视觉结果是什么、GPU 成本在哪、如何调试？ |

> **RHI 不是 Renderer。**
> - **RHI**：抽象 GPU 资源与命令，由 D3D12RHI 落地到 DX12。
> - **Renderer**：决定这一帧画什么、哪些物体可见、需要哪些 Pass、用什么 Shader/PSO。
> - **RDG**：组织 Pass、资源依赖、Barrier 和 transient 生命周期。

---

## 一、核心原则：贴合 UE 骨架，不过度合并

- 类的**分层 / 命名 / 文件结构跟 UE 走**，即使单 GPU、单线程也不把多个 UE 类压成一个。
- 简化**只针对内部细节**：接口多版本数组（Device1..12 / Factory2..7）→ 留基础版；多 GPU 数组 → 单个；多线程提交管线（Payload / 对象池 / Timing）→ 去掉。
- 判读标准：不是「把 DX12 画出来」，而是「**用 UE 的类分层 / 命名把它画出来**」。

（详细论证见 `RHI_Learning.md` 开头。）

---

## 二、UE 渲染体系分层地图（复刻的骨架）

```text
┌───────────────────────────────────────────────────────────────┐
│ Engine 层   (Runtime/Engine)   UWorld / 组件 / UMaterial / 网格  │
│   → 把「场景里有什么」喂给渲染器                                  │
├───────────────────────────────────────────────────────────────┤
│ Renderer 层 (Runtime/Renderer) FDeferredShadingSceneRenderer    │
│   → 具体 Pass：GBuffer / 延迟光照 / 阴影 / 后处理 / TAA           │
├───────────────────────────────────────────────────────────────┤
│ RenderCore 层 (Runtime/RenderCore)                              │
│   ├─ RDG    FRDGBuilder / FRDGPass（Pass 依赖图 + 资源生命周期）  │
│   └─ 抽象   FShader / FGlobalShader / FVertexFactory /           │
│             FMeshDrawCommand / FPrimitiveSceneProxy / FSceneView │
├───────────────────────────────────────────────────────────────┤
│ RHICore 层 (Runtime/RHICore)  FRHIComputeCommandList            │
│   → FRHITransition（资源状态 / 屏障自动追踪）                     │
├───────────────────────────────────────────────────────────────┤
│ RHI 层 (Runtime/RHI + D3D12RHI)  FDynamicRHI / FRHICommandList   │
│   → 硬件抽象：Device / Queue / 资源 / 描述符 / PSO / 命令列表     │
└───────────────────────────────────────────────────────────────┘
```

**自底向上复刻，一层不悬空再进下一层。**

---

## 三、复刻线（A 线）：阶段划分

### 阶段依赖（别跳层）

```text
RHI(A) ─▶ RHICore(B) ─▶ RenderCore(C) ─▶ RDG(D) ─▶ Renderer(E) ─▶ 整合(F) ─▶ UE 高级(G)
 地基       命令列表+屏障     shader/网格抽象    Pass 组织     具体管线
```

跳层会悬空：没有 C 的 `FVertexFactory` / `FMeshDrawCommand`，RDG 就「没有可图的 Pass」。

---

### 阶段 A —— RHI 层（进行中）

- 我们目录：`Source/RHI/` + `Source/D3D12RHI/`；UE 对照：`Runtime/RHI/` + `Runtime/D3D12RHI/`
- **执行文档：`RHI_Learning.md`**（第 1~9 章）

**A 内部按里程碑推进（垂直切片）**：

| 里程碑 | 最小能力 | 状态 |
|---|---|---|
| A0 纯 DX12 基础 | SwapChain/Present、CmdQueue/List/Allocator、Fence、VB/IB、PSO/RootSig/Shader、Descriptor、Barrier | 通过 Frank Luna 教材覆盖 |
| A1 Core 基础设施 | 引用计数（`FRHIResource + TRefCountPtr` 风格）、线程/事件/锁、TaskGraph 底座 | 部分（`Core/`） |
| A2 RHI 抽象层 | `RHIDefinitions / RHIResources / DynamicRHI / RHIContext / RHICommandList`，**不含任何 D3D12 类型** | 🔶 A2-a(资源创建)+A2-b片1(命令列表两层，draw loop 零裸D3D12) 通；A2-b 片2/3(PSO/RenderPass/SRV/barrier 创建与绑定抽象) 待做 |
| A3-M1 | RHI 初始化 → Clear → Present | ✅ |
| A3-M2 | 创建 Buffer / Texture / SRV / RTV / DSV | ✅ Buffer/RTV/DSV/Texture/SRV 均已通（棋盘格贴图立方体） |
| A3-M3 | Test 只调 RHI，画出三角形 / 静态 Mesh | ✅（画出转动三角形） |
| A3-M4 | 状态追踪、**自动 Barrier**、描述符管理、**多帧同步**、Fence 保护延迟释放 | 🔶 多帧同步(N分配器+CB ring+每帧fence，去每帧Flush)已通；状态追踪/自动Barrier/延迟释放待做 |
| A3-M5 | 动态常量数据 / ring buffer | ✅ 单 CB→CB ring（每帧一个，随 M4-a 多帧同步一起做） |

**A 章节表**（对照 UE 文件，详见 `RHI_Learning.md`）：

| 章 | 内容 | 我们文件 | UE 参考 |
|---|---|---|---|
| 1 | 设备初始化 | Adapter/Device/Queue/Viewport | D3D12Adapter/Device/Queue |
| 2 | 资源基类 | RHIResources.h | RHIResources.h |
| 3 | Buffer | FD3D12Buffer | D3D12Buffer.cpp |
| 4 | Descriptor Heap | D3D12Descriptors | D3D12Descriptors.h |
| 5 | Shader & PSO | D3D12PipelineState / RootSignature | D3D12PipelineState.h |
| 6 | CommandList | RHICommandList.h | D3D12Commands.cpp |
| 7 | Texture | FD3D12Texture | D3D12Texture.h |
| 8 | 整合画三角形 | — | — |
| 9 | Submission / 中断线程 | — | D3D12Submission.h |

**A1 的三个核心契约（贯穿始终）**：

1. **CPU 所有权与 GPU 生命周期分离**：`CPU 不再持有资源 ≠ GPU 已经不用资源`。资源释放必须受最后一次 GPU Fence 使用值保护，Fence 完成后才真正释放底层 resource / descriptor / upload 分配。
2. **引用计数与所有权**：`FRHIResource + TRefCountPtr` 风格。C++ 对象是否活着，与 GPU 是否仍访问资源，是两个不同的问题。
3. **单线程正确性优先**：先「同一线程录制 + 提交」；等 TaskGraph 成熟再做 RHIThread / 并行提交。

---

### 阶段 B —— RHICore 层（屏障 / 状态自动追踪）

- 复刻 `FRHIComputeCommandList` + `FRHITransition`
- 核心：D3D12 的 `ResourceBarrier` 最易错，UE5 把它抽象成「资源状态机 + 自动插屏障」
- 产出：单线程命令列表 + 资源状态表，自动推导并插入 Barrier（即 A3-M4 的自动 Barrier 部分的正式化）
- UE 对照：`Runtime/RHICore/`

---

### 阶段 C —— RenderCore 层（渲染抽象）

- 复刻 `FShader`/`FGlobalShader`、`FVertexFactory`、`FMeshDrawCommand`、`FPrimitiveSceneProxy`/`FSceneView`、`FRenderResource`
- 核心：把「网格 / 材质 / 着色器」从「硬件资源」里解耦——这是 UE 能挂几百个 Pass 而不乱的原因
- 起步先做**最小 Renderer**：`Scene / Camera / StaticMesh / Material / MeshPass / RenderTarget`；最小帧主线：
  `收集 Visible Items → 按 Pass/PSO/Material/Mesh 分类排序 → 写常量与资源绑定 → RHI Draw → Present`
- UE 对照：`Runtime/RenderCore/`

---

### 阶段 D —— RDG（渲染依赖图）

- 复刻 `FRDGBuilder` / `FRDGPass` / `FRDGTexture`
- 自己的最小 Render Graph 只做四件事：`RegisterExternalResource` / `CreateTransientTexture|Buffer` / `AddPass(Read, Write, Execute)` / `Compile + Execute`
- 目标：`GBuffer Pass 写 GBuffer → Lighting Pass 读 GBuffer 写 SceneColor → ToneMap Pass 读 SceneColor 写 BackBuffer`，系统**自动推导依赖、顺序、必要 Barrier**
- 引入时机：Renderer 手写 Pass 达到 ~5 个、Barrier/生命周期开始痛时
- UE 对照：`Runtime/RenderCore/` 里的 `RenderGraph*`

---

### 阶段 E —— Renderer 层（延迟渲染管线）

- 建议先完整走通 **Forward**，再上 **Deferred**（更利于理解 UE 传统帧结构）
- Forward 基础：Directional/Point/Spot、Shadow Map、Normal Map、Skybox/IBL、HDR、Tone Mapping、Bloom、透明排序
- Deferred 帧结构：`Depth Prepass → BasePass/GBuffer → Lighting → Translucency → PostProcess → Present`
- 复刻 `FDeferredShadingSceneRenderer` 主干
- UE 对照：`Runtime/Renderer/` + `Engine/Shaders/`（.usf / .ush）

---

### 阶段 F —— Mini 渲染器整合

- 把 A~E 拼成能加载场景、跑通完整管线的 demo
- UE 对照：`FScene` + `Runtime/Engine/`

---

### 阶段 G —— UE 定向深读与高级系统

**建议源码阅读顺序**（不要一上来钻 `DeferredShadingRenderer.cpp`）：

```text
Runtime/RHI → Runtime/RenderCore → Runtime/D3D12RHI → Runtime/Renderer → Runtime/Engine 的 Scene/Material 接口
```

**高级系统放在基础稳定之后**（进度标准不是"是否复刻了"，而是"能否说清它相对前置实现多解决了什么问题"）：

| UE 系统 | 不直接复刻 | 先完成的前置项目 |
|---|---|---|
| Nanite | 虚拟几何完整系统 | GPU Culling + Indirect Draw + Cluster 基础 |
| Lumen | 完整动态 GI | Shadow Map → SSAO → SSR → 简单 SDF/Probe GI |
| VSM | 虚拟页表阴影 | Cascaded Shadow Map / Atlas / Shadow Cache |
| TSR | 复杂超分重建 | TAA + Velocity + History + Jitter |
| GPU Scene | 完整 GPU Primitive 库 | Instance Buffer + Compute Culling |
| RDG | 全量 Render Graph | Read/Write 声明 + Barrier 推导 + Transient 生命周期 |
| Material System | 材质图 + 全量 permutation | 参数材质 + Texture/Sampler + 少量 Static Switch |
| Shader DDC | 分布式编译缓存 | Shader Key + 本地字节码缓存 |

其它高级：GPU Scene、PSO Cache/Pipeline Library、Shader permutation/Compile Worker、Descriptor Cache/Bindless、RHIThread/并行录制、Residency/Multi-GPU。

---

## 四、使用 / 观察线（B 线）：只聚焦渲染

> 目标不是泛学蓝图/网络/AI/UI；而是让每个渲染能力都能被**使用、观察、抓帧、定位**。

```text
U0 工具与项目认知     → Project Settings/Rendering、CVar、Show Flags、View Mode（Lit/Unlit/Wireframe/
                        Detail Lighting/Lighting Only/Shader Complexity/Quad Overdraw/Buffer Visualization）
U1 场景/坐标/相机/资产 → 左手系、Actor Transform、Static/Skeletal Mesh、LOD/HLOD、相机 FOV/曝光
U2 材质/纹理/Shader   → Material Domain/Blend/Shading Model、BaseColor/Metallic/Roughness/Normal、
                        Material Instance、材质→Shader 编译链
U3 光照/阴影/曝光      → Directional/Point/Spot/Sky、Exposure、HDR/ToneMap、Shadow 类型/Bias、Lumen（最后）
U4 渲染路径/几何/可见性 → Deferred vs Forward、GBuffer、Depth Prepass、Nanite、Frustum/Occlusion Culling
U5 后处理/屏幕空间     → Bloom/Exposure/ToneMap/AO/SSR/MotionBlur/DOF/TAA·TSR/Volumetric Fog
U6 性能分析/GPU 抓帧   → stat unit/gpu、ProfileGPU、Unreal Insights、RenderDoc/PIX、GPU Visualizer/RDG Events
U7 C++/Plugin 渲染扩展 → Gameplay C++ → SceneProxy → RenderThread Command → View Extension →
                        Global Shader → RDG Pass → Custom Mesh Pass（最后）
```

**固定实验关卡**：建 `UERenderingLab` 工程，每个 Map 是一个可诊断实验（不是追求最终画面）：

```text
Maps/ 00_Baseline 01_MaterialLab 02_LightingLab 03_ShadowLab 04_TransparencyLab
      05_PostProcessLab 06_DeferredLab 07_NaniteLab 08_LumenLab 09_VSMLab 10_TemporalLab
固定资产：Plane/Cube/Sphere、高频棋盘格+渐变+Normal 贴图、高低面数模型、透明卡片/Foliage、
         强 Emissive、移动物体、遮挡墙、金属球、不同 Roughness 球
```

**核心练习：关功能 → 抓帧 → 对比 → 回源码**（以 SSAO 为例：开/关各抓一帧 → 对比 GPU pass 列表 / 中间资源 / 输出 → 用 pass 名回查 UE 源码）。适用于 Shadow/Bloom/SSR/TAA/Lumen 任何功能。

---

## 五、两线并行推进法

不要先学完使用线再学复刻线，也不要只读源码。**围绕同一主题双向推进**：

| 主题 | 使用 / 观察线 | 复刻线 |
|---|---|---|
| 光照 | 调 Directional/Point/Spot、Exposure | Blinn-Phong、CB 分层（Object/Material/Pass） |
| 纹理 | Material、Texture、Sampler、Instance | SRV、Descriptor Heap、Sampler State |
| 阴影 | Shadow 参数、ProfileGPU、抓帧 | Shadow Map、DSV/SRV、深度比较 |
| 后处理 | PostProcess Volume、Bloom/TAA | HDR RT、Fullscreen Pass、History Buffer |
| 几何 | StaticMesh Editor、LOD、Nanite | VB/IB、Culling、Indirect Draw |
| Render Graph | GPU Visualizer / RDG Event / Capture | 最小 Render Graph、Barrier、Transient 资源 |

**每个专题至少建一张四层映射表**（以 Shadow Map 为例）：

```text
1. 数学/图形学：光源视角投影；比较表面深度与阴影图深度
2. 裸 DX12：   建 Depth Texture → DSV/SRV → Shadow Pass 写深度 → Main Pass 采样比较
3. 自己的 RHI/Renderer：FRHITexture / DSV / SRV / BeginRenderPass / SetPipeline / Draw
4. UE：        ShadowDepth Pass / Shadow 资源 / RDG Pass / Renderer 中投影与采样路径
```

---

## 六、项目矩阵：每阶段必有可验证产物

> 不等待一个「完整游戏项目」。用一组规模受控的实验项目逐层积累，每个只验证一组紧密相关的能力。

| 阶段 | 项目 | 视觉验收 | 架构验收 |
|---|---|---|---|
| DX12 | `Shapes`/`LitWaves`/`TextureLab` | 光照、动态水面、纹理材质 | FrameResource、CB、SRV、Sampler、同步正确 |
| RHI M1 | `RHI Clear` | 清屏并 Present | Test 不直接调 D3D12 |
| RHI M2-M3 | `Textured Mesh Viewer` | 纹理 Mesh、相机、方向光 | Buffer/Texture/View/Shader/PSO 经 RHI 建立 |
| Forward | `Mini Forward Renderer` | Shadow、HDR、Bloom、透明 | Pass/Resource/PSO/Material 分层 |
| Deferred | `Mini Deferred Renderer` | GBuffer、多光源、后处理 | MRT、全屏 Pass、RTV/SRV 切换、Barrier 正确 |
| RDG | `RenderGraph Migration` | 与改造前输出一致 | Pass 读写、依赖、Barrier、Transient 自动推导 |
| Temporal | `TAA Lab` | 运动场景抗锯齿稳定 | Velocity、History、Jitter、Camera Cut、Resize |
| GPU-driven | `Visibility Lab` | 大量 Instance 正确剔除 | Compute、UAV、Visible List、Indirect Draw |
| UE 使用线 | `UERenderingLab` | 每项功能可见对照 | Capture 能定位 UE Pass/资源/源码 |

三类项目职责：`Source/DirectX12`（DX12 怎么工作）/ `Source/RHI + D3D12RHI`（UE 为什么这样分层）/ `UERenderingLab`（UE 这一帧有什么 Pass、资源、代价）。

---

## 七、避免的误区

1. **会用编辑器 ≠ 理解 Renderer**。每个 UE 实验补三问：它增加了什么 GPU 工作？读写了哪些资源？成本在 CPU / 带宽 / 几何 / 像素着色？
2. **不从 Nanite/Lumen/TSR 开始**。先吃透 `Mesh → Depth → BasePass/GBuffer → Lighting → SceneColor → ToneMap`，再看高级系统替换/增强了哪个环节。
3. **不只读源码，也不只抓帧**。目标是走通 `RenderDoc/PIX event ↔ RDG event ↔ Renderer pass ↔ RHI command ↔ D3D12 command list`。

---

## 八、最终里程碑

1. **DX12RHI 学习版**：自己的 RHI API + 单线程 DX12 后端 + 静态 Mesh/CB/Texture/Shader/PSO + 稳定多帧与 Fence 生命周期。
2. **独立小型渲染器**：RHI + Scene/Renderer/Material + Forward 或 Deferred + Shadow/HDR/ToneMap/PostProcess + 最小 Render Graph。
3. **能定向读懂并扩展 UE Renderer**：理解 RHI ↔ RenderCore ↔ Renderer ↔ RDG 职责边界；理解一个 Mesh Draw 如何变成 GPU 命令；能用 Capture 把 GPU pass 定位到源码；能写 Global Shader / RDG Pass 扩展；再进入 Nanite/Lumen/VSM/TSR。

---

## 九、与功能学习主线（Frank Luna）的关系

- Frank Luna 教材（主线）= 学「**一个 feature 怎么写**」（PSO/Barrier、阴影、PBR、TAA）。
- 本计划（UE 骨架复刻）= 学「**UE 怎么组织这些 feature**」。
- 两者在**阶段 E / RHI 第 8 章画三角形**处汇合：同一个 PBR/Deferred，用两种结构写，做深度对照。

---

## 附：UE 源码参考路径 & 执行文档

**执行文档**：阶段 A（RHI）→ `RHI_Learning.md`（第 1~9 章，含每章 UE 讲解 + 简化实现 + 踩坑记录）。后续阶段各自另开执行文档。

**UE 源码路径**（版本固定，目录可能变动，以用户告知为准；两处择一）：

```text
RHI 接口层   ：F:\workspace\UnrealEngine58\Engine\Source\Runtime\RHI\
              或 F:\shakervon_engine_merge\Engine\Source\Runtime\RHI\
D3D12 实现层 ：...\Runtime\D3D12RHI\Private\
RHICore     ：...\Runtime\RHICore\
RenderCore  ：...\Runtime\RenderCore\
Renderer    ：...\Runtime\Renderer\
```
