#pragma once
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"

// 当前测试的固定顶点格式 Shader；尚未引入材质与 VertexFactory 排列。
class FTestVS : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FTestVS);
public:
    explicit FTestVS(const CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
    {
        if (GetFrequency() != SF_Vertex) throw std::invalid_argument("FTestVS needs vertex output");
        Constants.Bind(Initializer.ParameterMap, "CB", SPF_Mandatory);
    }
    const FShaderUniformBufferParameter& GetConstantsParameter() const { return Constants; }
    void SetParameters(FRHICommandList& CmdList, const void* Data, uint32 Size) const
    {
        SetShaderConstantBufferData(CmdList, GetFrequency(), Constants, Data, Size);
    }
private:
    FShaderUniformBufferParameter Constants;
};

class FTestPS : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FTestPS);
public:
    explicit FTestPS(const CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
    {
        if (GetFrequency() != SF_Pixel) throw std::invalid_argument("FTestPS needs pixel output");
        Texture.Bind(Initializer.ParameterMap, "gTex", SPF_Mandatory);
        Sampler.Bind(Initializer.ParameterMap, "gSamp", SPF_Mandatory);
        if (Texture.GetType() != EShaderParameterType::SRV || Sampler.GetType() != EShaderParameterType::Sampler)
            throw std::invalid_argument("Unexpected test shader resource types");
        // s0 当前仍由根签名静态 point/wrap 采样器提供，没有动态采样器绑定调用。
        if (Sampler.GetBaseIndex() != 0 || Sampler.GetNumResources() != 1)
            throw std::invalid_argument("Test shader requires static sampler s0");
    }
    const FShaderResourceParameter& GetTextureParameter() const { return Texture; }
    void SetParameters(FRHICommandList& CmdList, FRHIShaderResourceView* View) const
    {
        SetSRVParameter(CmdList, GetFrequency(), Texture, View);
    }
private:
    FShaderResourceParameter Texture;
    FShaderResourceParameter Sampler;
};

// 启动阶段注册类型并提供源码加载；不创建 GPU 对象。
void RegisterTestShaderTypes();
std::string LoadTestShaderSource(const std::string& VirtualPath);
