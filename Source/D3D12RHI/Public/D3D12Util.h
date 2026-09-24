#pragma once
#include "D3D12RHIModule.h"
#include "GenericPlatform.h"

#include <array>
#include <compare>

class FD3D12Adapter;
// 对应 UE 的 Adapter 子对象基类。
// 不拥有 Adapter，只保存回指。
class FD3D12AdapterChild
{
public:
    explicit FD3D12AdapterChild(FD3D12Adapter* InParent)
        : ParentAdapter(InParent)
    {}
    FD3D12Adapter* GetParentAdapter() const
    {
        return ParentAdapter;
    }

private:
    FD3D12Adapter* ParentAdapter = nullptr;
};

// 当前只实现 VS / PS。
enum EShaderVisibility
{
    SV_Vertex,
    SV_Pixel,
    SV_ShaderVisibilityCount
};

struct FShaderRegisterCounts
{
    uint8 SamplerCount = 0;
    uint8 ConstantBufferCount = 0;
    uint8 ShaderResourceCount = 0;
    uint8 UnorderedAccessCount = 0;

    auto operator<=>(const FShaderRegisterCounts&) const = default;
};

struct FD3D12QuantizedBoundShaderState
{
    std::array<FShaderRegisterCounts, SV_ShaderVisibilityCount>
        RegisterCounts{};
    bool bAllowIAInputLayout = false;
    auto operator<=>(const FD3D12QuantizedBoundShaderState&) const = default;

};



