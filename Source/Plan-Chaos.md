# Chaos 物理系统学习路线（2026-08-15 制定）

> 与 `Plan.md`（渲染主线）并行的一条新主线。沿用同一套方法论：**UE 源码为教材 + 亲手实现验证 + 算法对照**。
> 学习载体：`F:\workspace\UnrealEngine58\Engine\Source\Runtime\Experimental\Chaos\`（本地 UE 源码）

## 当前定位

- 🎯 目标深度：**深入算法**——不仅会用，还要把求解器/碰撞算法逐行搞懂
- 🛠️ 验证手段：在 BaseTestEngine 里写一个 **mini 物理引擎**（CPU 端），亲手实现每个阶段学到的算法
- 🏢 身处 UE 项目组：Chaos 是 UE5 自研物理，读懂它 = 能看懂项目里任何物理相关改造
- ✅ 已有基础：C++20、DX12/渲染管线（并行中）、UE 引擎源码阅读经验

### 为什么选 Chaos（而不是继续用 PhysX 思维）

- UE5 全面接管物理（刚体默认、布料/破坏/流体原生）——**未来项目的物理改造都发生在这**
- 破坏系统（cluster 碎裂）是 PhysX 时代做不到的
- 架构现代化：handle 粒子 + 任务图多线程 + CVD 可视化——学习价值极高
- 自研系统 = 源码就是唯一权威文档，适合你"源码对照"的学习法

---

## 路线总览（三线并行 + 一汇合）

```
                        ┌── 主线：原理算法 ──┐   ┌── 副线：UE 实战 ──┐
8月（阶段0-1）           物理基础 + 刚体动力学      CVD + 官方文档 + demo
                        F=ma/积分/冲量            Chaos Solver Actor 堆叠实验
                              │                       │
9月（阶段2）            碰撞检测                    Physics Asset / Physical Material
                        隐式几何/宽相/窄相/Contact   用 CVD 观察碰撞点
                              │                       │
10月（阶段3）★核心      求解器                     对照 PhysX 理解"为什么换"
                        顺序冲量/约束图/Island/CCD   项目里实际物理场景实验
                              │                       │
11月（阶段4）            进阶专题                    Chaos Destruction / Cloth 实操
                        XPBD布料/破坏cluster/Field
                              │                       │
                              └───────────┬───────────┘
                                          ▼
                        mini 物理引擎（BaseTestEngine/Physics）
                        与渲染主线汇合：物理结果 → 渲染可视化
```

### 与渲染主线的并行关系

```
主线A：DX12/RTR（Plan.md）──── 物理可视化需要渲染器 ────┐
                                                        ├── 最终：mini 引擎 + 渲染器
