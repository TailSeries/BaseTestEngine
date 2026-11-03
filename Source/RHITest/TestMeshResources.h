#pragma once
#include "RenderResource.h"
#include "DynamicRHI.h"
#include <cstring>

// 测试网格的 CPU 数据拥有者；基类负责 RHI 引用，派生类选择当前已验证的 UPLOAD 路径。
// Dynamic 在此表示 CPU 可写的上传堆，并不意味着数据一定每帧变化。
class FTestVertexBuffer : public FVertexBuffer
{
public:
    void SetData(const void* Source, uint32 Size, uint32 InStride)
    {
        if (IsInitialized() || !Source || !Size) throw std::invalid_argument("Invalid vertex upload data/state");
        Data.resize(Size);
        std::memcpy(Data.data(), Source, Size);
        Stride = InStride;
    }
protected:
    void InitRHI(FRHICommandListBase&) override
    {
        if (Data.empty()) throw std::logic_error("SetData before initializing vertex buffer");
        VertexBufferRHI = RHICreateBuffer(FRHIBufferDesc(static_cast<uint32>(Data.size()), Stride,
            EBufferUsageFlags::VertexBuffer | EBufferUsageFlags::Dynamic), Data.data());
    }
private:
    std::vector<uint8> Data;
    uint32 Stride = 0;
};
class FTestIndexBuffer : public FIndexBuffer
{
public:
    void SetData(const void* Source, uint32 Size, uint32 InStride)
    {
        if (IsInitialized() || !Source || !Size || (InStride != 2 && InStride != 4) || Size % InStride)
            throw std::invalid_argument("Invalid index upload data/state");
        Data.resize(Size);
        std::memcpy(Data.data(), Source, Size);
        Stride = InStride;
    }
protected:
    void InitRHI(FRHICommandListBase&) override
    {
        if (Data.empty()) throw std::logic_error("SetData before initializing index buffer");
        IndexBufferRHI = RHICreateBuffer(FRHIBufferDesc(static_cast<uint32>(Data.size()), Stride,
            EBufferUsageFlags::IndexBuffer | EBufferUsageFlags::Dynamic), Data.data());
    }
private:
    std::vector<uint8> Data;
    uint32 Stride = 0;
};
