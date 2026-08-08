## 渲染学习路线（2026-07-16 制定，2026-07-16 第二次修订）

### 当前定位
- ✅ 阶段 1 已完成：了解渲染管线基本流程，写过 shader，能搭基本渲染流程
- 🎯 现在：从「知道怎么做」过渡到「知道为什么」
- 🏢 身处 UE 项目组：有现成的引擎改造案例和团队积累，UE 源码学习可大幅加速
- 🎨 需要掌握 UE 实战技能：材质、粒子、动画、自定义渲染

### 路线总览（三线并行 + 一汇合）

```
                      ┌── 主线 ──┐   ┌── 副线1 ──┐   ┌── 副线2 UE实战 ──┐
7月下~8月             DX12 基础    组内UE改造对照     材质系统
                     管线/PSO      RHI/Shader模块     Material Editor
                     纹理/光照                        Functions/Layers
                          │              │                  │
8月~9月              DX12 Shadow   阴影/Deferred对照   粒子系统
                     PBR/Deferred  管线/PBR对比        Niagara基础
                                                       GPU粒子/数据接口
                          │              │                  │
9月~10月             DX12 TAA      渲染管线对照        动画系统
                     AO/后处理     PostProcess        AnimBP/骨骼蒙皮
                                                       Control Rig
                          │              │                  │
                          └──────────────┼──────────────────┘
                                         ▼
10月~11月           Mini渲染器整合 ── 深度对照UE ── 自定义渲染入门
                                        模块        SceneViewExtension
                                                   Custom Shading Model
                          │              │                  │
11月~12月            GPU Gems       UE源码系统通读    高级自定义渲染
                     专题深挖       Lumen/管线       自定义Pass/改造管线
                          │              │                  │
                          └──────────────┼──────────────────┘
                                         ▼
                          2026年12月底 ~ 2027年1月
                          四条线闭环：理论+底层+引擎+实战 全线贯通
```

### 分阶段时间线

#### 第 1 段：7月中旬 ~ 8月底（约 1.5 个月）

**主线 — DX12 + RTR 基础：**

| 阶段 | DX12 进度 | RTR 重点章节 | 产出 |
|------|----------|-------------|------|
| 7月下 | 初始化管线、三角形、CBV、纹理 | Ch 2 管线、Ch 3 GPU 架构、Ch 6 纹理 | 理解 PSO / Root Signature / Resource Barrier |
| 8月 | Shadow Map、光照 | Ch 7 阴影、Ch 10 光照 | PCF Shadow + Blinn-Phong，能跑的 demo |

**副线 1 — 组内 UE 改造对照：**
- 组里改过什么模块，当周就去读那一小块源码
- 组里有人能解释改动的理由和设计意图

**副线 2 — UE 材质系统（7月下 ~ 8月）：**
- Material Editor：节点连图、常用节点（lerp/multiply/saturate/Fresnel 等）
- Material Functions：封装可复用材质逻辑
- Material Layers：地形/角色材质的分层架构
- Material Instances：静态/动态实例的差异和性能
- 关键概念：Material 编译后的 HLSL 长什么样（材质→Shader 的翻译过程）
- 产出：能从零搭建一个完整的 PBR 材质，理解每个 pin 的数据流

> 材质阶段的目标不是记住所有节点，而是建立「材质节点 → 最终 HLSL」的直觉。
> 这对后面自定义渲染至关重要——自定义 Shading Model 本质上就是写一个 Materials 都用不上的新 pin 和新计算。

---

#### 第 2 段：8月底 ~ 9月底（约 1 个月）

**主线 — DX12 + RTR 进阶：**
- PBR（Ch 9 PBR）→ Cook-Torrance 完整管线
- Deferred Rendering（Ch 20 Deferred）→ G-Buffer 布局和管线切换
- 产出：G-Buffer 可视化的 PBR 渲染器

**副线 1 — UE 对应模块对照：**
- GBuffer / ShadingModels 源码结构
- Deferred Rendering 的实际流程

