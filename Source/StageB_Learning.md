# 阶段 B：资源访问、Transition 与 D3D12 Barrier

2026-10-26：按约定教学范围完成。Debug / Release 编译及运行验收通过。
学习方式切换为：先运行完整实现，再从一帧输出追踪源码，最后对照 UE 与面试问答。

## 1. 本阶段解决了什么

阶段 A 的 BeginRenderPass/EndRenderPass 写死了 backbuffer 的 Present ↔ RTV。
阶段 B 让调用方声明“哪一资源、从什么访问、到什么访问”，后端负责验证、追踪、映射、合并和提交。
RenderPass 只负责附件绑定与 Load/Store，不再猜下一次用途。

当前实现的实际帧结构：

```text
BeginFrame
  SceneColor: Unknown -> RTV（首帧无需屏障，后续帧从 PS SRV 转来）
  ScenePass: 清除离屏颜色/深度，绘制棋盘格立方体
  Transition batch:
    SceneColor: RTV -> SRVGraphicsPixel
    BackBuffer: Present -> RTV
  CompositePass: 全屏四边形采样 SceneColor，输出到 BackBuffer
  PreservePass: 空的 Load_Store Pass，回归验证内容保留
  BackBuffer: RTV -> Present
EndFrame（Execute + Signal）
EndDrawingViewport（Present）
```

两次真正的绘制 Pass，额外一个不绘制的 Load/Store 验收 Pass。
全屏 Pass 暂沿用相同 VS/PS、单位 WVP 和 D32 附件，使用 Always 比较且关闭深度写入。
`DrawTriangle` 保留旧函数名，实际绘制立方体和全屏四边形。

## 2. 推荐源码阅读顺序

| 顺序 | 文件/入口 | 阅读后必须能回答 |
|---|---|---|
| 1 | `RHITest/main.cpp`：Initialize、DrawTriangle | 同一 SceneColor 为什么既有 RTV 又有 SRV？什么时候切换用途？ |
| 2 | `RHI/Public/RHIAccess.h`、`RHITransition.h` | 访问、子资源范围、单资源描述、批量创建描述有什么区别？ |
| 3 | `RHI/Public/RHICommandList.cpp`：Transition | 谁创建对象，谁执行，谁清理？ |
| 4 | `RHI/Public/RHITransition.cpp` | RHI 不知道 D3D12 数据类型，怎样分配它的存储？ |
| 5 | `D3D12RHI/Public/D3D12LegacyBarriers.cpp`：BuildTransitionPlan | 怎样验证原状态、合并读状态、避免半批提交？ |
| 6 | `D3D12RHI/Public/D3D12CommandContext.cpp`：Begin/EndTransitions、Begin/EndRenderPass | 描述在哪一行变成 GPU 命令？状态记录何时改变？ |
| 7 | `RHITest/StageBValidation.cpp` | 哪些错误被主动拒绝？如何证明没有靠 GPU 容错得到正确画面？ |

先沿具体对象 SceneColor 走一遍，不必先背完所有枚举。

## 3. 从一次调用追到底

```text
FRHIComputeCommandList::Transition(Infos)
  RHICreateTransition(CreateInfo)                  RHI 自由函数
    aligned operator new：公共头 + padding + 私有对象存储
    GDynamicRHI->RHICreateTransition(...)          后端虚接口
      placement new FD3D12LegacyBarriersTransitionData
      复制 Infos，不能只保留 span
  IRHIComputeContext::RHIBeginTransitions
    当前单管线版本只验证/记录 Begin 配对
  IRHIComputeContext::RHIEndTransitions
    BuildTransitionPlan：验证整批、建立预测终态
    ID3D12GraphicsCommandList::ResourceBarrier
    更新 FD3D12Resource::State
  RHIReleaseTransition                            作用域清理
    后端析构私有数据，然后释放整块存储
```

注意同名 RHICreateTransition 有两层：自由函数负责公共内存，虚函数负责后端私有对象的构造。
RHI 公共头不包含 D3D12 类型。`GetPrivateData<T>()` 通过分配时记录的偏移访问尾部，并核对类型尺寸/对齐。

