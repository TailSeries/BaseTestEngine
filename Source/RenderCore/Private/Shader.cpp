#include "Shader.h"
#include "DynamicRHI.h"

FShader::FShader(const CompiledShaderInitializerType& Initializer) : Frequency(Initializer.Frequency)
{
    if (!Initializer.bSucceeded || Initializer.ShaderCode.empty())
        throw std::invalid_argument("Shader requires successful nonempty compiler output");
}

FShaderMapResource::FShaderMapResource(const FShaderCompilerOutput& Output)
    : ShaderCode(Output.ShaderCode), ResourceCounts(Output.ResourceCounts), Frequency(Output.Frequency)
{
    if (!Output.bSucceeded || ShaderCode.empty())
        throw std::invalid_argument("Shader resource requires successful nonempty compiler output");
    if (Frequency != SF_Vertex && Frequency != SF_Pixel)
        throw std::invalid_argument("Unsupported shader stage");
}

FShaderMapResource::~FShaderMapResource()
{
    // 当前 App 先 WaitForGPU 再销毁 Map/引用。编译失败时，新资源从未用于 Draw，也可直接释放。
    // 必须在此派生析构器执行，不能指望 FRenderResource 基类析构再调用派生虚函数。
    ReleaseResource();
}

const TRefCountPtr<FRHIShader>& FShaderMapResource::GetShader() const
{
    if (!IsInitialized() || !ShaderRHI)
        throw std::logic_error("Shader resource is not initialized");
    return ShaderRHI;
}

void FShaderMapResource::InitRHI(FRHICommandListBase& RHICmdList)
{
    // 接口签名对齐 UE；当前 Shader 创建直接走 DynamicRHI，不需要录制命令。
    (void)RHICmdList;
    FRHICreateShaderDesc Desc{std::span<const uint8>(ShaderCode)};
    Desc.ResourceCounts = ResourceCounts;
    if (Frequency == SF_Vertex) ShaderRHI = RHICreateVertexShader(Desc);
    else ShaderRHI = RHICreatePixelShader(Desc);
    if (!ShaderRHI) throw std::runtime_error("RHI shader creation failed");
}

void FShaderMapResource::ReleaseRHI() noexcept
{
    // 仅减少这一个所有者的引用；PSO/外部引用仍可能持有旧 Shader。
    ShaderRHI.reset();
}
