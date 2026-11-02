#include "Shader.h"
#include "DynamicRHI.h"

FShader::FShader(const CompiledShaderInitializerType& Initializer) : Frequency(Initializer.Frequency)
{
    if (!Initializer.bSucceeded || Initializer.ShaderCode.empty())
        throw std::invalid_argument("Shader requires successful nonempty compiler output");
}

FShaderMapResource::FShaderMapResource(const FShaderCompilerOutput& Output)
{
    // RHI 必须已初始化；字节码由后端在创建过程中复制，不借用 Output 的存储。
    const FRHICreateShaderDesc Desc = Output.MakeRHICreateDesc();
    if (Output.Frequency == SF_Vertex) ShaderRHI = RHICreateVertexShader(Desc);
    else if (Output.Frequency == SF_Pixel) ShaderRHI = RHICreatePixelShader(Desc);
    else throw std::invalid_argument("Unsupported shader stage");
    if (!ShaderRHI) throw std::runtime_error("RHI shader creation failed");
}