**副线 2 — UE 粒子系统：**
- Niagara 基础：Emitter / System / Module 三层结构
- Particle Spawn / Update 阶段的数据流
- GPU Particles：哪些计算跑在 GPU，数据怎么传到 shader
- Niagara 与 Materials 的交互（Particle Color / Texture Sample）
- 高级话题（了解即可）：自定义 Niagara Module、Data Interface
- 产出：能做天气效果（雨/雪）、爆炸、环境粒子

---

#### 第 3 段：9月底 ~ 10月底（约 1 个月）

**主线 — DX12 + RTR 收尾：**
- SSAO（Ch 11 全局光照）
- TAA（Ch 5 抗锯齿）
- Bloom + Tone Mapping（Ch 8 光与色）
- 产出：一个完整的前向+延迟混合渲染器

**副线 1 — PostProcess 对照：**
- UE TemporalAA / TSR
- PostProcess Tonemap / Bloom

**副线 2 — UE 动画系统：**
- Animation Blueprint：状态机、Blend Space、Layered Blend Per Bone
- Skeletal Mesh 渲染：骨骼矩阵怎么传到 GPU（Bone Buffer）
- Control Rig：程序化动画，IK/FK
- 动画与渲染的交叉：Skinning 在 GPU 上怎么算的
- 产出：能搭角色动画逻辑，理解动画数据从 CPU 到 GPU 的路径

---

#### 第 4 段：10月底 ~ 11月底（约 1 个月）← 四线汇合

**主线 — Mini 渲染器整合：**
- 把之前分散的 feature 拼成完整 demo
- 加载 Sponza 或类似场景
- 跑通完整 PBR + Shadow + AO + TAA + 后处理管线

**副线 1 — 深度对照 UE 模块：**
- 渲染器做到哪个模块，对照 UE 对应模块
- 开始建「UE 渲染模块速查表」：哪个类管哪个功能

**副线 2 — 自定义渲染入门：**
- SceneViewExtension：注入自定义 Pass 的入口
- Custom Shading Model：在 UE 的 GBuffer/Shading 管线里加新 shader
- Custom Node in Materials：在材质里直接写 HLSL
- Mesh Material Override / Render CustomDepth Pass
- 产出：能在 UE 里做一个简单的自定义渲染效果（如轮廓线描边、自定义半透明排序、简单后处理）

> 自定义渲染是材质+粒子+动画+DX12 四者的交汇点：
> - 材质 → 你知道 Material Expression 怎么翻译到 HLSL，就能自定义节点
> - 粒子 → 你知道 Niagara GPU 粒子怎么进渲染管线，就能优化
> - 动画 → 你知道 Bone Buffer 格式，就能做 GPU Skinning 改造
> - DX12 → 你知道 Resource Barrier / PSO / Descriptor 的含义，UE RHI 就不再是黑盒

---

#### 第 5 段：11月底 ~ 12月底（约 1 个月）

**GPU Gems 专题深挖 + UE 源码系统通读 + 高级自定义渲染：**

按组里实际项目需求，选 1-2 个方向精进：

| 方向 | GPU Gems 参考 | UE 源码对应 | 自定义渲染延伸 |
|------|-------------|------------|--------------|
| 阴影质量 | GPU Gems 2 Ch2 | Virtual Shadow Maps | 自定义 Shadow Pass |
| 全局光照 | GPU Gems 3 Ch13 | Lumen | 自定义 GI 注入 |
| 体积效果 | GPU Gems 3 Ch23 | Volumetric Fog | 自定义 Volumetric 材质 |

- UE 管线系统通读：FDeferredShadingSceneRenderer → ShadingPath → PostProcess
- 自定义 Pass / 改造管线：理解 UE 渲染管线的扩展点
- 借组内改造案例理解架构设计

---

#### 第 6 段：2027年1月

**闭环收尾：**
- 四条线知识串联梳理
- 输出个人总结（可选分享给组内）
- 后续方向按项目需求自由深挖

---

### 为什么材质/粒子/动画现在就能学