UE 对照（本机根目录 `F:/workspace/UnrealEngine58/Engine/Source/`）：

- `Runtime/RHI/Public/RHIAccess.h`：抽象访问类型。
- `Runtime/RHI/Public/RHITransition.h`：范围、描述、创建参数、FRHITransition 尾部存储。
- `Runtime/RHI/Public/RHICommandList.h`：四层命令列表与 Transition 入口。
- `Runtime/RHI/Private/RHICommandList.cpp`：TransitionInternal 的 Bypass 分支，使用临时内存构造转换对象，Begin/End 后手动释放。
- `Runtime/D3D12RHI/Private/D3D12CommandContext.cpp`：创建/释放入口转给 Barrier 实现。
- `Runtime/D3D12RHI/Private/D3D12LegacyBarriers.cpp`：传统屏障私有数据与执行。

我们更接近 UE 的单管线 Bypass 路径，但用堆内存 + RAII 代替 MemStack；未复刻多管线原子完成位、命令重放、Legacy/Enhanced 的运行时策略分发。
UE 也有通过各管线 Begin/End 完成标记自动回收的路径，不能把本项目的显式释放概括为 UE 唯一机制。

## 4. 状态追踪的核心契约

`State` 是按录制/提交顺序预测的原生状态，不是向 GPU 查询的实时值。
本版本只有一个默认 Context，所有常规帧按同一 Direct Queue 顺序提交，所以可以在录制时更新全局记录。
不能直接把这种实现用于多个并行录制的 CommandList；那需要局部状态、提交时原状态对账等机制。

- `AccessBefore == Unknown`：采用已有追踪状态。绝不将它解释成原生 COMMON，也不丢弃数据。
- 显式 Before：必须与追踪状态一致，或者是当前只读状态集合的子集，否则报错。
- After Unknown、未实现访问位、错误资源能力、局部范围均报错。
- 完全相同状态不提交屏障。
- 读状态扩大时合并并提交屏障；请求已有读集合的子集时保留原集合，无需屏障。
- 一个批次内部没有 Draw/Copy：同资源的连续转换先逐条验证，最后只提交初态到终态，往返同态可完全省略。
- 整批验证完成前不录制原生命令、不修改资源状态；后半批有错误不会留下前半批状态修改。
- UPLOAD Buffer 原生状态固定为 GENERIC_READ，只接受包含于它的读用途，拒绝写转换。
- DEFAULT Buffer 在 ExecuteCommandLists 批次完成后衰减为 COMMON。EndFrame 预测下一批起始状态为 COMMON，不做 CPU 等待。
- 原生 PRESENT 与 COMMON 都是数值 0；RHI 的 Present 仍只允许交换链纹理。

`TranslateAccess` 是本项目的集中映射函数，不能把 RHI 枚举数值直接强转为 D3D12_RESOURCE_STATES。
深度只提供可写 DSV：可以验证 DSVRead/DSVWrite 状态往返，但 BeginRenderPass 仍要求 DEPTH_WRITE。

## 5. 三种不同的生命周期/同步

| 对象或机制 | 何时结束/解决什么 |
|---|---|
| 调用方 Infos 数组 | 后端复制描述后就可以销毁 |
| FRHITransition 与私有数据 | 当前即时路径 End 完成命令录制后即可清理 |
| Texture/Buffer/View | 必须存活到 GPU 完成其最后使用，通过现有延迟删除与 Fence 保护 |
| ResourceBarrier | 在 GPU 命令流中表达资源访问之间的同步与状态变化，不是 CPU 等待 |
| Fence / WaitCPU | 判断队列进度、保护帧槽和资源生命周期；不能代替资源屏障 |

Transition 描述中的裸指针是借用。CPU 释放转换对象，不会释放纹理，也不会证明 GPU 已完成。
Begin/EndTransitions 在此不是 D3D12 BEGIN_ONLY/END_ONLY split barrier：Begin 记录配对，End 提交普通完整屏障。
当前低层 Begin/End 只允许同 Context、同帧配对，未配对不能 EndFrame；高层同步 Transition 自动管理对象。

