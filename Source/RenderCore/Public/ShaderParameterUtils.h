#pragma once
#include "ShaderParameters.h"
#include "RHICommandList.h"

// 当前教学桥接：尚无 FRHIUniformBuffer，因此把完整 CPU 字节块复制到每帧常量 arena。
// 不把它命名为 UE 的 SetUniformBufferParameter，避免混淆“上传字节”与“绑定 UniformBuffer 对象”。
inline void SetShaderConstantBufferData(FRHICommandList& CmdList, EShaderFrequency Frequency,
    const FShaderUniformBufferParameter& Parameter, const void* Data, uint32 Size)
{
    if (!Parameter.IsBound()) return; // 可选参数被优化掉时无需提交。
    if (Frequency != SF_Vertex) throw std::invalid_argument("Constant upload currently supports VS only");
    if (!Data || Size != Parameter.GetNumBytes())
        throw std::invalid_argument("CPU constant block size must match reflected cbuffer size");
    CmdList.SetShaderConstants(Parameter.GetBaseIndex(), Data, Size);
}

inline void SetSRVParameter(FRHICommandList& CmdList, EShaderFrequency Frequency,
    const FShaderResourceParameter& Parameter, FRHIShaderResourceView* View)
{
    if (!Parameter.IsBound()) return;
    if (Frequency != SF_Pixel || Parameter.GetType() != EShaderParameterType::SRV || Parameter.GetNumResources() != 1)
        throw std::invalid_argument("SRV binding currently requires a single pixel shader SRV");
    if (!View) throw std::invalid_argument("Null SRV");
    CmdList.SetShaderResourceViewParameter(Parameter.GetBaseIndex(), View);
}
