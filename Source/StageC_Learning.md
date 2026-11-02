# 阶段 C：RenderCore 与最小 Renderer

## C1：Shader 编译输出与参数映射（2026-11-02）

本节把测试程序的 CompileTestShader 升级为独立 ShaderFormatD3D 平台编译入口。
RenderCore 提供平台无关的 FShaderCompilerInput、FShaderCompilerOutput、FShaderParameterMap。
C1 最初只包含头文件类型；C2 加入运行时实现后，RenderCore 已改为 STATIC target。

### 从示例追调用链

1. RHITest/main.cpp 的 Initialize 构造编译输入：源码、入口、Shader 阶段。
2. ShaderFormatD3D/Private/D3DShaderCompiler.cpp 的 CompileD3DShader 调用 D3DCompile。
3. D3DReflect 读取实际编译产物，而不是扫描 HLSL 文本。优化掉的资源不应被当作实际绑定需求。
4. 遍历 BoundResources，记录资源名称、寄存器起点、数组长度；cbuffer 额外读取字节大小。
5. ParameterMap 回答“CB/gTex/gSamp 在哪里”；ResourceCounts 回答“各类寄存器需要覆盖多大范围”。
6. MakeRHICreateDesc 把字节码视图和统计交给 RHICreateVertexShader / RHICreatePixelShader。
7. 既有 D3D12RHI 根据统计生成 QBSS、查找根签名、创建 PSO，原有 Draw 链路不变。

### 与前面 b0/b1 问题的关系

仅使用 b1 时，参数表记录 BufferIndex=1，NumCBs=2，覆盖寄存器范围 [0,2)。
本例 FXC 将纹理数组 t3[2] 展开成 Images[0]、Images[1]，BaseIndex 分别为 3、4，Size 各为 1，NumSRVs=5。
参数表保留真实反射记录，不承诺所有编译器都以同样方式表达数组。
寄存器索引不等于根参数槽位，也不等于描述符堆偏移。
当前后端依然只接受 VS b0、PS t0/s0，所以反射成功的 b1 Shader 不代表能创建并绘制对应 PSO。
本节验证 b1 和数组元数据；扩大根签名生成器、绑定接口的范围属于后续独立工作。

### 所有权与边界

- Output 拥有字节码和参数表；MakeRHICreateDesc 返回借用视图，Output 必须活到 RHI 创建调用返回。
- 反射 COM 对象只存在于同步编译过程，不保留到 Draw。
- 编译失败、反射失败、超范围、不支持资源返回 bSucceeded=false 和 Diagnostics；失败输出不能转为 RHI 描述。
- 当前只支持 SM5 VS/PS、整块 cbuffer、纹理和采样器元数据；不包括 cbuffer 成员参数、结构化缓冲、UAV、include、宏、Worker、缓存、序列化。
- 保留已有 FShaderCodePackedResourceCounts 在 RHI 的位置作为过渡，避免 RHI 反向依赖 RenderCore。
- UE 将统计存入 ShaderCode 附加数据，当前通过 FRHICreateShaderDesc 显式传递。
- UE 的现代 D3D 编译路径包含 DXC/SM6；本节沿用 FXC/D3DReflect，不能视为完整复刻。
- 采样器寄存器信息不包含运行时过滤/寻址配置；现有 point/wrap 静态采样器策略仍由后端决定。

### UE 对照

- Engine/Source/Runtime/RenderCore/Public/ShaderCore.h：FParameterAllocation、FShaderParameterMap。
- Engine/Source/Runtime/RenderCore/Public/ShaderCompilerCore.h：FShaderCompilerInput、FShaderCompilerOutput。
- Engine/Source/Developer/Windows/ShaderFormatD3D/Private/D3DShaderCompiler.cpp：平台编译器职责。

### 验证

2026-11-02：Debug / Release 编译、C1 元数据验证及 A/B GPU 回归全部通过；调试队列无警告/错误。
入口：`Source/RHITest/RunStageCValidation.ps1 -Configuration Debug`（或 Release）。

RHITest --validate 在初始化时执行 StageCValidation.cpp，生成 stage-c-compiler-checks.txt。
覆盖 b1 与 64 字节 cbuffer、t3 纹理数组、s2、寄存器范围、缺失参数、RHI 描述转换、错误诊断与不支持资源拒绝。
继续运行原有 A/B 验收，检查双 Pass 图像、GPU 生命周期、资源屏障和调试层。

## C2：Shader 对象与按名称解析参数（2026-11-02）

### 对象职责与所有权

