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
C2 时 FShaderMapResource 尚未继承 FRenderResource；C4 已接入生命周期基类，仍不实现 UE 的批量资源/异步清理机制。
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

## C3：类型注册与 GlobalShaderMap（2026-11-02）

### 为什么增加这一层

C2 的 CreateShader<T> 仍需要调用方逐个填写入口、阶段、编译和创建。
C3 把这些规则集中到 FShaderType/FGlobalShaderType，FGlobalShaderMap 统一编译缺少的类型并保存实例。
FShaderType 是“如何生成 Shader”的描述，FShader 是某份已编译实例的元信息，二者不同。

### 从应用追调用链

1. TestShaders.h 使用 DECLARE_GLOBAL_SHADER 声明 GetStaticType。
2. TestShaders.cpp 使用 IMPLEMENT_GLOBAL_SHADER 关联类型、虚拟源码路径、入口、Shader 阶段与构造工厂。
3. RegisterTestShaderTypes 显式访问两个静态类型，使其进入注册表；没有编译或 GPU 操作。
4. App 的 GlobalShaderMap.CompileMissing 遍历注册表，跳过已有类型。
5. 源码加载回调把 /Demo/TestShaders.usf 映射到 RHITest/Shaders/TestShaders.usf。
6. 编译回调 CompileD3DShader 生成字节码、参数表、统计；RenderCore 不依赖 ShaderFormatD3D。
7. 类型工厂构造 FTestVS/FTestPS，解析参数；独立 FShaderMapResource 创建对应 RHI Shader。
8. 全部成功后提交到 Map；应用通过 GetShader<FTestVS/FTestPS>() 取 TShaderRef。
9. 后续 PSO 与 Draw 链路沿用 C2；不会每次 Draw 重编译或查字符串。

### 生命周期、缓存与失败行为

- 注册表只借用静态类型描述，不持有 GPU 资源；仅支持启动线程访问。
- App 持有 Map，Map 持有每个条目的元信息和资源。TShaderRef 共享持有，Map 销毁后仍可存活。
- 正常路径仍然先 WaitForGPU，再析构 App，最后 RHIExit；Map 不能替代 GPU Fence。
- 重复 CompileMissing 仅编译新增类型，不检测文件是否变化，不是热重载或磁盘字节码缓存。
- 新条目先进入临时 Map；任何源码加载、编译或创建失败都会丢弃新增条目，原 Map 不变。
- 编译回调必须返回与注册阶段匹配的输出；异常追加类型名、文件路径和入口方便定位。
- 同一静态类型重复注册安全；不同描述使用相同类型名会报错。

### 与 UE 的对应和精简

对应 RenderCore/Public/Shader.h 的 FShaderType，以及 GlobalShader.h 的 FGlobalShaderType、FGlobalShaderMap 和注册宏。
宏名称及声明/实现的使用方式对齐，内部不是直接复制 UE 的实现。
UE 有自动静态注册；本 Demo 显式 RegisterTestShaderTypes，避免静态初始化顺序与静态库链接裁剪问题。
当前只支持一个运行平台和默认排列，没有 permutation、ShaderMapSection、类型序列化、异步编译 Worker、DDC 或热重载。
Map 的每个条目仍使用单 Shader 的 FShaderMapResource；UE 的批量资源容器/索引体系尚未实现。
源码路径由 CMake 注入绝对源码目录，独立于当前工作目录；程序打包后必须部署 Shader 并调整路径。
本节没有增加 #include 解析或修改后自动重新编译能力。

### 验证

2026-11-02：C3 Debug / Release 构建及 C1/C2/C3/A/B 回归全部通过，图像一致，调试队列无警告/错误。

stage-c-map-checks.txt 覆盖重复/冲突注册、未编译查询、重复编译不创建新对象、
第二个 Shader 失败时整体不提交、源码加载失败、阶段错误、失败后重试及资源引用保持。
正常 A/B 双 Pass 现在使用 Map 中的 Shader，继续做图像逐像素比较和调试队列检查。

## C4：FRenderResource 生命周期（2026-11-02）

### 两种资源层次

FRenderResource 是上层资源生命周期基类，不继承 FRHIResource。
FRHIResource 是后端硬件资源抽象；一个 FRenderResource 可以持有一份或多份 RHI 资源。
本节将 FShaderMapResource 改为继承 FRenderResource，因此 C2/C3 中直接创建 RHI Shader 的时机更新为 InitRHI。

### 创建链路

