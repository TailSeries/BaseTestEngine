# 阶段 A 收尾说明（2026-09-27）

本轮按既定阶段 A 边界收尾：用 UE 风格的 RHI / D3D12RHI 分层画出纹理 Mesh，并验证资源创建、描述符、多帧、延迟释放与退出。并不表示完整复刻 UE RHI；通用资源状态追踪和自动 Barrier 是下一阶段 B。

## 这轮具体改了什么

| 模块/文件 | 改动与目的 |
|---|---|
| D3D12Resources、D3D12View、D3D12Viewport | 每个 backbuffer 纹理持有 FD3D12RenderTargetView；创建 View 时分配 RTV 槽，Context 从 RenderPass 颜色附件取得 RTV，删除 Viewport 的 GetCurrentBackBufferRTV 和私有 RTV 堆。 |
| D3D12Device | Device 持有 SRV/RTV/DSV 描述符堆，创建并初始化默认 Context。Context 先于堆和 Queue 析构。 |
| D3D12CommandContext | Init 只接收 Device，内部取得 Direct Queue，不再保存 Viewport。当前 Pass 保存颜色附件，EndRenderPass 为它录制屏障；EndFrame 仅提交、Signal、挂接延迟释放与推进帧槽。 |
| RHI.cpp、RHICommandList | 新增 RHIInit/RHIExit、FRHICommandListImmediate、FRHICommandListExecutor。上层取得即时命令列表，不再创建或取得具体 D3D12 Context。 |
| DynamicRHI、D3D12Viewport | EndDrawingViewport → RHIEndDrawingViewport → PresentInternal。使用 UE 同名 FRHIPresentArgs；当前单线程版本要求先 EndFrame 提交，再调用呈现入口。 |
| D3D12Descriptors、D3D12View | 固定容量空闲槽列表与使用标记替代单向递增分配；耗尽/越界/重复释放显式报错。View 析构归还槽位，GPU 在飞引用经 PendingDeletes → 提交 Fence → DeletionQueue 保活。 |
| D3D12CommandContext 常量 | 两个帧槽各持有 64 KiB upload 常量区，每次绑定分配独立的 256 字节对齐区域，避免同帧后一次写入覆盖前一次 draw 的常量。超过容量显式拒绝；复用帧槽前等待 Fence。 |
| RHITest/main.cpp | 资源、PSO、View、Viewport 创建和绘制只使用 RHI 类型。测试平台启动和 Shader 编译移到 TestPlatform；字节码以 vector 返回，不暴露 Blob。顶点数据增加 16 字节前缀以验证 Offset 与声明 Stride 的独立性。 |
| RHITest 换纹理 | 初始化时预上传两张棋盘纹理，运行时只替换 SRV；不再每次同步上传而排空 GPU 队列。旧 SRV 显式 DeferredDelete。自动验收执行 120 次替换，超过 8 槽容量验证回收。 |
| D3D12RHIPrivate | 修复 VERIFY_D3D12：不能用 assert 包含必须执行的 API 调用，否则 Release 会消去调用。现在始终执行并检查 HRESULT。 |
| D3D12Adapter、DynamicRHI、Queue | 修复选卡成功条件后误加分号；处理无可用适配器、空 Shader 字节码、Fence event 创建/等待失败。无初始数据的采样纹理直接使用 PIXEL_SHADER_RESOURCE 状态，内容仍未定义。 |

## UE 对照与简化边界

参考根目录：`F:\workspace\UnrealEngine58\Engine\Source\Runtime`。

- `D3D12RHI/Private/D3D12Device.h/.cpp`：Device 管理默认 Context；本项目仍保留 Adapter → Device → Queue。
- `D3D12RHI/Private/D3D12Texture.h` 的 EmplaceRTV / GetRenderTargetView：纹理拥有附件 View。本项目每种附件只存一个 View，不实现 mip/切片数组。
- `D3D12RHI/Private/D3D12View.cpp` 的 AllocateHeapSlot / FreeHeapSlot：View 创建/析构与描述符分配/释放配对。本项目用固定堆空闲列表，未实现 UE offline/online managers、复制缓存和 bindless。
- `RHI/Public/RHICommandList.h`、`RHI/Private/RHICommandList.cpp`：保留 ImmediateCommandList 与 Executor 的职责。没有命令重放、RHI Thread、并行翻译或 Submission payload。
- `RHI/Public/DynamicRHI.h`：RHIGetDefaultContext 仅供内部初始化，不向测试暴露同名自由函数；UE 5.8 已弃用上层直接调用该自由函数。
- UE 5.8 的 BeginDrawingViewport 已是弃用空函数，本项目没有引入它。Present 使用 EndDrawingViewport；当前 EndFrame 的具体提交时机是教学版约定，不声称与 UE 调度完全一致。
- 平台工厂注入的 RHIInit 签名是教学简化，不是 UE 的逐字复刻。TestPlatform 只是启动和编译适配器，不是新的 Renderer 层。

