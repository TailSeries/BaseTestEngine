#include "D3DShaderCompiler.h"
#include <fstream>

// 编译元数据验证不依赖 GPU；用异常而非 assert，确保 Release 也执行检查。
void ValidateStageCCompiler()
{
    const auto Require = [](bool Condition, const char* Message)
    {
        if (!Condition) throw std::runtime_error(Message);
    };
    const auto VS = CompileD3DShader({
        "cbuffer CB : register(b1) { float4x4 WVP; }; "
        "float4 Main(float4 P : POSITION) : SV_POSITION { return mul(P, WVP); }",
        "Main", SF_Vertex});
    Require(VS.bSucceeded, "C1: b1 compilation failed");
    const auto CB = VS.ParameterMap.FindParameterAllocation("CB");
    Require(CB && CB->Type == EShaderParameterType::UniformBuffer &&
        CB->BufferIndex == 1 && CB->BaseIndex == 0 && CB->Size == 64,
        "C1: cbuffer binding or size incorrect");
    Require(VS.ResourceCounts.NumCBs == 2 && VS.ResourceCounts.NumSRVs == 0,
        "C1: sparse b1 must produce register extent 2");
    Require(!VS.ParameterMap.FindParameterAllocation("Missing"), "C1: missing parameter found");

    const auto PS = CompileD3DShader({
        "Texture2D Images[2] : register(t3); SamplerState S : register(s2); "
        "float4 Main(float2 UV : TEXCOORD) : SV_Target { "
        "return Images[0].Sample(S, UV) + Images[1].Sample(S, UV); }",
        "Main", SF_Pixel});
    Require(PS.bSucceeded, "C1: texture array compilation failed");
    const auto Images = PS.ParameterMap.FindParameterAllocation("Images[0]");
    const auto Image1 = PS.ParameterMap.FindParameterAllocation("Images[1]");
    const auto Sampler = PS.ParameterMap.FindParameterAllocation("S");
    Require(Images && Images->Type == EShaderParameterType::SRV &&
        Images->BaseIndex == 3 && Images->Size == 1 && Image1 &&
        Image1->BaseIndex == 4 && Image1->Size == 1, "C1: texture array allocation incorrect");
    Require(Sampler && Sampler->Type == EShaderParameterType::Sampler && Sampler->BaseIndex == 2,
        "C1: sampler allocation incorrect");
    Require(PS.ResourceCounts.NumSRVs == 5 && PS.ResourceCounts.NumSamplers == 3,
        "C1: resource extents incorrect");
    const auto Desc = PS.MakeRHICreateDesc();
    Require(Desc.Code.size() == PS.ShaderCode.size() && Desc.ResourceCounts.NumSRVs == 5,
        "C1: output to RHI bridge lost metadata");

    const auto Invalid = CompileD3DShader({"invalid hlsl", "Main", SF_Pixel});
    Require(!Invalid.bSucceeded && !Invalid.Diagnostics.empty(), "C1: compiler diagnostics missing");
    bool Rejected = false;
    try { (void)Invalid.MakeRHICreateDesc(); }
    catch (const std::logic_error&) { Rejected = true; }
    Require(Rejected, "C1: failed output accepted by RHI bridge");
    const auto Unsupported = CompileD3DShader({
        "StructuredBuffer<float4> B : register(t0); "
        "float4 Main() : SV_Target { return B[0]; }", "Main", SF_Pixel});
    Require(!Unsupported.bSucceeded && !Unsupported.Diagnostics.empty(),
        "C1: unsupported resource silently accepted");
    std::ofstream Report("stage-c-compiler-checks.txt");
    Report << "PASS: b1 binding/64-byte cbuffer, register extents, t3 array, s2, "
        "parameter lookup, RHI bridge, compilation diagnostics, unsupported resource rejection.\n";
    if (!Report) throw std::runtime_error("Cannot write Stage C compiler report");
}
