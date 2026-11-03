#pragma once
#include "VertexStreamComponent.h"
#include "RHICommandList.h"

// 提交 Draw 时使用的流绑定快照，借用 RHI Buffer，不负责 GPU 生命周期。
struct FVertexInputStream
{
    uint32 StreamIndex;
    uint32 Offset;
    FRHIBuffer* VertexBuffer;
};
using FVertexInputStreamArray = std::vector<FVertexInputStream>;

//解释顶点数据如何进入 Shader，并提供顶点声明与顶点流。
class FVertexFactory : public FRenderResource
{
public:
    ~FVertexFactory() override;
    const TRefCountPtr<FRHIVertexDeclaration>& GetDeclaration() const;
    // 默认只返回借用快照；缓存绘制命令时可同时获取持有底层 Buffer 的引用。
    FVertexInputStreamArray GetStreams(std::vector<TRefCountPtr<FRHIBuffer>>* OutReferences = nullptr) const;
protected:
    // 相同 Buffer、StreamOffset、Stride 复用同一个流，属性 Offset 不参与流去重。
    FVertexElement AccessStreamComponent(const FVertexStreamComponent& Component, uint8 AttributeIndex);
    void InitDeclaration(const FVertexDeclarationElementList& Elements);
    void ReleaseRHI() noexcept override;
private:
    struct FVertexStream
    {
        const FVertexBuffer* VertexBuffer;
        uint32 Offset;
        uint16 Stride;
    };
    std::vector<FVertexStream> Streams;
    TRefCountPtr<FRHIVertexDeclaration> Declaration;
};

// 教学期提交辅助函数；后续由 MeshDrawCommand 保存/提交 GetStreams 的结果。
// 必须先设置与 Factory 顶点声明匹配的 PSO，Stride 仍由当前 PSO 决定。
void SetVertexFactoryStreams(FRHICommandList& CmdList, const FVertexFactory& Factory);