1. FShaderMapResource(Output) 仅复制字节码、资源统计和阶段，不需要借用 Output 后续存储。
2. CreateShader<T> / GlobalShaderMap::CompileMissing 调用 BeginInitResource(Resource.get())。
3. BeginInitResource 取得即时命令列表，同步调用 InitResource。
4. InitResource 检查 RHI 已初始化，进入 Initializing，虚调用派生类 InitRHI。
5. FShaderMapResource::InitRHI 调用 RHICreateVertexShader/PixelShader，成功后状态变为 Initialized。
6. GetShader 只允许访问已初始化资源；未初始化或已释放会报错，避免空指针继续传递。

### 释放链路

BeginReleaseResource → ReleaseResource → 派生 ReleaseRHI → 重置 RHI 引用 → Uninitialized。
释放不会销毁上层 C++ 对象，其保留的 CPU 数据可用于再次 InitResource。
FShaderMapResource 的派生析构器会调用 ReleaseResource；基类析构器只诊断未释放状态。
不能在基类析构时指望虚调用进入已经析构的派生类。
PSO/外部引用仍可持有旧 RHI Shader；重新初始化不会自动更新已有 PSO。

### 与 Fence 的边界

InitResource/ReleaseResource 管理 CPU 对象及其 RHI 引用，不判断 GPU 工作是否完成。
BeginReleaseResource 既不 WaitForGPU，也不自动把对象放入 PendingDeletes。
当前 App 保持 WaitForGPU → 析构 App/Map/TShaderRef → RHIExit 的顺序。
编译失败时新 Shader 从未用于 Draw，因此可直接清理；其他在飞资源仍需显式同步或延迟释放。

### 状态和异常契约

重复 InitResource（已初始化）和 ReleaseResource（未初始化）均不重复执行钩子。
初始化失败会调用不抛异常的 ReleaseRHI 清理部分结果，再恢复 Uninitialized，允许重试。
派生 ReleaseRHI 必须能处理空资源和部分初始化；本节通过 noexcept 声明这一约束。
初始化/释放钩子不得重入生命周期入口；复制和移动 FRenderResource 被禁用。

### UE 对照及简化

参考 Engine/Source/Runtime/RenderCore/Public/RenderResource.h 与 Private/RenderResource.cpp。
保留 InitResource/InitRHI、ReleaseResource/ReleaseRHI 和 BeginInitResource/BeginReleaseResource 的职责。
UE Begin* 安排渲染线程命令；当前同一线程直接执行，没有 RenderCommandPipe、线程切换或 RenderCommandFence。
UE 能注册 RHI 启动前的资源，稍后统一 InitRHI；当前必须先 RHIInit，未实现全局资源列表、初始化阶段及设备重建。
当前 IsInitialized 表示本资源已成功完成 InitRHI；UE 在 RHI 尚未启动时的注册状态含义更丰富。
本节也没有按首次 GetShader 延迟创建 RHI Shader，而是在编译填充 Map 时立即 InitResource。

### 验证

2026-11-02：C4 Debug / Release 构建及 C1～C4、A/B 回归全部通过；图像一致，调试队列无警告/错误，未出现未释放资源诊断。

stage-c-resource-checks.txt 验证初始化/释放幂等、失败回滚、重入拒绝与重试，
编译输出销毁后仍可初始化、访问状态检查、重新创建 RHI Shader、析构释放与外部引用保持。
原有 C1/C2/C3/A/B 回归继续使用真实 GPU 执行验证。

## C5：Buffer 资源与最小 LocalVertexFactory（2026-11-02）

### 模块及职责

- RenderCore/RenderResource.h：FVertexBuffer、FIndexBuffer 继承 FRenderResource，持有 RHI Buffer 引用。
- RHITest/TestMeshResources.h：测试派生类拥有 CPU 字节，SetData 复制、InitRHI 创建 UPLOAD Buffer。
- RenderCore/VertexStreamComponent.h：FVertexStreamComponent 描述单个属性从哪里读取。
- RenderCore/VertexFactory.h：FVertexFactory 管理去重后的顶点流和 RHI 顶点声明。
- Engine/LocalVertexFactory.h：FLocalVertexFactory 组织 Position 与一个 UV 通道；模块归属对齐 UE。

### 从示例追调用链

1. VB/IB.SetData 复制 CPU 数据，BeginInitResource 创建 RHI Buffer。
2. FLocalVertexFactory::FDataType 描述 PositionComponent 与 TextureCoordinates。
3. Factory.SetData 复制布局描述；不复制/拥有 Buffer。
4. BeginInitResource(Factory) → InitRHI → AccessStreamComponent，为属性分配或复用流槽位。
5. InitDeclaration 创建 RHI 顶点声明，交给 PSO 的 BoundShaderState。
6. Draw 先设置匹配的 PSO，再调用 SetVertexFactoryStreams。
7. GetStreams 查询 Buffer 当前 RHI 对象并生成流绑定快照，辅助函数逐项调用 SetStreamSource。
8. 索引缓冲独立交给 DrawIndexedPrimitive，不属于顶点属性流。

