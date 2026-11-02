#pragma once
#include "ShaderCore.h"

// 控制 Bind() 按名称查询编译后的 ParameterMap 时，是否允许参数不存在。
// 不是 GPU 资源绑定开关，也不负责检查每次 Draw 是否已经提交参数数据。
enum EShaderParameterFlags
{
    // 可选参数：允许未声明或被编译器优化掉，查找失败时保持 IsBound() == false。
    // 当前参数提交辅助函数会跳过未绑定参数；若参数存在，仍须通过类型等检查。
    SPF_Optional,

    // 必需参数：编译后的参数表中必须存在该名称，否则 Bind() 抛出异常。
    // 用于尽早发现名称拼写错误、Shader 接口变化或必需参数被优化掉的问题。
    SPF_Mandatory
};

// Bind 只在初始化时按名称解析位置，不绑定 GPU 数据；Draw 使用缓存的数字索引。
class FShaderUniformBufferParameter
{
public:
    void Bind(const FShaderParameterMap& Map, const std::string& Name, EShaderParameterFlags Flags = SPF_Optional);
    bool IsBound() const { return Allocation.has_value(); }
    uint32 GetBaseIndex() const;
    // 教学扩展：当前以 CPU 字节块上传整个 cbuffer，因此保留大小用于检查。
    uint32 GetNumBytes() const;
private:
    std::optional<FParameterAllocation> Allocation;
};

class FShaderResourceParameter
{
public:
    void Bind(const FShaderParameterMap& Map, const std::string& Name, EShaderParameterFlags Flags = SPF_Optional);
    bool IsBound() const { return Allocation.has_value(); }
    uint32 GetBaseIndex() const;
    uint32 GetNumResources() const;
    EShaderParameterType GetType() const;
private:
    std::optional<FParameterAllocation> Allocation;
};
