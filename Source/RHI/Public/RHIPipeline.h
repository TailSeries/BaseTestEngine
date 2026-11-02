#pragma once
#include "GenericPlatform.h"
#include "EnumClassFlags.h"
// RHI 层的逻辑执行管线。
// 与 Shader 阶段、Graphics PSO，以及后端的队列类型枚举不同。
enum class ERHIPipeline : uint8
{
    // 图形执行管线，也可以执行计算任务。
    Graphics = 1u << 0,

    // 异步计算执行管线。
    // 当前只保留 UE 的枚举定义，尚未接入跨管线转换与同步。
    AsyncCompute = 1u << 1,

    // 未指定管线。
    None = 0,

    // 两条管线的组合掩码。
    All = Graphics | AsyncCompute,

    // 管线数量，不是用于组合的独立标志位。
    Num = 2,
};
//这个枚举没有 Copy，是因为我们对齐的是 UE 的 ERHIPipeline，它并不是 D3D12 三种队列类型的直接翻译。
ENUM_CLASS_FLAGS(ERHIPipeline)
