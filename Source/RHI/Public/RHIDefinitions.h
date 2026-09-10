#pragma once
#include "GenericPlatform.h"
#include "EnumClassFlags.h"

// UE: enum ERHIResourceType : uint8（RHIDefinitions.h，RRT_ 前缀，非 enum class）
// 精简：只留当前用得到的类型，UE 大概有几十种吧，包括各种RayTracing相关的buffer，命名对齐 UE，后续按需扩充
enum ERHIResourceType :uint8
{
    RRT_None = 0,
    RRT_Buffer,
    RRT_Texture,
    RRT_VertexShader,
    RRT_PixelShader,
    RRT_GraphicsPipelineState,
};

// UE: enum class EBufferUsageFlags : uint32（RHIDefinitions.h，位标志）
// 精简：只留当前用得到的几个, 注意这个结构体比较过分，它不仅是堆类型的标志，甚至还是 哪种类型的标志
enum class EBufferUsageFlags : uint32
{
    None = 0,
    Static = 1 << 0,   // 内容基本不变，GPU 读为主（DEFAULT 堆）
    Dynamic = 1 << 1,   // CPU 频繁写（UPLOAD 堆，Map+memcpy）
    VertexBuffer = 1 << 2,
    IndexBuffer = 1 << 3,
    ConstantBuffer = 1 << 4,
};
ENUM_CLASS_FLAGS(EBufferUsageFlags);
