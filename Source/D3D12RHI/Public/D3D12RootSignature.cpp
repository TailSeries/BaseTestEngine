#include "D3D12RootSignature.h"
#include "D3D12Device.h"
#include <cassert>

FD3D12RootSignature::FD3D12RootSignature(FD3D12Device* InDevice, const D3D12_ROOT_SIGNATURE_DESC& Desc)
{
	// 1. 序列化：desc → blob
	ComPtr<ID3DBlob> Seralized;
	ComPtr<ID3DBlob> Error;
	HRESULT hr = D3D12SerializeRootSignature(&Desc, D3D_ROOT_SIGNATURE_VERSION_1, &Seralized, &Error);
    if (FAILED(hr))
    {
        if (Error)
            OutputDebugStringA((const char*)Error->GetBufferPointer());
        assert(false && "Serialize root signature failed");
        return;
    }

    // 2. 从 blob 创建
    ID3D12Device* D3DDevice = InDevice->GetDevice();
    VERIFY_D3D12(D3DDevice->CreateRootSignature(0, Seralized->GetBufferPointer(), Seralized->GetBufferSize(), IID_PPV_ARGS(&RootSignature)));
}

FD3D12RootSignature::~FD3D12RootSignature() = default;

/*
 * 
说明

- 空签名 ≠ 什么都不用:三角形的顶点(位置/颜色)从 VB 经 IA 进 shader,是"输入布局"路径,不是根签名参数。等要传 MVP 矩阵(常量缓冲)时,才往根签名加 CBV 参数——那是下一步让三角形动起来要做的。
- ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT:忘了它是新手最常见的坑,PSO 会以 E_INVALIDARG 挂掉且不说原因。
- 版本 1.0:D3D_ROOT_SIGNATURE_VERSION_1。1.1 加了静态描述符优化(告诉驱动描述符创建后不变),学习先用 1.0。
- UE 对照:UE 的 FD3D12RootSignature 带"量化绑定状态"(QBSS)缓存、自动从 shader 反射生成签名,很重。我们手动填 desc,直白。
---
 */