- FShader：记录阶段，派生类拥有已解析的参数成员；不直接持有 RHI Shader。
- FGlobalShader：不依赖材质和 VertexFactory 排列的 Shader 基类；不是单例，也不自动编译。
- FShaderMapResource：本节为单 Shader 容器，持有 TRefCountPtr<FRHIShader>。
- TShaderRef<T>：共享拥有元信息对象与资源容器，提供 GetVertexShader/GetPixelShader。
- CreateShader<T>：显式创建入口，确保两部分来自同一编译产物；TShaderRef 配对构造不向外开放。
- App 在 WaitForGPU 后析构，全部 Shader 引用在 RHIExit 前释放。运行中替换 Shader 的延迟释放尚不由这些类自动处理。

### 实际调用链

1. CompileD3DShader 返回编译输出。
2. CreateShader<FTestVS/FTestPS> 构造派生 Shader；构造器调用参数成员的 Bind。
3. Bind 在 ParameterMap 按名字查找 CB/gTex/gSamp，把数字索引和类型缓存到成员。
4. FShaderMapResource 创建 FRHIVertexShader/FRHIPixelShader；TShaderRef 关联元信息和资源。
5. PSO 从 TShaderRef 取得 RHI Shader。Draw 前先设置匹配的 PSO。
6. VertexShader->SetParameters 提交矩阵；PixelShader->SetParameters 提交 SRV。
7. RenderCore 辅助函数检查大小/类型/阶段，再将已解析的索引传给 RHI。
8. D3D12RHI 沿用现有根 CBV 上传与 SRV 表绑定路径。

Bind 是“建立 CPU 参数映射”，不是“绑定 GPU 资源”。每次 Draw 不再通过字符串查表。
FShaderUniformBufferParameter 的 GetBaseIndex 返回 ParameterMap 中的 BufferIndex（b 寄存器）。
FShaderResourceParameter 的 GetBaseIndex 返回 ParameterMap 中的 BaseIndex（t/s/u 寄存器）。
两者不是根参数槽位。

### 必需参数与可选参数

SPF_Mandatory 找不到名字就报错，包括拼写错误或资源被优化掉的情况。
SPF_Optional 找不到则保持未绑定，提交辅助函数直接跳过。
重新 Bind 时先清空旧映射，失败后不能沿用旧位置。
整块常量数据大小必须等于反射的 cbuffer 大小；C++ 结构布局仍须由调用方与 HLSL 对齐。
类型检查无法证明字节内容正确，例如同为 64 字节也可能使用了错误的矩阵布局。

### 与 UE 的关系及当前边界

对照 UE RenderCore/Public 的 Shader.h、GlobalShader.h、ShaderParameters.h。
保留 FShader、FGlobalShader、FShaderMapResource、TShaderRef 和参数类的职责分层。
本节仍未实现 FShaderType/GlobalShaderMap 注册、permutation、ShaderMap 指针表、序列化、延迟 RHI Shader 创建。
FShaderMapResource 尚未继承后续要建立的 FRenderResource，也不实现 UE 的批量资源/异步清理机制。
当前 TShaderRef 用共享引用持有对象，UE 的 ShaderMap 生命周期实现不同。

我们没有 FRHIUniformBuffer，所以 SetShaderConstantBufferData 是明确命名的教学桥接：
上传整个 CPU 字节块到帧常量 arena，而不是冒充 UE 的 SetUniformBufferParameter 对象绑定。
FTestPS 检查静态采样器 s0 的元数据；没有动态设置 sampler 的调用。
只支持 VS b0、PS t0/s0 的限制保持不变；本节验证非零索引的解析，不宣称非零索引已能绘制。
调用方必须设置与参数所属 Shader 一致的 PSO；当前 RHI 没有通用的 Shader 身份绑定校验。

### 验证入口

2026-11-02：C2 的 Debug / Release 构建及 C1/C2/A/B 回归全部通过，图像逐像素一致，D3D12 调试队列无警告/错误。

RunStageCValidation.ps1 执行 C1、C2 与 A/B 图像回归。
C2 输出 stage-c-parameter-checks.txt，覆盖非零索引解析、必需/可选参数、类型/大小/阶段拒绝、
失败重新绑定清空、RHI Shader 创建与错误阶段访问。正常双 Pass 验证按名解析后的真实 GPU 提交。

### 接下来

继续完善 Shader 类型注册与 GlobalShaderMap，再推进 FRenderResource、VertexFactory 与场景绘制抽象。
多寄存器根签名与绑定扩展需要单独打通；阶段 C 尚未完成。
