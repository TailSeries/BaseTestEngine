#include "MeshPassProcessor.h"
#include "LocalVertexFactory.h"
#include "TestMeshResources.h"
#include <fstream>
#include <limits>
#include <type_traits>

void ValidateStageCMeshDraw(const FGraphicsPipelineStateInitializer& Pipeline,
    const FShaderUniformBufferParameter& ConstantsParameter,
    const FShaderResourceParameter& TextureParameter,
    const TRefCountPtr<FRHIShaderResourceView>& Texture)
{
    static_assert(std::is_nothrow_move_constructible_v<FMeshDrawCommand>);
    const auto Require = [](bool Condition, const char* Message)
    {
        if (!Condition) throw std::runtime_error(Message);
    };
    const auto Reject = [&Require](auto&& Operation)
    {
        bool Failed = false;
        try { Operation(); } catch (const std::exception&) { Failed = true; }
        Require(Failed, "C6: invalid mesh operation accepted");
    };
    FRHICommandList NoContext(nullptr);
    Reject([&] { FMeshDrawCommand{}.SubmitDraw(NoContext); });
    std::vector<FMeshDrawCommand> Commands;
    std::weak_ptr<FRHIBuffer> WeakVB, WeakIB;
    std::weak_ptr<FRHIGraphicsPipelineState> WeakPSO;
    {
        FTestVertexBuffer VB;
        FTestIndexBuffer IB;
        float Vertices[3][5]{};
        uint16 Indices[3]{0, 1, 2};
        VB.SetData(Vertices, sizeof(Vertices), 20);
        IB.SetData(Indices, sizeof(Indices), 2);
        BeginInitResource(&VB);
        BeginInitResource(&IB);
        WeakVB = VB.GetRHI();
        WeakIB = IB.GetRHI();
        FLocalVertexFactory Factory;
        FLocalVertexFactory::FDataType Data;
        Data.PositionComponent = FVertexStreamComponent(&VB, 0, 20, VET_Float3);
        Data.TextureCoordinates.push_back(FVertexStreamComponent(&VB, 12, 20, VET_Float2));
        Factory.SetData(Data);
        BeginInitResource(&Factory);
        FMeshPassProcessor Processor(Pipeline);
        const auto FirstPSO = Processor.GetPipelineState(Factory);
        WeakPSO = FirstPSO;
        Require(FirstPSO == Processor.GetPipelineState(Factory), "C6: processor did not reuse PSO");
        FMeshBatch Mesh;
        Mesh.VertexFactory = &Factory;
        Mesh.Elements.push_back({&IB, 0, 1, 0, 1});
        FMeshDrawShaderBindings Bindings;
        std::vector<uint8> Bytes(ConstantsParameter.GetNumBytes(), 0x2a);
        Reject([&] { Bindings.Add(ConstantsParameter, SF_Pixel, Bytes.data(), static_cast<uint32>(Bytes.size())); });
        Reject([&] { Bindings.Add(ConstantsParameter, SF_Vertex, Bytes.data(), 1); });
        Reject([&] { Bindings.Add(TextureParameter, SF_Pixel, nullptr); });
        Bindings.Add(ConstantsParameter, SF_Vertex, Bytes.data(), static_cast<uint32>(Bytes.size()));
        Bindings.Add(TextureParameter, SF_Pixel, Texture);
        Bytes[0] = 0x7f;
        Processor.AddMeshBatch(Mesh, Bindings, Commands);
        Require(Commands.size() == 1 && Commands[0].GetNumPrimitives() == 1 &&
            Commands[0].GetShaderBindings().GetConstantData()[0] == 0x2a &&
            Commands[0].GetShaderBindings().GetSRV() == Texture,
            "C6: command did not snapshot constants/resources");
        Bindings.Add(ConstantsParameter, SF_Vertex, Bytes.data(), static_cast<uint32>(Bytes.size()));
        Require(Commands[0].GetShaderBindings().GetConstantData()[0] == 0x2a,
            "C6: source binding mutation changed built command");
        // 第二个元素非法：不得先把第一个元素追加到调用方数组。
        Mesh.Elements.push_back({&IB, 0, 2, 0, 1});
        Reject([&] { Processor.AddMeshBatch(Mesh, Bindings, Commands); });
        Require(Commands.size() == 1, "C6: failed batch appended partial commands");
        Mesh.Elements.pop_back();
        Mesh.Elements[0].FirstIndex = 1;
        Reject([&] { Processor.AddMeshBatch(Mesh, Bindings, Commands); });
        Mesh.Elements[0].FirstIndex = 0;
        Mesh.Elements[0].BaseVertexIndex = 1;
        Reject([&] { Processor.AddMeshBatch(Mesh, Bindings, Commands); });
        Mesh.Elements[0].BaseVertexIndex = 0;
        Mesh.Elements[0].NumInstances = 2;
        Reject([&] { Processor.AddMeshBatch(Mesh, Bindings, Commands); });
        Mesh.Elements[0].NumInstances = 1;
        Mesh.Elements[0].NumPrimitives = std::numeric_limits<uint32>::max();
        Reject([&] { Processor.AddMeshBatch(Mesh, Bindings, Commands); });
        Mesh.Elements[0].NumPrimitives = 0;
        Reject([&] { Processor.AddMeshBatch(Mesh, Bindings, Commands); });
        Mesh.Elements[0].NumPrimitives = 1;
        Mesh.Elements.push_back(Mesh.Elements[0]);
        Processor.AddMeshBatch(Mesh, Bindings, Commands);
        Require(Commands.size() == 3, "C6: multi-element mesh did not produce one draw per element");
        Mesh.VertexFactory = nullptr;
        Reject([&] { Processor.AddMeshBatch(Mesh, Bindings, Commands); });
    }
    // 上层 Factory、Buffer、Processor 全部销毁，命令拥有的 RHI 引用必须仍有效。
    Require(!WeakVB.expired() && !WeakIB.expired() && !WeakPSO.expired(), "C6: commands lost resource ownership");
    Require(Commands[0].GetVertexStreams()[0].VertexBuffer == WeakVB.lock().get(),
        "C6: vertex snapshot no longer references retained buffer");
    Commands.clear();
    Require(WeakVB.expired() && WeakIB.expired() && WeakPSO.expired(), "C6: command resources leaked");
    std::ofstream Report("stage-c-mesh-draw-checks.txt");
    Report << "PASS: constant/resource snapshots, PSO reuse, retained buffers, multi-element draws, "
        "range/instance rejection, atomic failed batch and release after command destruction.\n";
    if (!Report) throw std::runtime_error("Cannot write mesh draw validation report");
}
