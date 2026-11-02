#pragma once
#include "ShaderCompilerCore.h"
#include "RenderResource.h"
#include <utility>

// 对照 UE FShaderMapResource：集中持有 RHI Shader，FShader 本身不拥有硬件对象。
// 每个容器仍只管理一个 Shader；GlobalShaderMap 每个条目持有一个容器。
// 生命周期已接入 FRenderResource；尚未实现批量资源索引与首次使用时按需创建。
class FShaderMapResource : public FRenderResource
{
public:
    // 构造只复制 CPU 数据，尚不创建 RHI Shader，因此不借用 Output 的存储。
    explicit FShaderMapResource(const FShaderCompilerOutput& Output);
    ~FShaderMapResource() override;
    const TRefCountPtr<FRHIShader>& GetShader() const;
protected:
    void InitRHI(FRHICommandListBase& RHICmdList) override;
    void ReleaseRHI() noexcept override;
private:
    // 保留创建数据以支持 ReleaseResource 后再次 InitResource；还没有独立 ShaderCode 共享库。
    std::vector<uint8> ShaderCode;
    FShaderCodePackedResourceCounts ResourceCounts{};
    EShaderFrequency Frequency;
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
    friend class FGlobalShaderMap;
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
// 普通应用现由 GlobalShaderMap 管理；此入口保留给独立创建和底层验证。
template<typename ShaderType>
TShaderRef<ShaderType> CreateShader(const FShaderCompilerOutput& Output)
{
    TRefCountPtr<ShaderType> Shader = std::make_shared<ShaderType>(Output);
    TRefCountPtr<FShaderMapResource> Resource = std::make_shared<FShaderMapResource>(Output);
    BeginInitResource(Resource.get());
    return TShaderRef<ShaderType>(std::move(Shader), std::move(Resource));
}

// Shader 类型描述，不是某一份编译后的 Shader 实例。
// 名称、源码路径、入口、阶段和工厂共同决定“如何构造这一类 Shader”。
class FShaderType
{
public:
    using ConstructCompiledType = TRefCountPtr<FShader> (*)(const FShaderCompilerOutput&);
    FShaderType(std::string InName, std::string InSource, std::string InEntry,
        EShaderFrequency InFrequency, ConstructCompiledType InConstruct)
        : Name(std::move(InName)), SourceFilename(std::move(InSource)),
          EntryPoint(std::move(InEntry)), Frequency(InFrequency), Construct(InConstruct)
    {
        if (Name.empty() || SourceFilename.empty() || EntryPoint.empty() || !Construct)
            throw std::invalid_argument("Incomplete shader type description");
    }
    virtual ~FShaderType() = default;
    const std::string& GetName() const { return Name; }
    const std::string& GetSourceFilename() const { return SourceFilename; }
    const std::string& GetEntryPoint() const { return EntryPoint; }
    EShaderFrequency GetFrequency() const { return Frequency; }
    TRefCountPtr<FShader> ConstructCompiled(const FShaderCompilerOutput& Output) const
    {
        if (!Output.bSucceeded || Output.Frequency != Frequency)
            throw std::invalid_argument("Compiler output does not match shader type: " + Name);
        TRefCountPtr<FShader> Shader = Construct(Output);
        if (!Shader || Shader->GetFrequency() != Frequency)
            throw std::invalid_argument("Shader factory returned wrong stage: " + Name);
        return Shader;
    }
private:
    std::string Name, SourceFilename, EntryPoint;
    EShaderFrequency Frequency;
    ConstructCompiledType Construct;
};
