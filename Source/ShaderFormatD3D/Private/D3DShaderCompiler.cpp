#include "D3DShaderCompiler.h"
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <wrl/client.h>
#include <algorithm>
#include <limits>

using Microsoft::WRL::ComPtr;

FShaderCompilerOutput CompileD3DShader(const FShaderCompilerInput& Input)
{
    FShaderCompilerOutput Output;
    Output.Frequency = Input.Frequency;
    try
    {
        const char* Target = nullptr;
        if (Input.Frequency == SF_Vertex) Target = "vs_5_0";
        else if (Input.Frequency == SF_Pixel) Target = "ps_5_0";
        else throw std::invalid_argument("Only vertex/pixel compilation is implemented");
        if (Input.Source.empty() || Input.EntryPoint.empty())
            throw std::invalid_argument("Shader source and entry point must not be empty");

        ComPtr<ID3DBlob> Code, Errors;
        const HRESULT Result = D3DCompile(Input.Source.data(), Input.Source.size(),
            nullptr, nullptr, nullptr, Input.EntryPoint.c_str(), Target,
            D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &Code, &Errors);
        if (Errors) Output.Diagnostics.assign(static_cast<const char*>(Errors->GetBufferPointer()));
        if (FAILED(Result))
        {
            if (Output.Diagnostics.empty()) Output.Diagnostics = "D3DCompile failed";
            return Output;
        }

        ComPtr<ID3D11ShaderReflection> Reflection;
        if (FAILED(D3DReflect(Code->GetBufferPointer(), Code->GetBufferSize(), IID_PPV_ARGS(&Reflection))))
            throw std::runtime_error("D3DReflect failed");
        D3D11_SHADER_DESC ShaderDesc{};
        if (FAILED(Reflection->GetDesc(&ShaderDesc))) throw std::runtime_error("Shader reflection GetDesc failed");

        for (UINT Index = 0; Index < ShaderDesc.BoundResources; ++Index)
        {
            D3D11_SHADER_INPUT_BIND_DESC Binding{};
            if (FAILED(Reflection->GetResourceBindingDesc(Index, &Binding)))
                throw std::runtime_error("Resource reflection failed");
            // 数量编码为寄存器范围的上界：仅使用 b1 也需要覆盖 [0,2)，不能误报成 b0。
            // 当前后端将拒绝超过自身能力的布局；反射不负责偷偷重排寄存器。
            const uint64 End = static_cast<uint64>(Binding.BindPoint) + Binding.BindCount;
            if (!Binding.BindCount || End > std::numeric_limits<uint8>::max())
                throw std::invalid_argument("Shader register range exceeds packed metadata capacity");
            uint8* Count = nullptr;
            EShaderParameterType Type;
            uint16 BufferIndex = 0;
            uint16 BaseIndex = static_cast<uint16>(Binding.BindPoint);
            uint16 Size = static_cast<uint16>(Binding.BindCount);
            switch (Binding.Type)
            {
            case D3D_SIT_CBUFFER:
            {
                Type = EShaderParameterType::UniformBuffer;
                Count = &Output.ResourceCounts.NumCBs;
                D3D11_SHADER_BUFFER_DESC BufferDesc{};
                if (FAILED(Reflection->GetConstantBufferByName(Binding.Name)->GetDesc(&BufferDesc)))
                    throw std::runtime_error("Constant buffer reflection failed");
                if (BufferDesc.Size > std::numeric_limits<uint16>::max())
                    throw std::invalid_argument("Constant buffer too large for parameter allocation");
                BufferIndex = BaseIndex;
                BaseIndex = 0;
                Size = static_cast<uint16>(BufferDesc.Size);
                break;
            }
            case D3D_SIT_TEXTURE:
                Type = EShaderParameterType::SRV; Count = &Output.ResourceCounts.NumSRVs; break;
            case D3D_SIT_SAMPLER:
                Type = EShaderParameterType::Sampler; Count = &Output.ResourceCounts.NumSamplers; break;
            default:
                // Buffer SRV、UAV 等尚未接通后端，明确失败，避免生成看似可用的错误布局。
                throw std::invalid_argument("Unsupported reflected resource: " + std::string(Binding.Name));
            }
            *Count = std::max(*Count, static_cast<uint8>(End));
            // FXC 可能把数组展开成 Images[0]、Images[1]；保留反射结果，不猜测源码声明。
            Output.ParameterMap.AddParameterAllocation(Binding.Name, BufferIndex, BaseIndex, Size, Type);
        }
        const auto* Bytes = static_cast<const uint8*>(Code->GetBufferPointer());
        Output.ShaderCode.assign(Bytes, Bytes + Code->GetBufferSize());
        Output.bSucceeded = true;
    }
    catch (const std::exception& Error)
    {
        Output.Diagnostics += Error.what();
    }
    return Output;
}
