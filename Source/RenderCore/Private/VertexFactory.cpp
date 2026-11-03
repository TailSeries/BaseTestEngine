#include "VertexFactory.h"
#include "DynamicRHI.h"
#include <limits>

FVertexFactory::~FVertexFactory() { ReleaseResource(); }

FVertexElement FVertexFactory::AccessStreamComponent(const FVertexStreamComponent& Component, uint8 AttributeIndex)
{
    if (!Component.VertexBuffer) throw std::invalid_argument("Vertex component has no buffer");
    const TRefCountPtr<FRHIBuffer>& Buffer = Component.VertexBuffer->GetRHI();
    uint32 ElementSize = 0;
    switch (Component.Type)
    {
    case VET_Float1: ElementSize = 4; break;
    case VET_Float2: ElementSize = 8; break;
    case VET_Float3: ElementSize = 12; break;
    case VET_Float4: ElementSize = 16; break;
    default: throw std::invalid_argument("This vertex factory currently supports float components only");
    }
    // 检查至少能访问第一个属性；Draw 的完整索引范围仍是调用方职责。
    // 不要求 Offset+ElementSize <= Stride，以保留零步长等读取方式。
    if (static_cast<uint64>(Component.StreamOffset) + Component.Offset + ElementSize > Buffer->GetSize())
        throw std::out_of_range("Vertex component exceeds buffer size");
    size_t StreamIndex = 0;
    for (; StreamIndex < Streams.size(); ++StreamIndex)
    {
        const FVertexStream& Stream = Streams[StreamIndex];
        if (Stream.VertexBuffer == Component.VertexBuffer && Stream.Offset == Component.StreamOffset &&
            Stream.Stride == Component.Stride) break;
    }
    if (StreamIndex > std::numeric_limits<uint8>::max()) throw std::out_of_range("Too many vertex streams");
    if (StreamIndex == Streams.size())
        Streams.push_back({Component.VertexBuffer, Component.StreamOffset, Component.Stride});
    return FVertexElement(static_cast<uint8>(StreamIndex), Component.Offset, Component.Type,
        AttributeIndex, Component.Stride);
}

void FVertexFactory::InitDeclaration(const FVertexDeclarationElementList& Elements)
{
    Declaration = RHICreateVertexDeclaration(Elements);
    if (!Declaration) throw std::runtime_error("Vertex declaration creation failed");
}
void FVertexFactory::ReleaseRHI() noexcept
{
    Declaration.reset();
    Streams.clear(); // 不拥有 Buffer，不在这里释放 Buffer。
}
const TRefCountPtr<FRHIVertexDeclaration>& FVertexFactory::GetDeclaration() const
{
    if (!IsInitialized() || !Declaration) throw std::logic_error("Vertex factory is not initialized");
    return Declaration;
}
FVertexInputStreamArray FVertexFactory::GetStreams(std::vector<TRefCountPtr<FRHIBuffer>>* OutReferences) const
{
    (void)GetDeclaration();
    FVertexInputStreamArray Result;
    std::vector<TRefCountPtr<FRHIBuffer>> References;
    for (size_t Index = 0; Index < Streams.size(); ++Index)
    {
        const FVertexStream& Stream = Streams[Index];
        const TRefCountPtr<FRHIBuffer>& Reference = Stream.VertexBuffer->GetRHI();
        FRHIBuffer* Buffer = Reference.get();
        if (Stream.Offset >= Buffer->GetSize()) throw std::out_of_range("Vertex stream offset exceeds buffer");
        Result.push_back({static_cast<uint32>(Index), Stream.Offset, Buffer});
        if (OutReferences) References.push_back(Reference);
    }
    if (OutReferences) *OutReferences = std::move(References);
    return Result;
}
void SetVertexFactoryStreams(FRHICommandList& CmdList, const FVertexFactory& Factory)
{
    // 先取完整快照并验证所有 Buffer，避免 Buffer 未初始化时只提交了部分流。
    const FVertexInputStreamArray Streams = Factory.GetStreams();
    for (const FVertexInputStream& Stream : Streams)
        CmdList.SetStreamSource(Stream.StreamIndex, Stream.VertexBuffer, Stream.Offset);
}
