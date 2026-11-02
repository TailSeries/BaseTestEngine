#pragma once
#include "Shader.h"
#include <functional>

// 不依赖材质/VertexFactory 排列的 Shader 基类；Global 不代表单例。
class FGlobalShader : public FShader
{
public:
    explicit FGlobalShader(const CompiledShaderInitializerType& Initializer) : FShader(Initializer) {}
};

class FGlobalShaderType : public FShaderType
{
public:
    using FShaderType::FShaderType;
    // 注册表仅借用静态类型描述，不持有任何 RHI 资源。当前只在启动线程注册/访问。
    static bool Register(const FGlobalShaderType& Type);
    static std::vector<const FGlobalShaderType*> GetTypes();
};

// 当前只支持一个平台、默认排列；不包含 UE 的 MapSection、指针表或磁盘缓存。
// Map 由 App 持有，必须在 GPU 使用结束后、RHIExit 前释放。
class FGlobalShaderMap
{
public:
    using FCompiler = std::function<FShaderCompilerOutput(const FShaderCompilerInput&)>;
    using FSourceLoader = std::function<std::string(const std::string&)>;
    // 只编译当前缺少的类型；成功后整体提交，失败不会污染已有 Map。
    // 编译与读文件回调仅在本次同步调用内使用，RenderCore 不依赖具体 D3D 编译器。
    void CompileMissing(const FCompiler& Compiler, const FSourceLoader& LoadSource);
    size_t GetNumShaders() const { return Entries.size(); }

    template<typename ShaderType>
    TShaderRef<ShaderType> GetShader() const
    {
        const FGlobalShaderType& Type = ShaderType::GetStaticType();
        const auto It = Entries.find(&Type);
        if (It == Entries.end()) throw std::logic_error("Global shader is not compiled: " + Type.GetName());
        TRefCountPtr<ShaderType> Shader = std::dynamic_pointer_cast<ShaderType>(It->second.Shader);
        if (!Shader) throw std::logic_error("Global shader C++ type mismatch: " + Type.GetName());
        return TShaderRef<ShaderType>(std::move(Shader), It->second.Resource);
    }
private:
    struct FEntry
    {
        TRefCountPtr<FShader> Shader;
        TRefCountPtr<FShaderMapResource> Resource;
    };
    std::map<const FGlobalShaderType*, FEntry> Entries;
};

// 保留 UE 同名注册宏的用法，内部是教学实现。
// 与 UE 静态全局注册不同：启动时显式访问 GetStaticType，避免跨翻译单元初始化顺序问题。
#define DECLARE_GLOBAL_SHADER(ShaderClass) \
    public: static const FGlobalShaderType& GetStaticType()

/*
* | 参数 | 含义 | 本例 |
|---|---|---|
| `ShaderClass` | 对应的 C++ Shader 类，负责保存参数绑定信息等 | `FTestVS` |
| `Source` | HLSL 源文件的虚拟路径 | `/Demo/TestShaders.usf` |
| `Entry` | 从源码中的哪个函数开始编译 | `VSMain` |
| `Frequency` | Shader 所属阶段，**不是执行频率** | `SF_Vertex`，顶点阶段 |
*/
#define IMPLEMENT_GLOBAL_SHADER(ShaderClass, Source, Entry, Frequency) \
    const FGlobalShaderType& ShaderClass::GetStaticType() \
    { \
        static const FGlobalShaderType Type(#ShaderClass, Source, Entry, Frequency, \
            [](const FShaderCompilerOutput& Output) -> TRefCountPtr<FShader> \
            { return std::make_shared<ShaderClass>(Output); }); \
        static const bool Registered = FGlobalShaderType::Register(Type); \
        (void)Registered; \
        return Type; \
    }
