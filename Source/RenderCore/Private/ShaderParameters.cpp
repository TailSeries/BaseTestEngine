#include "ShaderParameters.h"

namespace
{
std::optional<FParameterAllocation> Find(const FShaderParameterMap& Map, const std::string& Name, EShaderParameterFlags Flags)
{
    std::optional<FParameterAllocation> Result = Map.FindParameterAllocation(Name);
    if (!Result && Flags == SPF_Mandatory)
        throw std::invalid_argument("Required shader parameter missing (possibly optimized out): " + Name);
    return Result;
}
const FParameterAllocation& Require(const std::optional<FParameterAllocation>& Allocation)
{
    if (!Allocation) throw std::logic_error("Shader parameter is not bound");
    return *Allocation;
}
}

void FShaderUniformBufferParameter::Bind(const FShaderParameterMap& Map, const std::string& Name, EShaderParameterFlags Flags)
{
    Allocation.reset(); // 重新绑定失败后不能意外沿用旧位置。
    const std::optional<FParameterAllocation> Result = Find(Map, Name, Flags);
    if (Result && (Result->Type != EShaderParameterType::UniformBuffer || !Result->Size))
        throw std::invalid_argument("Expected nonempty uniform buffer: " + Name);
    Allocation = Result;
}
uint32 FShaderUniformBufferParameter::GetBaseIndex() const { return Require(Allocation).BufferIndex; }
uint32 FShaderUniformBufferParameter::GetNumBytes() const { return Require(Allocation).Size; }

void FShaderResourceParameter::Bind(const FShaderParameterMap& Map, const std::string& Name, EShaderParameterFlags Flags)
{
    Allocation.reset();
    const std::optional<FParameterAllocation> Result = Find(Map, Name, Flags);
    if (Result && (Result->Type == EShaderParameterType::UniformBuffer || !Result->Size))
        throw std::invalid_argument("Expected shader resource: " + Name);
    Allocation = Result;
}
uint32 FShaderResourceParameter::GetBaseIndex() const { return Require(Allocation).BaseIndex; }
uint32 FShaderResourceParameter::GetNumResources() const { return Require(Allocation).Size; }
EShaderParameterType FShaderResourceParameter::GetType() const { return Require(Allocation).Type; }
