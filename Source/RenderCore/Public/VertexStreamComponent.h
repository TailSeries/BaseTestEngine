#pragma once
#include "RenderResource.h"
#include <stdexcept>

// 一项顶点属性的数据来源。借用上层 Buffer；拥有者必须让 Buffer 活得比 Factory 更久。
// 当前仅支持逐顶点读取，不包含 UE 的 Instancing/ManualFetch/Overridden 标志。
struct FVertexStreamComponent
{
    const FVertexBuffer* VertexBuffer = nullptr;
    uint32 StreamOffset = 0; // 整条流在 Buffer 内的起始偏移，最终传给 SetStreamSource。
    uint8 Offset = 0;       // 每个顶点内的属性偏移，进入 FVertexElement。
    uint8 Stride = 0;       // 对齐 UE 此结构的 8 位存储；不是从 Buffer 的 Stride 推导。
    EVertexElementType Type = VET_None;

    FVertexStreamComponent() = default;
    FVertexStreamComponent(const FVertexBuffer* Buffer, uint32 InOffset, uint32 InStride, EVertexElementType InType)
        : FVertexStreamComponent(Buffer, 0, InOffset, InStride, InType) {}
    FVertexStreamComponent(const FVertexBuffer* Buffer, uint32 InStreamOffset, uint32 InOffset,
        uint32 InStride, EVertexElementType InType)
        : VertexBuffer(Buffer), StreamOffset(InStreamOffset), Type(InType)
    {
        if (InOffset > 255 || InStride > 255) throw std::out_of_range("Vertex component offset/stride exceeds 8 bits");
        Offset = static_cast<uint8>(InOffset);
        Stride = static_cast<uint8>(InStride);
    }
};