```
                      DX12 学习曲线         UE 实战学习曲线
                      ╱╲
                     ╱  ╲               ╱╲────── 自定义渲染（需要DX12底子）
                    ╱    ╲             ╱
                   ╱      ╲─── 能写    ╱   ╲─── 粒子（高级）
                  ╱        ╲   自定义  ╱     ╲── 动画
                 ╱          ╲  渲染   ╱       ╲─ 粒子（基础）
                ╱            ╲      ╱───────── 材质
               ╱──────────────╲   ╱
              DX12 ────────── 实战 ──────────────→ 时间
              前3个月爬坡      前3个月就能出活
                                ↑
                           你现在在这，两条线同时进行
```

- 材质、粒子、动画在前 3 个月不需要 DX12 知识，但学到后面自然会问「为什么」
- 自定义渲染是四者交汇点，必须等 DX12 底子够了再深入
- 这四个技能也是你在项目组里最直接的生产力——学了第二天就能用

---

### 三线并行学习对照表

| 月份 | 主线（DX12+RTR） | 副线1（UE源码对照） | 副线2（UE实战） | 三者交叉点 |
|------|-----------------|-------------------|----------------|-----------|
| 7月下~8月 | 管线/纹理/光照 | RHI/Shader模块 | 材质系统 | 材质节点→HLSL 翻译直觉 |
| 8月~9月 | Shadow/PBR/Deferred | GBuffer/ShadingModels | 粒子系统 | GPU 粒子怎么进渲染管线 |
| 9月~10月 | TAA/AO/后处理 | PostProcess | 动画系统 | Bone Buffer → GPU Skinning |
| 10月~11月 | Mini渲染器整合 | 深度对照 | 自定义渲染入门 | SceneViewExtension/CustomNode |
| 11月~12月 | GPU Gems 专题 | Lumen/管线通读 | 高级自定义渲染 | Custom Pass/管线改造 |
| 1月 | ←←← 闭环 →→→ | ←←← 闭环 →→→ | ←←← 闭环 →→→ | 理论+底层+引擎+实战 |

---

### 每周节奏（修订版）

```
周一~三：主线 DX12 代码，实现一个 feature
周四　：翻 RTR 对应章节，理论打底
周五　：UE 实战技能（材质→粒子→动画→自定义渲染，按阶段推进）
　　　 + 读组里改过的 UE 对应模块源码
周末　：重写/优化本周 DX12 代码，对照 UE 实现加深理解
```

---

### 推荐资源（新增部分）

| 类别 | 资源 | 用途 |
|------|------|------|
| DX12 | [Frank Luna - Introduction to 3D Game Programming with DX12] | 主线教材 |
| DX12 | [Microsoft DirectX-Graphics-Samples] | 官方示例 |
| RTR | Real-Time Rendering 4th | 全程词典 |
| UE 材质 | UE 官方文档 Material 章节 + [Ben Cloward YouTube] | 材质入门 |
| UE 粒子 | UE 官方 Niagara 文档 + [CGHOW YouTube] | 粒子入门 |
| UE 动画 | UE 官方 Animation 文档 + Unreal Online Learning | 动画入门 |
| UE 渲染 | UE 源码（Engine/Source/Runtime/Renderer/） | 副线对照 |
| GPU Gems | GPU Gems 全集（NVIDIA 免费） | 进阶专题 |

### 参考书籍及使用策略

| 书名 | 使用方式 | 时机 |
|------|---------|------|
| Frank Luna DX12 | 跟做教材，每周推进 | 7月~10月 |
| Real-Time Rendering 4th | 当词典，做 feature 时查对应章节 | 全程 |
| GPU Gems 系列 | 论文集，按项目需求按专题查 | 11月~12月 |
| UE 渲染源码 | DX12 底子 + RTR 理论 + 组内案例 | 副线全程，12月系统通读 |

---

## 2026-6-8 10:51:36
```
刷题X3-2h.周日复习全部内容2h。
工作内容5h
```

## 算法部分
### 用vs练注意记忆

## UE 部分
### 1.UE GP
### 2.UE physics
### 3.UE render
```
## 周末可以根据当前这一周的md进行复习
```
