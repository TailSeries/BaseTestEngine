# RHI 复刻学习 — 工作约定

在 BaseTestEngine 中仿照 UE 的 RHI/D3D12RHI 架构封装 DirectX12,纯学习用途。
**路线图与章节进度以 `Source/RHI_Learning.md` 为准**(随时更新那份)。

## 教学模式
- 2026-10-26 起切换为面试导向：**先对照 UE 完成可运行实现与验收，再沿真实调用链讲解源码**。用户已明确授权直接完成阶段 B，无需逐段等待用户手敲。
- 新增代码必须有中文注释，说明用途、所有权、状态/同步契约，以及相对 UE 的简化；讲解以可验证的双 Pass 示例和面试问题为入口。
- 后续按用户当次要求推进；保持 UE 对照，避免把“能运行”说成“完整复刻 UE”。
- **禁止参考 `Source/DirectX12/` 下的任何内容**:那是独立的 D3DApp 教程代码,不是 UE 架构,混用会引入与 UE 骨架不一致的模式。参考来源只有 UE 源码 + 用户自己的 RHI/D3D12RHI 模块。

## 贴合 UE 骨架,不过度合并
- 保持 UE 的类分层 / 命名 / 文件结构。D3D12 层保留三层:
  `FD3D12Adapter`(物理GPU+工厂+Device容器) → `FD3D12Device`(GPU节点,持有 Queues) → `FD3D12Queue`(D3DCommandQueue+Fence)。
  **即使单 GPU、单线程也不把多层合并成一个类。**
- 简化**只针对内部细节**:接口多版本数组(Device1..12 / Factory2..7 只留基础版)、多GPU数组、
  多线程提交管线(Payload 队列 / 对象池 / Timing)、间接绘制签名等。**骨架、层级、命名跟 UE 走。**
- **Why:** 过度合并短期省事,但项目变大后命名 / 结构对不上 UE,反而难管理、难对照,背离「对照 UE 学习」的初衷。

## UE 源码参考路径(机器相关,以用户告知为准)
- 接口层:`F:\workspace\UnrealEngine58\Engine\Source\Runtime\RHI\`
- 实现层:`F:\workspace\UnrealEngine58\Engine\Source\Runtime\D3D12RHI\Private\`

## 工程坑
- 共享所有权类型统一写 `TRefCountPtr<T>`，不直接写 `std::shared_ptr<T>`；当前别名实现除外。创建对象仍可使用 `std::make_shared<T>()`。
- 源文件采用 **UTF-8 无 BOM**，根 CMakeLists 已启用 MSVC `/utf-8`；保留该选项，避免中文被按代码页 936 解析。换行遵守 `.gitattributes` 和 `.editorconfig`。

- 日志优先使用现有 spdlog 封装 `Logging/Log.h` 的 `CORE_LOG`，不再另外建立日志系统。