主线C：Chaos 物理（本计划）── mini 物理引擎（CPU）───────┘
```

- 阶段 2 之后，物理结果（刚体位置/碰撞点）可以画进 DX12 渲染器做线框可视化——两条线自然交汇
- 渲染线学到 Deferred/实例化后，可以渲染大量物理粒子——GPU 粒子 × 物理的经典组合

---

## 分阶段时间线

### 阶段 0：先玩起来（第 1 周）🎮 ← 当前

**目标：感性认识 + CVD 会用，不读深源码**

- [ ] 在 UE 里搭一个 Chaos demo：Chaos Solver Actor + 几个盒子堆叠，跑起来
- [ ] 装/连 **ChaosVisualDebugger（CVD）**，观察：
  - 刚体的速度/角速度向量
  - 碰撞点（contact point）和法线
  - 休眠（sleeping）状态变色
- [ ] 读 UE 官方文档 Chaos Physics 总览章节
- [ ] 产出：能用自己的话说出"一个刚体从 spawn 到落地，这一帧经历了哪些阶段"

> CVD 是 Chaos 的杀手级调试工具，能直接看到求解器内部状态。**先看到，再理解**，后面读源码时脑子里有画面。

### 阶段 1：物理基础 + 刚体数学（第 2~3 周）

| 主题 | 理论 | Chaos 源码入口 | mini 引擎 milestone |
|---|---|---|---|
| 刚体动力学 | F=ma、半隐式欧拉积分、角速度/角动量、惯性张量 | `Math/`（FVec3、FQuat、FMatrix33） | M1：球自由落体 + 地面弹跳（无旋转） |
| 碰撞响应 | 冲量、恢复系数、库仑摩擦 | `Framework/`（FChaosPhysicsMaterial） | M1+：带摩擦滑动 |
| 粒子机制 | ParticleHandle / SOA 数据布局 | `Particle/`（FGeometryParticleHandle、FPBDRigidParticles） | 理解"为什么用 handle" |

- 产出：一个球在平面上弹跳、滑动，参数（恢复系数/摩擦）可调

### 阶段 2：碰撞检测（第 4~6 周）

| 主题 | 理论 | Chaos 源码入口 | mini 引擎 milestone |
|---|---|---|---|
| 隐式几何 | FImplicitObject 抽象（为什么用隐式几何/SDF 风格） | `Geometry/`（TBox、TSphere、FConvex、FHeightField） | M2：球-球、球-平面、盒-盒 |
| 宽相 | AABBTree / BVH、dirty 网格 | `Collision/`（AABBTree.h、BoundingVolumeHierarchy.h） | M2+：宽相剔除 |
| 窄相 | primitive 对测试、凸-凸算法（去源码里确认 Chaos 用 GJK 还是 MPR） | `Collision/`（CollisionResolution.h、CollisionResolutionUtil.h） | 逐个 primitive 对实现 |
| Contact | FContactPoint、manifold、ContactModification | `Collision/`（ContactModification.h） | contact 生成与持久化 |

- 产出：多个刚体互撞 + 堆叠（初步），碰撞点可视化

### 阶段 3：求解器 ★ 核心（第 7~9 周）

| 主题 | 理论 | Chaos 源码入口 | mini 引擎 milestone |
|---|---|---|---|
| 速度求解 | **顺序冲量**（sequential impulse）、约束图、速度级 vs 位置级 | `Evolution/`（FEvolution、FPBDRigidsSolver） | M3：稳定堆叠（这是最难的一步） |
| 位置校正 | 位置级投影/校正、防穿透 | `Evolution/` | M3+：消除"陷进地面" |
| 性能机制 | Island 休眠/唤醒、任务图并行 | `Island/`（FIslandManager） | M4：休眠标记 |
| 关节 | 约束求解（球关节/铰链） | `Joint/` | M4+：摆锤/链条 |
| 连续碰撞 | CCD（扫掠检测）防高速穿透 | `Collision/`（CCDUtilities.h） | 高速球不穿墙 |

- 产出：能稳定堆叠方块堆 + 一个关节 demo 的 mini 引擎
- 阅读重点：`FEvolution::AdvanceOneTimeStep` —— 一帧物理的全流程

### 阶段 4：进阶专题（第 10~12 周，按兴趣选 1~2 个）

| 专题 | 原理 | Chaos 源码入口 | 参考论文 |
|---|---|---|---|
| 布料/软体 | **XPBD** 约束求解 | `Deformable/`（BlendedXPBDCorotatedConstraints.h）、`ChaosCloth/` | XPBD（Müller 2016） |
| 破坏 | cluster 层级、断裂 | `GeometryCollection/`、`PhysicsProxy/` | Epic GDC/SIGGRAPH 讲座 |
| 力场 | FieldData 场驱动 | `Field/` | UE 文档 Field 章节 |
| 角色物理 | 步行/质心控制 | `Character/` | — |

### 阶段 5：收尾（第 13 周+）

- 输出 Chaos 架构图 + 算法笔记（求解器/碰撞各一篇）
- mini 引擎沉淀为 BaseTestEngine 的 `Physics` 模块
- 与渲染线汇合：刚体位置/碰撞点画进 DX12 渲染器

---

## 每周节奏（沿用 Plan.md 风格）

```
周一~三：主线算法 + 读 Chaos 源码（一次一个主题）
周四　：读对应论文/理论（Catto 求解器系列 / XPBD 等）
周五　：UE 实战（CVD 观察、demo 实验、物理资产）
周末　：mini 物理引擎推进一个 milestone
```

---

## 关键概念清单（打勾用）

- [ ] FImplicitObject 隐式几何抽象
- [ ] ParticleHandle / SOA 数据布局
- [ ] 宽相：AABBTree / BVH / dirty 网格
- [ ] 窄相：primitive 对 + 凸-凸算法（GJK/MPR？去源码确认）
- [ ] Contact：FContactPoint / manifold / ContactModification
- [ ] 顺序冲量求解 + 位置校正
- [ ] Island 休眠 / 唤醒
- [ ] CCD 连续碰撞检测
- [ ] XPBD（布料）
- [ ] Cluster 破坏层级
- [ ] PhysicsProxy：游戏线程 ↔ 物理线程的桥
- [ ] CVD 可视化调试

---

## 源码速查表（F:\workspace\UnrealEngine58\Engine\Source\Runtime\Experimental\Chaos\）

| 想看什么 | 去哪 |
|---|---|
| 数学类型 | `Public/Chaos/Math/` |
| 粒子/句柄 | `Public/Chaos/Particle/` |
| 几何体 | `Public/Chaos/Geometry/` |
| 碰撞 | `Public/Chaos/Collision/`（AABBTree.h、CollisionResolution.h） |
| 求解器驱动 | `Public/Chaos/Evolution/`（FEvolution） |
| 休眠 | `Public/Chaos/Island/` |
| 关节 | `Public/Chaos/Joint/` |
| 布料/软体 | `Public/Chaos/Deformable/` |
| 物理材质/场景 | `Public/Chaos/Framework/` |
| 引擎桥接 | `PhysicsProxy/` |
| 破坏资产 | `GeometryCollection/` |
| 力场 | `Field/` |
| 调试 | `ChaosVisualDebugger/`、`ChaosDebugDraw/` |

---

## 推荐资源

| 类别 | 资源 | 用途 |
|---|---|---|
| 官方文档 | [UE Physics 官方文档](https://dev.epicgames.com/documentation/unreal-engine/physics-in-unreal-engine)（含 Chaos Physics / Destruction / Cloth 章节） | 总览与术语 |
| 源码 | 本地 `Experimental/Chaos/` | 唯一权威 |
| 论文 | PBD（Müller 2007）→ XPBD（Müller 2016）→ Catto GDC 求解器系列 | 算法原理 |
| 开源对照 | Bullet（刚体架构）、**Jolt**（架构清晰、现代 C++，强烈推荐）、Box2D（求解器教学） | 跨引擎对照 |
| 讲座 | Epic 的 Chaos 相关 GDC/SIGGRAPH | 设计动机 |
| 工具 | ChaosVisualDebugger（CVD） | 全程调试 |

---

## 阶段 0 启动清单（本周）

1. UE 里放 Chaos Solver Actor，堆 3~5 个盒子，跑起来
2. 连上 CVD，观察速度向量/碰撞点/休眠
3. 官方文档 Chaos 总览过一遍
4. 回答："一帧里刚体经历了哪些阶段"（感性版即可）
