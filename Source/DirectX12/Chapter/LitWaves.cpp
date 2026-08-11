#include "LitWaves.h"
#include "Common/UploadBuffer.h"
using namespace Microsoft::WRL;
LitWavesApp::LitWavesApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{}

LitWavesApp::~LitWavesApp()
{
    if (MD3dDevice != nullptr)
        FlushCommandQueue();
}

bool LitWavesApp::Initialize()
{
    if (!D3DApp::Initialize())
        return false;

    ThrowIfFailed(MCommandList->Reset(MCommandAllocator.Get(), nullptr));
    MCBVSRVUAVDescriptorSize = MD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildLandGeometry();
    BuildWavesGeometryBuffers();
    BuildMaterials();
    BuildRenderItems();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    ThrowIfFailed(MCommandList->Close());
    ID3D12CommandList* cmdList[] = {MCommandList.Get()};
    MCommandQueue->ExecuteCommandLists(1, cmdList);
    FlushCommandQueue();
    return true;
}


void LitWavesApp::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[3];

    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsConstantBufferView(1);
    slotRootParameter[2].InitAsConstantBufferView(2);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    
    // 根签名应该保存成二进制数据传入
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errBlob.GetAddressOf());
    if (errBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);
    ThrowIfFailed(MD3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void LitWavesApp::BuildShadersAndInputLayout()
{
    mShaders["standardVS"] = D3DUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
    mShaders["opaquePS"] = D3DUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_0");
}




