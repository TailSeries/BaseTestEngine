#pragma once
#include "ShaderCompilerCore.h"
#include <utility>

// 对照 UE FShaderMapResource：集中持有 RHI Shader，FShader 本身不拥有硬件对象。
// 本节精简为一个 Shader/索引 0，尚未接入 FRenderResource、延迟创建和 ShaderMap。
class FShaderMapResource
{
public:
    explicit FShaderMapResource(const FShaderCompilerOutput& Output);
    const TRefCountPtr<FRHIShader>& GetShader() const { return ShaderRHI; }
private:
    TRefCountPtr<FRHIShader> ShaderRHI;
};

class FShader
{
public:
    // 构造期间借用编译输出，派生类在构造器中 Bind 参数；不保存整个输出的引用。
    using CompiledShaderInitializerType = FShaderCompilerOutput;
    explicit FShader(const CompiledShaderInitializerType& Initializer);
    virtual ~FShader() = default;
    EShaderFrequency GetFrequency() const { return Frequency; }
private:
    EShaderFrequency Frequency;
};

// 对照 UE TShaderRef 的“元信息 + 所属资源”关系。
// 当前共享拥有二者，不实现 UE 的 ShaderMap 指针表、资源索引和序列化。
template<typename ShaderType>
class TShaderRef
{
public:
    TShaderRef() = default;
private:
    template<typename T> friend TShaderRef<T> CreateShader(const FShaderCompilerOutput&);
    TShaderRef(TRefCountPtr<ShaderType> InShader, TRefCountPtr<FShaderMapResource> InResource)
        : Shader(std::move(InShader)), Resource(std::move(InResource))
    {
        if (!Shader || !Resource || Shader->GetFrequency() != Resource->GetShader()->GetFrequency())
            throw std::invalid_argument("Shader metadata/resource stage mismatch");
    }
public:
    const ShaderType* operator->() const
    {
        if (!Shader) throw std::logic_error("Empty shader reference");
        return Shader.get();
    }
    TRefCountPtr<FRHIVertexShader> GetVertexShader() const
    {
        if (operator->()->GetFrequency() != SF_Vertex) throw std::logic_error("Not a vertex shader");
        return std::static_pointer_cast<FRHIVertexShader>(Resource->GetShader());
    }
    TRefCountPtr<FRHIPixelShader> GetPixelShader() const
    {
        if (operator->()->GetFrequency() != SF_Pixel) throw std::logic_error("Not a pixel shader");
        return std::static_pointer_cast<FRHIPixelShader>(Resource->GetShader());
    }
private:
    TRefCountPtr<ShaderType> Shader;
    TRefCountPtr<FShaderMapResource> Resource;
};

// 显式创建入口，确保元信息和 RHI 资源来自同一编译输出。
// 后续由 GlobalShaderMap/类型注册替代应用层手动调用；这里不隐藏编译行为。
template<typename ShaderType>
TShaderRef<ShaderType> CreateShader(const FShaderCompilerOutput& Output)
{
    TRefCountPtr<ShaderType> Shader = std::make_shared<ShaderType>(Output);
    TRefCountPtr<FShaderMapResource> Resource = std::make_shared<FShaderMapResource>(Output);
    return TShaderRef<ShaderType>(std::move(Shader), std::move(Resource));
}
