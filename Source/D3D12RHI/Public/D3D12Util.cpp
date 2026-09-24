#include  "D3D12Util.h"
#include "D3D12Adapter.h"
#include "D3D12Shader.h"
#include "D3D12RootSignature.h"

#include <stdexcept>


static void QuantizeBoundShaderStateCommon(FD3D12QuantizedBoundShaderState& OutQBSS, const FD3D12ShaderData* ShaderData, EShaderVisibility ShaderVisibility)
{
    if (!ShaderData)
    {
        return;
    }

    const FShaderCodePackedResourceCounts& Counts = ShaderData->ResourceCounts;

    FShaderRegisterCounts& Registers = OutQBSS.RegisterCounts[ShaderVisibility];

	/// 当前支持范围为 0 / 1，暂不做更大数量的分档。
    // UE 在此结合 Resource Binding Tier 等条件进行量化。

    Registers.SamplerCount = Counts.NumSamplers;
    Registers.ConstantBufferCount = Counts.NumCBs;
    Registers.ShaderResourceCount = Counts.NumSRVs;
    Registers.UnorderedAccessCount = Counts.NumUAVs;
}

const FD3D12RootSignature* FD3D12Adapter::GetRootSignature(const FBoundShaderStateInput& BSS)
{
    if (!BSS.GetVertexShader() || !BSS.GetPixelShader())
    {
        throw std::invalid_argument(
            "Current graphics pipeline requires VS and PS");
    }


    FD3D12QuantizedBoundShaderState QBSS{};

    QBSS.bAllowIAInputLayout = BSS.VertexDeclarationRHI != nullptr;

    const auto* VertexShader =
        static_cast<const FD3D12VertexShader*>(BSS.GetVertexShader());

    const auto* PixelShader =
        static_cast<const FD3D12PixelShader*>(BSS.GetPixelShader());

    QuantizeBoundShaderStateCommon(
        QBSS, VertexShader, SV_Vertex);

    QuantizeBoundShaderStateCommon(
        QBSS, PixelShader, SV_Pixel);
    //这里最终刻意没有把 Shader 指针或字节码加入 Key：不同 Shader，只要根签名布局需求相同，就可以共享根签名。
    return RootSignatureManager.GetRootSignature(QBSS);

}
