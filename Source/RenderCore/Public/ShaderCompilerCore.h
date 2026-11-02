#pragma once
#include "ShaderCore.h"

// 对照 UE 的编译输入/输出分层；此阶段同步编译内存源码，不含 Worker、include、宏和缓存。
struct FShaderCompilerInput
{
    std::string Source;
    std::string EntryPoint;
    EShaderFrequency Frequency = SF_Vertex;
};

struct FShaderCompilerOutput
{
    // 输出拥有字节码与参数元数据；反射接口在编译函数返回前释放。
    std::vector<uint8> ShaderCode;
    //	按名称查找参数，例如 CB 对应哪个 b 寄存器
    FShaderParameterMap ParameterMap;
    FShaderCodePackedResourceCounts ResourceCounts{};
    EShaderFrequency Frequency = SF_Vertex;
    std::string Diagnostics;
    bool bSucceeded = false;

    // Code 是借用视图：本 Output 必须活到 RHICreate*Shader 返回。
    // UE 把资源统计打包进 ShaderCode 附加数据；当前保留已有 RHI 描述作为过渡。
    FRHICreateShaderDesc MakeRHICreateDesc() const
    {
        if (!bSucceeded) throw std::logic_error("Cannot create RHI shader from failed compilation");
        FRHICreateShaderDesc Desc{std::span<const uint8>(ShaderCode)};
        Desc.ResourceCounts = ResourceCounts;
        return Desc;
    }
};