## 6. 验收与复现

```powershell
cmake --build out/build/x64-Debug --config Debug --target RHITest
& Source/RHITest/RunStageBValidation.ps1 -Configuration Debug

# 复用已存在的 Release 构建目录（目录名保留历史名称）。
cmake --build out/build/stage-a-release --config Release --target RHITest
& Source/RHITest/RunStageBValidation.ps1 -Configuration Release
```

2026-10-26 两种配置均通过：

- 空批次、相同状态、读状态合并/子集、Unknown、同批次重复资源。
- 错误 Before/After、空资源、越界范围、局部范围、互斥读写、非法 Present/UAV、无帧、Pass 内转换。
- 失败批次没有部分状态更新和部分屏障提交。
- 私有存储对齐、描述副本、Begin/End 配对、未配对 EndFrame 拒绝。
- UPLOAD 固定状态、DEFAULT Buffer 跨提交衰减、深度状态往返。
- GPU 纹理复制回读，所有 RGBA 像素与清除色一致。
- 121 帧双 Pass，最终离屏/合成 RGBA 逐像素一致；640×480，27286 个非背景像素，角落为清除色。
- 保留阶段 A：描述符边界/回收、24 次 Resize、24 次 DSV 重建、真实 GPU gate 保活、120 次换 SRV、退出。
- D3D12 debug layer / GPU validation 的 warning/error/corruption 均无。

产物：`out/stage-b/Debug` 和 `out/stage-b/Release` 下的 `stage-b-validation.txt`、`stage-b-checks.txt`、`stage-a-checks.txt`、`stage-b-frame.ppm`。
运行日志使用已有 `CORE_LOG` / spdlog，写到验证工作目录的 `Logs/RHITest.log`。
回读诊断直接使用原生临时命令列表，但在同一队列上执行并恢复原状态，不污染常规追踪；业务绘制只调 RHI。

## 7. 面试复述练习

1. **为什么已经有 SRV 还需要 Transition？** View 描述访问的格式/范围；Transition 描述访问之间的状态与同步，它们职责不同。
2. **创建标志、访问状态、PSO 有什么区别？** 创建标志决定允许用途；访问状态决定当前合法用途；PSO 决定 Shader 和固定管线行为。
3. **为什么 EndRenderPass 不自动 Present？** 下个消费者可能是 Shader、Copy 或另一个 Pass；Present 仅适用于交换链图像。
4. **Barrier 与 Fence 能相互替代吗？** 不可以。前者处理资源访问，后者跟踪队列进度/队列间依赖和 CPU 等待。
5. **为什么可以提前更新 State？** 它是顺序预测；只有当前单 Context、同队列提交的约束下才成立。
6. **为什么转换对象可以先释放？** GPU 使用的是已经录制的命令及资源，CPU 转换描述此时已经消费完；资源仍需保活。
7. **相同状态就一定不需要屏障吗？** 当前支持的普通状态转换可去重；UAV 同态读写依赖可能仍需 UAV Barrier。本版拒绝 UAV，不能推广这个优化。
8. **这里自动化了什么，RDG 还做什么？** 本阶段自动映射/追踪/合并资源状态；RDG 从 Pass 参数推导依赖、执行顺序和必要转换。
9. **如何排查黑屏？** 先检查离屏回读，再检查合成结果；核对创建标志、RTV/SRV、PSO、前后访问和调试层，最后检查提交/Present。

## 8. 明确保留的边界

只支持单线程即时录制、Graphics -> Graphics、Direct Queue、整资源、RGBA8 单颜色附件 + D32、单层/mip0/单采样。
不包含跨队列 Fence、异步 Compute、UAV 执行、局部子资源追踪、只读 DSV View、MRT、MSAA、资源别名、Enhanced Barriers、并行命令列表、RDG、Shader 反射编译系统。
未实现访问在入口明确拒绝，不用空实现伪装成功。这些是后续专题，不作为本次阶段 B 的完成门槛。
