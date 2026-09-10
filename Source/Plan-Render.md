# UE 渲染体系复刻计划（Plan-Render）

> 目标：复刻 UE 的渲染体系，以此深入学习 UE 整个渲染架构。
> 本文件是「总览 / 路线图」；每个阶段的详细执行文档另开（阶段 A 由 `RHI_Learning.md` 承担）。

## 一、核心原则（不变）

贴合 UE 骨架，不过度合并：

- 类的分层 / 命名 / 文件结构**跟 UE 走**，即使单 GPU、单线程也不把多个 UE 类压成一个。
- 简化**只针对内部细节**：接口多版本数组 → 留基础版；多 GPU 数组 → 单个；多线程提交管线 → 去掉。
- 判读标准：不是「把 DX12 画出来」，而是「**用 UE 的类分层 / 命名把它画出来**」。

（详细论证见 `RHI_Learning.md` 开头）

## 二、UE 渲染体系分层地图（复刻的骨架）

```
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

自底向上复刻，**一层不悬空再进下一层**。

## 三、阶段划分

### 阶段 A —— RHI 层（进行中）
- 我们目录：`Source/RHI/` + `Source/D3D12RHI/`
- UE 对照：`Runtime/RHI/` + `Runtime/D3D12RHI/`
- 执行文档：**`RHI_Learning.md`**（第 1~9 章）

| 章 | 内容 | 我们文件 | UE 参考 |
|---|---|---|---|
| 1 | 设备初始化 | Adapter/Device/Queue/Viewport | D3D12Adapter/Device/Queue |
| 2 | 资源基类 | RHIResources.h | RHIResources.h |
| 3 | Buffer | FD3D12Buffer | D3D12Buffer.cpp |
| 4 | Descriptor Heap | D3D12Descriptors | D3D12Descriptors.h |
| 5 | Shader & PSO | D3D12PipelineState | D3D12PipelineState.h |
| 6 | CommandList | RHICommandList.h | D3D12Commands.cpp |
| 7 | Texture | FD3D12Texture | D3D12Texture.h |
| 8 | 整合画三角形 | — | — |
| 9 | Submission/中断线程 | — | D3D12Submission.h |

### 阶段 B —— RHICore 层（过渡 / 屏障）
- 复刻 `FRHIComputeCommandList` + `FRHITransition`
- 核心：D3D12 的 `ResourceBarrier` 最易错，UE5 把它抽象成「资源状态机 + 自动插屏障」
- 产出：单线程命令列表 + 资源状态表，自动推导并插入 Barrier
- UE 对照：`Runtime/RHICore/`

### 阶段 C —— RenderCore 层（渲染抽象）
- 复刻 `FShader`/`FGlobalShader`、`FVertexFactory`、`FMeshDrawCommand`、`FPrimitiveSceneProxy`/`FSceneView`、`FRenderResource`
- 核心：把「网格 / 材质 / 着色器」从「硬件资源」里解耦，这是 UE 能挂几百个 Pass 而不乱的原因
- UE 对照：`Runtime/RenderCore/`

### 阶段 D —— RDG（渲染依赖图）
- 复刻 `FRDGBuilder` / `FRDGPass` / `FRDGTexture`
- 核心：声明式 Pass + 资源依赖图 + 自动生命周期 / 屏障——**理解「UE 每一帧怎么组织渲染」的钥匙**
- UE 对照：`Runtime/RenderCore/` 里的 `RenderGraph*`

### 阶段 E —— Renderer 层（延迟渲染管线）
- 复刻 `FDeferredShadingSceneRenderer` 主干：GBuffer → Deferred Lighting → Shadow → PostProcess（TAA/Bloom/ToneMapping）
- 接上功能主线：Shadow / PBR / Deferred、TAA / AO
- UE 对照：`Runtime/Renderer/` + `Engine/Shaders/`（.usf / .ush）

### 阶段 F —— Mini 渲染器整合
- 把 A~E 拼成能加载场景、跑通完整管线的 demo
- UE 对照：`FScene` + `Runtime/Engine/`

## 四、阶段依赖（别跳层）

```
RHI(A) ──▶ RHICore(B) ──▶ RenderCore(C) ──▶ RDG(D) ──▶ Renderer(E) ──▶ 整合(F)
  地基        命令列表+屏障       shader/网格抽象      Pass 组织      具体管线
```

跳层会悬空：没有 C 的 `FVertexFactory`/`FMeshDrawCommand`，RDG 就「没有可图的 Pass」。

## 五、与功能学习（Frank Luna 主线）的关系

- Frank Luna 教材（主线）= 学「**一个 feature 怎么写**」（PSO / Barrier、阴影、PBR、TAA）
- 本计划（UE 骨架复刻）= 学「**UE 怎么组织这些 feature**」
- 两者在**阶段 E / 第 8 章画三角形**处汇合：同一个 PBR / Deferred，用两种结构写，做深度对照

---

## 附：UE 源码参考路径（版本固定，目录可能变动，以用户告知为准）

- RHI 接口层：`F:\workspace\UnrealEngine58\Engine\Source\Runtime\RHI\`
- D3D12 实现层：`F:\workspace\UnrealEngine58\Engine\Source\Runtime\D3D12RHI\Private\`
- RHICore：`F:\workspace\UnrealEngine58\Engine\Source\Runtime\RHICore\`
- RenderCore：`F:\workspace\UnrealEngine58\Engine\Source\Runtime\RenderCore\`
- Renderer：`F:\workspace\UnrealEngine58\Engine\Source\Runtime\Renderer\`