### 两种偏移不能混淆

StreamOffset 是整条流在 Buffer 中的起点，最终传给 SetStreamSource。
Offset 是每个顶点内的属性偏移，进入 FVertexElement.Offset。
Stride 来自 Component/顶点声明，不能由 Buffer 的 Stride 自动决定。

立方体使用前缀 16 字节：Position {StreamOffset=16, Offset=0, Stride=20}；
UV {StreamOffset=16, Offset=12, Stride=20}，共享一个 Buffer，因此去重为同一个 Stream 0。
流去重依据 Buffer 指针、StreamOffset、Stride，不比较每个属性的 Offset。
全屏网格 Position 使用一个 Buffer（Stride=12），UV 使用另一个 Buffer（Stride=8），生成 Stream 0/1。
两个都是逐顶点流，索引缓冲中的同一个索引分别取两个流中的对应数据。

### Shader 接口

对齐 UE LocalVertexFactory 的语义编号：Position=ATTRIBUTE0，第一组 UV=ATTRIBUTE4。
TestShaders.usf 已同步更新，RHI 顶点声明允许语义索引不连续。
1/2/3 的切线/颜色输入尚未实现，因此当前声明只包含 0 和 4。
全屏 PSO 必须使用全屏 Factory 的声明，不能再复用立方体的交错单流声明。
Shader 字节码可以相同，因为输入语义相同，差别由 PSO 输入布局和流绑定处理。

### 所有权与生命周期

Factory 和 Component 借用 FVertexBuffer 指针，FVertexInputStream 借用 FRHIBuffer 指针。
拥有者必须保证 Buffer 对象活得比 Factory/流快照更久，GPU 使用完成前还要保留底层 RHI 资源。
App 的成员顺序保证 Factory 先析构，Buffer 后析构，退出前仍先 WaitForGPU。
初始化顺序是 Buffer → Factory；更新布局必须先释放 Factory，再 SetData/初始化并重建相应 PSO。
重新创建不同大小/布局的 Buffer 后同样需要重新验证 Factory，不能只靠旧快照继续绘制。
FVertexBuffer/FIndexBuffer 基类析构清理自身 Buffer 引用；派生类若增加其他 RHI 成员，必须在自己的析构器先 ReleaseResource。

### 简化边界

这不是完整 UE VertexFactory：尚无 FVertexFactoryType 注册、.ush 接口注入、材质排列、法线/切线/颜色、多 UV、Instancing、ManualFetch 或 GPUScene。
当前 LocalVertexFactory 只支持 float3 Position 与一个 float2 UV；RHI 本身已有的整数格式能力不受此限制影响。
Component 按 UE 使用 8 位 Offset/Stride，超出 255 明确拒绝；RHI FVertexElement 的 Stride 仍为 16 位。
没有把 Offset+属性大小<=Stride 当作通用约束；只检查初始化时第一个属性是否落在 Buffer 内。
完整 Draw 顶点/索引范围和当前 PSO 是否匹配仍由调用方保证。
GetStreams 的借用快照目前立即提交；后续 MeshDrawCommand 缓存必须另外处理资源存活/更新。

### 验证

2026-11-02：C5 Debug / Release 构建与 C1～C5、A/B 回归全部通过，单流场景和双流合成逐像素一致，调试队列无警告/错误。

stage-c-vertex-factory-checks.txt 覆盖 Buffer 生命周期、依赖初始化顺序、单流去重、16 字节流偏移、
ATTRIBUTE0/4、双流与各自 Stride、越界/布局修改拒绝、初始化失败回滚和重新初始化。
实际 ScenePass 使用单流，CompositePass 使用双流，原有逐像素图像比较验证真实 GPU 输入。

## C6：MeshBatch → MeshPassProcessor → MeshDrawCommand（2026-11-02）

### 职责与模块

- Engine/MeshBatch.h：FMeshBatch 借用 VertexFactory，Elements 借用 IndexBuffer 并描述图元数量。
- Renderer/MeshPassProcessor.h：FMeshPassProcessor 保存一个 Pass 的 Shader/固定状态模板，组织命令构建。
- FMeshDrawShaderBindings：复制常量字节，持有 SRV，并保存从反射参数对象解析出的绑定索引。
- FMeshDrawCommand：持有 PSO、ShaderBindings、顶点流快照、顶点/索引 Buffer 引用和图元数量。
- RHITest::BuildMeshCommands：当前应用层把 WVP、纹理和网格组合起来；尚未实现 MaterialRenderProxy 或 SceneView。

