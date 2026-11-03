#pragma once
#include "MeshBatch.h"
#include "ShaderParameters.h"
#include <map>
#include <span>

// 最小 Shader 绑定快照：只支持当前 VS 根 CBV b0 与 PS SRV t0。
// 常量拥有字节副本，SRV 共享持有；不是把局部变量指针留到 SubmitDraw 再读取。
class FMeshDrawShaderBindings
{
public:
    // 参数位置 + 当前要绑定的数据
    void Add(const FShaderUniformBufferParameter& Parameter, EShaderFrequency Frequency, const void* Data, uint32 Size);
    void Add(const FShaderResourceParameter& Parameter, EShaderFrequency Frequency, TRefCountPtr<FRHIShaderResourceView> View);
    void SetOnCommandList(FRHICommandList& CmdList) const;
    std::span<const uint8> GetConstantData() const { return ConstantData; }
    const TRefCountPtr<FRHIShaderResourceView>& GetSRV() const { return SRV; }
private:
    uint32 ConstantBufferIndex = 0;
    std::vector<uint8> ConstantData;
    uint32 ResourceIndex = 0;
    TRefCountPtr<FRHIShaderResourceView> SRV;
};

// 	这一次 Draw 最终要绑定什么资源、使用什么 PSO、画多少索引？
// Renderer 层的具体绘制快照；不拥有 RenderPass，不负责 Transition 或 Present。
// 与 UE 的 PipelineId/外部生命周期管理不同，教学版直接共享持有 PSO 和资源引用。
class FMeshDrawCommand
{
public:
    void SubmitDraw(FRHICommandList& CmdList) const;
    uint32 GetNumPrimitives() const { return NumPrimitives; }
    const FMeshDrawShaderBindings& GetShaderBindings() const { return ShaderBindings; }
    const FVertexInputStreamArray& GetVertexStreams() const { return VertexStreams; }
    const TRefCountPtr<FRHIGraphicsPipelineState>& GetPipelineState() const { return PipelineState; }
private:
    friend class FMeshPassProcessor;
    FMeshDrawShaderBindings ShaderBindings; // 	常量数据、SRV、绑定位置
    FVertexInputStreamArray VertexStreams; // 	StreamIndex、Offset、Buffer 指针 借用下方 VertexBufferReferences 所持有的资源。
    std::vector<TRefCountPtr<FRHIBuffer>> VertexBufferReferences; // 	顶点 Buffer 引用
    TRefCountPtr<FRHIBuffer> IndexBuffer; // 	底层索引 Buffer
    TRefCountPtr<FRHIGraphicsPipelineState> PipelineState; // 	本次绘制使用的 PSO
    uint32 NumPrimitives = 0; // 	图元数量
};

// 在当前 Pass 中，使用什么 Shader、状态，把它加工成什么命令？
// 当前固定一个 Pass 的 Shader 和渲染状态，按 VertexFactory 声明复用 PSO。
// 尚未实现 UE 的材质选择、MeshDrawCommandContext、排序、合批或可见性处理。
class FMeshPassProcessor  
{
public:
    explicit FMeshPassProcessor(const FGraphicsPipelineStateInitializer& InPipeline);

    // 其实是 GetOrCreate 
    TRefCountPtr<FRHIGraphicsPipelineState> GetPipelineState(const FVertexFactory& Factory);
    // 教学签名：接收已解析的参数快照。每个 Element 生成一个 Draw，失败不追加部分结果。
    void AddMeshBatch(const FMeshBatch& Mesh, const FMeshDrawShaderBindings& Bindings,
        std::vector<FMeshDrawCommand>& OutCommands);
private:
    // 保存当前 Pass 的配置模板。
    FGraphicsPipelineStateInitializer PipelineTemplate;
    struct FPipelineEntry
    {
        TRefCountPtr<FRHIVertexDeclaration> Declaration; // 保持作为 Key 的对象地址有效。
        TRefCountPtr<FRHIGraphicsPipelineState> Pipeline;
    };
    std::map<const FRHIVertexDeclaration*, FPipelineEntry> PipelineCache;
};
