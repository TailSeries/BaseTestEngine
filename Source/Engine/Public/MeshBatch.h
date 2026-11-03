#pragma once
#include "VertexFactory.h"
//可以类比，但不能把它们严格等同于 Mesh 和 SubMesh. Mesh/SubMesh 通常描述模型资产如何划分，MeshBatch/Element 描述一次渲染如何组织。 FMeshBatch 不拥有模型资产，而是引用绘制所需的数据。



// Engine 层的网格描述，不包含后端命令。指针只在构建 MeshDrawCommand 时借用。
struct FMeshBatchElement
{
    const FIndexBuffer* IndexBuffer = nullptr;
    uint32 FirstIndex = 0;
    uint32 NumPrimitives = 0; // 图元数，不是索引数；当前三角形列表每个图元用 3 个索引。
    int32 BaseVertexIndex = 0;
    uint32 NumInstances = 1;
};

// 表达 这个网格有哪些顶点流、索引数据，要画多少图元？
struct FMeshBatch
{
    const FVertexFactory* VertexFactory = nullptr;
    std::vector<FMeshBatchElement> Elements;
    EPrimitiveType Type = PT_TriangleList;
    // 当前材质参数由 Pass 外部提供；后续接入 UE 风格 MaterialRenderProxy 与 SceneProxy。
};