### 实际调用链

1. 初始化时给 SceneProcessor、CompositeProcessor 分别配置固定状态与 Shader。
2. GetPipelineState 使用 Factory 的声明创建 PSO，并在该 Processor 内按声明身份复用。
3. 每帧 BuildMeshCommands 组织 FMeshBatch：立方体 NumPrimitives=12，全屏四边形=2。
4. 通过 Shader 的参数成员向 FMeshDrawShaderBindings 写入 WVP 副本和 SRV 引用。
5. AddMeshBatch 获取顶点流及 RHI 引用，检查全部 Element 的索引范围和受支持参数。
6. 每个 Element 生成一个命令，全部成功后才追加到输出数组。
7. 在 BeginRenderPass/EndRenderPass 之间调用 SubmitDraw：设置 PSO → 常量/SRV → 顶点流 → DrawIndexedPrimitive。
8. 当前 TriangleList 的 IndexCount = NumPrimitives * 3；12 个三角形对应 36 个索引。

### 生命周期

MeshBatch 借用的上层对象只须覆盖命令构建阶段。
GetStreams 默认仍返回借用快照；C6 增加可选 OutReferences，在同一次查询中取得资源共享引用。
MeshDrawCommand 保存这些引用，因此上层 Factory/Buffer 对象释放后，命令中的 FRHIBuffer 指针仍有效。
常量数据是字节副本，不保存局部 FrameCB 指针；后续修改源常量或源 Bindings 不会影响已构建命令。
命令持有引用只覆盖其 CPU 生命周期，不代表 GPU 完成；命令销毁后仍需其他所有者或 Fence 延迟释放保护。
当前 App 持有常驻 Buffer/PSO，换纹理沿用 DeferredDelete，退出前仍 WaitForGPU。

### 当前边界及 UE 差异

对照 UE Engine/Public/MeshBatch.h 和 Renderer/Public/MeshPassProcessor.h。
保留网格批次、Pass 处理、绑定快照和绘制命令分层，但并未照搬 UE 的全部字段和函数签名。
当前 FMeshPassProcessor 是可直接实例化的固定状态处理器，AddMeshBatch 接收外部提供的绑定快照，
尚未包含 SceneProxy/MaterialRenderProxy 选择 Shader、材质排列、DrawListContext、排序、合批、实例化或缓存命令失效处理。
Processor 的 PSO 缓存仅对自身固定状态模板有效，不是 UE 的全局 PipelineStateCache。
UE 的 MeshDrawCommand 常通过 PipelineId 和外部资源生命周期体系管理引用；本节直接共享持有 PSO/Buffer/SRV。

只支持 TriangleList、FirstIndex=0、BaseVertexIndex=0、NumInstances=1，明确拒绝其他值，不忽略字段。
索引计数不能超过 Buffer 容量；本节不扫描索引内容验证每个顶点索引是否落在所有流的有效范围内。
ShaderBindings 仅支持 VS b0 与 PS t0，静态采样器仍在根签名中；调用方须传入与 Processor Shader 匹配的参数。
不自动验证所有必需 Shader 参数是否已提供，也不从 SRV 自动推导 Transition。
命令不拥有 RenderPass，不做资源状态转换或 Present；这些仍由当前帧流程负责，自动 Pass 依赖留给阶段 D。
命令每帧构建；尚未实现跨帧静态命令缓存。一个 Batch 的全部 Elements 当前共用一份绑定快照。

### 验证

2026-11-02：C6 Debug / Release 构建与 C1～C6、A/B 回归全部通过。121 帧后离屏与合成结果逐像素一致，D3D12 调试队列无警告/错误。

stage-c-mesh-draw-checks.txt 验证常量复制、源 Bindings 修改隔离、PSO 复用、
上层对象销毁后的 RHI 引用保持、多 Element 生成、范围/实例/溢出拒绝、失败批次不部分追加及最终释放。
正常单流 ScenePass/双流 CompositePass 全部改走命令提交，并继续运行 C1～C5、A/B GPU 图像回归。

### 接下来

引入 Scene/View、网格与简单材质参数，建立多物体收集和按 Pass 组织提交的最小 Renderer。
参数结构体自动绑定、Shader permutation、多寄存器绑定和完整 VertexFactory Shader 接口仍待实现；阶段 C 尚未完成。
