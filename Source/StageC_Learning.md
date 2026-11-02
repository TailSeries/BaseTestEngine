# 阶段 C：RenderCore 与最小 Renderer

## C1：Shader 编译输出与参数映射（2026-11-02）

本节把测试程序的 CompileTestShader 升级为独立 ShaderFormatD3D 平台编译入口。
RenderCore 提供平台无关的 FShaderCompilerInput、FShaderCompilerOutput、FShaderParameterMap。
当前 RenderCore 只有头文件类型，CMake 使用 INTERFACE target；后续加入运行时实现再调整。

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

### 接下来

C1 之后建立 FShader / FGlobalShader 与参数绑定对象，使示例通过参数名绑定编译所得位置。
随后完善 VertexFactory、网格/材质/视图、Mesh Pass 与 MeshDrawCommand。阶段 C 尚未完成。
这些职责分属 RenderCore、Engine 接口与 Renderer，不应全部堆入 RenderCore。
