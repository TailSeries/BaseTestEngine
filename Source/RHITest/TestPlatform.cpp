#include "TestPlatform.h"
#include "D3D12DynamicRHI.h"
#include "D3D12RHIPrivate.h"
#include <cstring>
#include <d3d12sdklayers.h>
using Microsoft::WRL::ComPtr;

std::unique_ptr<FDynamicRHI> CreateTestRHI(bool EnableValidation)
{
    if (EnableValidation)
    {
        ComPtr<ID3D12Debug> Debug;
        VERIFY_D3D12(D3D12GetDebugInterface(IID_PPV_ARGS(&Debug)));
        Debug->EnableDebugLayer();
        ComPtr<ID3D12Debug1> Debug1;
        if (SUCCEEDED(Debug.As(&Debug1))) Debug1->SetEnableGPUBasedValidation(TRUE);
    }
    return std::make_unique<FD3D12DynamicRHI>();
}