## 必须保持的生命周期约定

1. 所有资源/View/Viewport 都必须先于 RHIExit 销毁；Device 拥有的描述符堆比 View 活得更久。
2. 绑定接口接收裸指针，不会自动捕获资源。最后一个上层引用释放前，若 GPU 仍可能使用它，必须通过 DeferredDelete 保活。SRV 持有纹理，因此延迟删除 SRV 也保住纹理。
3. PendingDeletes 在 EndFrame 使用真实 Signal 值入队；BeginFrame 与 WaitForGPU 只释放已完成 Fence 的条目。View 真正析构时才归还描述符。
4. Resize 前必须等待 GPU，并释放旧 backbuffer 的外部引用。Viewport 删除旧纹理及 RTV 后再 ResizeBuffers，新 View 可以复用槽位。
5. RenderPass 附件仍借用上层资源；调用方须持有至 GPU 完成。当前仅支持交换链 backbuffer + D32，屏障仍明确写成 PRESENT ↔ RENDER_TARGET，深度维持 DEPTH_WRITE。
6. Load_Store 验证的是同一帧前一 Pass 的内容保留，不承诺 flip-discard Present 后跨帧保留内容。

## 可重复验收

先构建 Debug：

```powershell
cmake --build out/build/x64-Debug --target RHITest
powershell -ExecutionPolicy Bypass -File Source/RHITest/RunStageAValidation.ps1 -Configuration Debug
```

本机 Debug 构建目录只生成 Debug 配置；Release 使用独立目录：

```powershell
cmake -S Source -B out/build/stage-a-release -G "Visual Studio 18 2026" -A x64 -DCMAKE_CONFIGURATION_TYPES=Release
cmake --build out/build/stage-a-release --config Release --target RHITest
powershell -ExecutionPolicy Bypass -File Source/RHITest/RunStageAValidation.ps1 -Configuration Release
```

也可以直接运行 `RHITest.exe --validate`，报告和 PPM 回读图像写入工作目录。验收启用 D3D12 debug layer 和可用的 GPU-based validation；未安装调试层时会明确失败，不会静默略过验证。

验收内容：

- 描述符耗尽、越界、重复释放、释放后复用；帧与 Pass 配对、空 Shader、非法 draw 范围、顶点 Offset 越界、常量区容量。
- 24 次 backbuffer Resize，以及 24 次深度纹理创建/销毁，RTV/DSV 已分配数量不增长。
- 创建专用 GPU gate Fence，阻塞 Direct Queue 后提交真实 indexed draw。释放 CPU 引用，确认 SRV 与纹理仍存活，新 View 不覆盖在飞槽；解除 gate 并等待后确认弱引用失效，旧槽可复用。
- 120 次 SRV 替换、多帧提交、Clear_Store 后接 Load_Store；最终回读 backbuffer，检查背景色和非背景图元像素，保存图像供视觉检查。
- 检查 D3D12 InfoQueue 中没有 warning/error/corruption，等待 GPU 后释放上层资源并关闭 RHI。

后端专用测试放在 BackendValidation.cpp；它有意使用 D3D12 的 Fence、读回和诊断接口，不属于普通绘制代码的 RHI 边界。

## 本机验收结果

2026-09-27：Debug 与 Release 构建和运行均通过。每个配置完成上述测试，D3D12 InfoQueue 无 warning/error/corruption；640×480 回读图像包含 27286 个非背景像素，角落像素匹配清除颜色。Debug 回读画面已人工式图像检查为蓝底棋盘格立方体。报告位于 `out/stage-a/Debug/` 与 `out/stage-a/Release/`。本结论仅覆盖上述显式约束，不代表所有 UE 功能或设备组合都经过验证。

## 阶段 B 接续点

下一步是 FRHITransition / ERHIAccess / 资源状态追踪：逐步替换 Context 中固定的 backbuffer 屏障，支持离屏附件和 RTV/SRV 用途转换。RHI 多线程、完整 Shader 编译系统、UniformBuffer 系统、完整 PSO 缓存、动态采样器、MRT/MSAA/mip/数组均没有在本次冒充完成。
