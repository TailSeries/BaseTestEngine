#include "LocalVertexFactory.h"
#include "TestMeshResources.h"
#include <fstream>

// 独立验证描述转换；真实单流/双流 Draw 由正常 ScenePass/CompositePass 的图像回归覆盖。
void ValidateStageCVertexFactory()
{
    const auto Require = [](bool Condition, const char* Message)
    {
        if (!Condition) throw std::runtime_error(Message);
    };
    const auto Reject = [&Require](auto&& Operation)
    {
        bool Failed = false;
        try { Operation(); } catch (const std::exception&) { Failed = true; }
        Require(Failed, "C5: invalid vertex factory operation accepted");
    };
    uint8 Interleaved[16 + 3 * 20]{};
    FTestVertexBuffer VertexBuffer;
    Reject([&] { (void)VertexBuffer.GetRHI(); });
    VertexBuffer.SetData(Interleaved, sizeof(Interleaved), 20);
    FLocalVertexFactory::FDataType Data;
    Data.PositionComponent = FVertexStreamComponent(&VertexBuffer, 16, 0, 20, VET_Float3);
    Data.TextureCoordinates.push_back(FVertexStreamComponent(&VertexBuffer, 16, 12, 20, VET_Float2));
    FLocalVertexFactory Factory;
    Factory.SetData(Data);
    Reject([&] { BeginInitResource(&Factory); }); // 必须先初始化依赖的 Buffer。
    Require(!Factory.IsInitialized(), "C5: failed factory init retained state");
    BeginInitResource(&VertexBuffer);
    BeginInitResource(&Factory);
    FVertexDeclarationElementList Elements;
    Require(Factory.GetDeclaration()->GetInitializer(Elements) && Elements.size() == 2,
        "C5: declaration roundtrip failed");
    Require(Elements[0].StreamIndex == 0 && Elements[1].StreamIndex == 0 &&
        Elements[0].Offset == 0 && Elements[1].Offset == 12 &&
        Elements[0].Stride == 20 && Elements[1].Stride == 20 &&
        Elements[0].AttributeIndex == 0 && Elements[1].AttributeIndex == 4,
        "C5: interleaved declaration mapping incorrect");
    const FVertexInputStreamArray Single = Factory.GetStreams();
    Require(Single.size() == 1 && Single[0].Offset == 16 &&
        Single[0].VertexBuffer == VertexBuffer.GetRHI().get(), "C5: stream deduplication/base offset incorrect");
    Reject([&] { Factory.SetData(Data); });
    Reject([&] { VertexBuffer.SetData(Interleaved, sizeof(Interleaved), 20); });
    BeginReleaseResource(&Factory);
    Reject([&] { (void)Factory.GetStreams(); });
    BeginInitResource(&Factory);
    Require(Factory.GetStreams().size() == 1, "C5: factory reinit accumulated old streams");
    BeginReleaseResource(&Factory);

    float Positions[3][3]{}, UVs[3][2]{};
    FTestVertexBuffer PositionBuffer, UVBuffer;
    PositionBuffer.SetData(Positions, sizeof(Positions), 12);
    UVBuffer.SetData(UVs, sizeof(UVs), 8);
    BeginInitResource(&PositionBuffer);
    BeginInitResource(&UVBuffer);
    Data.PositionComponent = FVertexStreamComponent(&PositionBuffer, 0, 12, VET_Float3);
    Data.TextureCoordinates[0] = FVertexStreamComponent(&UVBuffer, 0, 8, VET_Float2);
    FLocalVertexFactory Split;
    Split.SetData(Data);
    BeginInitResource(&Split);
    Elements.clear();
    Require(Split.GetDeclaration()->GetInitializer(Elements) && Elements[0].StreamIndex == 0 &&
        Elements[1].StreamIndex == 1 && Elements[0].Stride == 12 && Elements[1].Stride == 8,
        "C5: separate streams/strides incorrect");
    const FVertexInputStreamArray Streams = Split.GetStreams();
    Require(Streams.size() == 2 && Streams[0].VertexBuffer == PositionBuffer.GetRHI().get() &&
        Streams[1].VertexBuffer == UVBuffer.GetRHI().get(), "C5: split buffer bindings incorrect");
    // Buffer 对象仍活着但其 RHI 引用已释放时，也必须拒绝生成绑定快照。
    BeginReleaseResource(&UVBuffer);
    Reject([&] { (void)Split.GetStreams(); });
    BeginReleaseResource(&Split);
    BeginInitResource(&UVBuffer);
    // 让 Position 成功加入流、UV 越界失败，再修正重试，验证部分流状态被回滚。
    Data.TextureCoordinates[0].StreamOffset = sizeof(UVs);
    Split.SetData(Data);
    Reject([&] { BeginInitResource(&Split); });
    Data.TextureCoordinates[0].StreamOffset = 0;
    Split.SetData(Data);
    BeginInitResource(&Split);
    Require(Split.GetStreams().size() == 2, "C5: partial init rollback retained stale streams");
    BeginReleaseResource(&Split);
    Reject([&] { FVertexStreamComponent Invalid(&VertexBuffer, 256, 20, VET_Float3); });
    Reject([&] { FVertexStreamComponent Invalid(&VertexBuffer, 0, 256, VET_Float3); });
    Data.TextureCoordinates.clear();
    Reject([&] { Split.SetData(Data); });

    uint16 Indices[3]{0, 1, 2};
    FTestIndexBuffer IndexBuffer;
    Reject([&] { IndexBuffer.SetData(Indices, sizeof(Indices), 3); });
    IndexBuffer.SetData(Indices, sizeof(Indices), 2);
    BeginInitResource(&IndexBuffer);
    Require(IndexBuffer.GetRHI()->GetStride() == 2, "C5: index buffer creation incorrect");
    BeginReleaseResource(&IndexBuffer);
    Reject([&] { (void)IndexBuffer.GetRHI(); });
    std::ofstream Report("stage-c-vertex-factory-checks.txt");
    Report << "PASS: buffer lifecycle, dependency order, interleaved stream deduplication, "
        "base offset 16, ATTRIBUTE0/4, split streams/strides, layout guards, rollback and reinit.\n";
    if (!Report) throw std::runtime_error("Cannot write vertex factory validation report");
}
