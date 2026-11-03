#include "MeshPassProcessor.h"
#include "DynamicRHI.h"
#include <cstring>
#include <limits>
#include <iterator>

void FMeshDrawShaderBindings::Add(const FShaderUniformBufferParameter& Parameter,
    EShaderFrequency Frequency, const void* Data, uint32 Size)
{
    if (!Parameter.IsBound()) return;
    if (Frequency != SF_Vertex || Parameter.GetBaseIndex() != 0 || !Data || Size != Parameter.GetNumBytes())
        throw std::invalid_argument("Mesh constants require VS b0 and exact reflected byte size");
    std::vector<uint8> Copy(Size);
    std::memcpy(Copy.data(), Data, Size);
    ConstantData = std::move(Copy);
    ConstantBufferIndex = Parameter.GetBaseIndex();
}
void FMeshDrawShaderBindings::Add(const FShaderResourceParameter& Parameter,
    EShaderFrequency Frequency, TRefCountPtr<FRHIShaderResourceView> View)
{
    if (!Parameter.IsBound()) return;
    if (Frequency != SF_Pixel || Parameter.GetType() != EShaderParameterType::SRV ||
        Parameter.GetBaseIndex() != 0 || Parameter.GetNumResources() != 1 || !View)
        throw std::invalid_argument("Mesh resource binding requires one PS t0 SRV");
    SRV = std::move(View);
    ResourceIndex = Parameter.GetBaseIndex();
}
void FMeshDrawShaderBindings::SetOnCommandList(FRHICommandList& CmdList) const
{
    if (!ConstantData.empty())
        CmdList.SetShaderConstants(ConstantBufferIndex, ConstantData.data(), static_cast<uint32>(ConstantData.size()));
    if (SRV) CmdList.SetShaderResourceViewParameter(ResourceIndex, SRV.get());
}

FMeshPassProcessor::FMeshPassProcessor(const FGraphicsPipelineStateInitializer& InPipeline)
    : PipelineTemplate(InPipeline)
{
    if (InPipeline.PrimitiveType != PT_TriangleList)
        throw std::invalid_argument("Mesh pass currently supports triangle lists only");
    // Factory 决定顶点声明，不使用调用方模板中可能残留的声明。
    PipelineTemplate.BoundShaderState.VertexDeclarationRHI.reset();
}
TRefCountPtr<FRHIGraphicsPipelineState> FMeshPassProcessor::GetPipelineState(const FVertexFactory& Factory)
{
    const TRefCountPtr<FRHIVertexDeclaration>& Declaration = Factory.GetDeclaration();
    const auto It = PipelineCache.find(Declaration.get());
    if (It != PipelineCache.end()) return It->second.Pipeline;
    FGraphicsPipelineStateInitializer Initializer = PipelineTemplate;
    Initializer.BoundShaderState.VertexDeclarationRHI = Declaration;
    TRefCountPtr<FRHIGraphicsPipelineState> Pipeline = RHICreateGraphicsPipelineState(Initializer);
    if (!Pipeline) throw std::runtime_error("Mesh PSO creation failed");
    PipelineCache.emplace(Declaration.get(), FPipelineEntry{Declaration, Pipeline});
    return Pipeline;
}
void FMeshPassProcessor::AddMeshBatch(const FMeshBatch& Mesh, const FMeshDrawShaderBindings& Bindings,
    std::vector<FMeshDrawCommand>& OutCommands)
{
    if (!Mesh.VertexFactory || Mesh.Type != PT_TriangleList || Mesh.Elements.empty())
        throw std::invalid_argument("Invalid triangle mesh batch");
    std::vector<TRefCountPtr<FRHIBuffer>> VertexReferences;
    const FVertexInputStreamArray Streams = Mesh.VertexFactory->GetStreams(&VertexReferences);
    if (Streams.empty()) throw std::invalid_argument("Mesh has no vertex streams");
    std::vector<FMeshDrawCommand> Pending;
    Pending.reserve(Mesh.Elements.size());
    for (const FMeshBatchElement& Element : Mesh.Elements)
    {
        // 现有 RHI Draw 接口只有 IndexCount，不能悄悄忽略不支持的偏移和实例参数。
        if (!Element.IndexBuffer || Element.FirstIndex != 0 || Element.BaseVertexIndex != 0 ||
            Element.NumInstances != 1 || Element.NumPrimitives == 0 ||
            Element.NumPrimitives > std::numeric_limits<uint32>::max() / 3)
            throw std::invalid_argument("Unsupported mesh element range or instance count");
        const TRefCountPtr<FRHIBuffer>& Buffer = Element.IndexBuffer->GetRHI();
        const uint32 Stride = Buffer->GetStride();
        if ((Stride != 2 && Stride != 4) || Element.NumPrimitives * 3 > Buffer->GetSize() / Stride)
            throw std::out_of_range("Mesh index range exceeds index buffer");
        FMeshDrawCommand Command;
        Command.ShaderBindings = Bindings;
        Command.VertexStreams = Streams;
        Command.VertexBufferReferences = VertexReferences;
        Command.IndexBuffer = Buffer;
        Command.NumPrimitives = Element.NumPrimitives;
        Pending.push_back(std::move(Command));
    }
    // 先验证全部元素，再创建/获取 PSO。已有缓存不等于 UE 的完整全局 PSO 缓存。
    const TRefCountPtr<FRHIGraphicsPipelineState> Pipeline = GetPipelineState(*Mesh.VertexFactory);
    for (FMeshDrawCommand& Command : Pending) Command.PipelineState = Pipeline;
    OutCommands.reserve(OutCommands.size() + Pending.size());
    OutCommands.insert(OutCommands.end(), std::make_move_iterator(Pending.begin()), std::make_move_iterator(Pending.end()));
}
void FMeshDrawCommand::SubmitDraw(FRHICommandList& CmdList) const
{
    if (!PipelineState || !IndexBuffer || NumPrimitives == 0)
        throw std::logic_error("Cannot submit an unbuilt mesh command");
    CmdList.SetGraphicsPipelineState(PipelineState.get());
    ShaderBindings.SetOnCommandList(CmdList);
    for (const FVertexInputStream& Stream : VertexStreams)
        CmdList.SetStreamSource(Stream.StreamIndex, Stream.VertexBuffer, Stream.Offset);
    CmdList.DrawIndexedPrimitive(IndexBuffer.get(), NumPrimitives * 3);
}
