#include "GlobalShader.h"

namespace
{
std::map<std::string, const FGlobalShaderType*>& GetRegistry()
{
    static std::map<std::string, const FGlobalShaderType*> Registry;
    return Registry;
}
}

bool FGlobalShaderType::Register(const FGlobalShaderType& Type)
{
    const auto [It, Inserted] = GetRegistry().emplace(Type.GetName(), &Type);
    if (!Inserted && It->second != &Type)
        throw std::invalid_argument("Duplicate global shader type: " + Type.GetName());
    return true;
}

std::vector<const FGlobalShaderType*> FGlobalShaderType::GetTypes()
{
    std::vector<const FGlobalShaderType*> Types;
    for (const auto& [Name, Type] : GetRegistry()) Types.push_back(Type);
    return Types; // 返回快照；此阶段不允许并发注册。
}

void FGlobalShaderMap::CompileMissing(const FCompiler& Compiler, const FSourceLoader& LoadSource)
{
    if (!Compiler || !LoadSource) throw std::invalid_argument("Missing shader compiler/source loader");
    // 保留已有对象身份；失败时新增对象尚未提交给 GPU，可正常销毁。
    auto Pending = Entries;
    for (const FGlobalShaderType* Type : FGlobalShaderType::GetTypes())
    {
        if (Pending.contains(Type)) continue;
        try
        {
            const FShaderCompilerInput Input{LoadSource(Type->GetSourceFilename()), Type->GetEntryPoint(), Type->GetFrequency()};
            const FShaderCompilerOutput Output = Compiler(Input);
            if (!Output.bSucceeded) throw std::runtime_error(Output.Diagnostics);
            TRefCountPtr<FShader> Shader = Type->ConstructCompiled(Output);
            TRefCountPtr<FShaderMapResource> Resource = std::make_shared<FShaderMapResource>(Output);
            BeginInitResource(Resource.get());
            Pending.emplace(Type, FEntry{std::move(Shader), std::move(Resource)});
        }
        catch (const std::exception& Error)
        {
            throw std::runtime_error("Global shader " + Type->GetName() + " [" +
                Type->GetSourceFilename() + ":" + Type->GetEntryPoint() + "]: " + Error.what());
        }
    }
    Entries.swap(Pending);
}
