# RHI 复刻学习 — 工作约定

在 BaseTestEngine 中仿照 UE 的 RHI/D3D12RHI 架构封装 DirectX12,纯学习用途。
**路线图与章节进度以 `Source/RHI_Learning.md` 为准**(随时更新那份)。

## 教学模式
- 节奏:**先讲 UE 源码 → 再对照写简化版**。每实现新章节前,先查对应的 UE 参考文件。
- **用户自己敲代码学习**:我负责讲解 + 给参考代码块,**不要直接写进用户的源文件**(除非用户明确要求)。
- **禁止参考 `Source/DirectX12/` 下的任何内容**:那是独立的 D3DApp 教程代码,不是 UE 架构,混用会引入与 UE 骨架不一致的模式。参考来源只有 UE 源码 + 用户自己的 RHI/D3D12RHI 模块。

## 贴合 UE 骨架,不过度合并
- 保持 UE 的类分层 / 命名 / 文件结构。D3D12 层保留三层:
  `FD3D12Adapter`(物理GPU+工厂+Device容器) → `FD3D12Device`(GPU节点,持有 Queues) → `FD3D12Queue`(D3DCommandQueue+Fence)。
  **即使单 GPU、单线程也不把多层合并成一个类。**
- 简化**只针对内部细节**:接口多版本数组(Device1..12 / Factory2..7 只留基础版)、多GPU数组、
  多线程提交管线(Payload 队列 / 对象池 / Timing)、间接绘制签名等。**骨架、层级、命名跟 UE 走。**
- **Why:** 过度合并短期省事,但项目变大后命名 / 结构对不上 UE,反而难管理、难对照,背离「对照 UE 学习」的初衷。

## UE 源码参考路径(机器相关,以用户告知为准)
- 接口层:`F:\shakervon_engine_merge\Engine\Source\Runtime\RHI\`
- 实现层:`F:\shakervon_engine_merge\Engine\Source\Runtime\D3D12RHI\Private\`

## 工程坑
- **新建含中文注释的源文件,存成 UTF-8 with BOM**。中文系统(代码页 936)下 MSVC 会把无 BOM 的
  UTF-8 当 GBK 解析,拼错多字节字符、连带打乱 `#if/#endif` 与大括号配对。仓库既有文件多为 GBK。
