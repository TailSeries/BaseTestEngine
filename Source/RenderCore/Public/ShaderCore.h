#pragma once
#include "RHIResources.h"
#include <map>
#include <optional>
#include <string>
#include <stdexcept>

// 对照 UE ShaderCore.h；当前只描述整块常量缓冲和资源，不包含常量缓冲内部成员。
enum class EShaderParameterType : uint8 { UniformBuffer, SRV, Sampler, UAV };

struct FParameterAllocation
{
    // UniformBuffer：BufferIndex 是 b 寄存器，BaseIndex 为 0，Size 为字节数。
    // SRV/Sampler/UAV：BufferIndex 为 0，BaseIndex 是 t/s/u 寄存器，Size 为数组长度。
    // 这些都是 Shader 绑定信息，不是根参数槽位、描述符堆偏移或 GPU 地址。
    uint16 BufferIndex = 0;
    uint16 BaseIndex = 0;
    uint16 Size = 0;
    EShaderParameterType Type = EShaderParameterType::UniformBuffer;
};

class FShaderParameterMap
{
public:
    void AddParameterAllocation(const std::string& Name, uint16 BufferIndex,
        uint16 BaseIndex, uint16 Size, EShaderParameterType Type)
    {
        if (!Parameters.emplace(Name, FParameterAllocation{BufferIndex, BaseIndex, Size, Type}).second)
            throw std::invalid_argument("Duplicate shader parameter: " + Name);
    }

    // 返回值副本，调用方不会持有容器内部悬空指针。
    std::optional<FParameterAllocation> FindParameterAllocation(const std::string& Name) const
    {
        const auto It = Parameters.find(Name);
        if (It == Parameters.end()) return std::nullopt;
        return It->second;
    }
private:
    std::map<std::string, FParameterAllocation> Parameters;
};
