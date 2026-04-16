#include "BoxApp.h"
#ifdef  CHAPTER_6




BoxApp::BoxApp(HINSTANCE hInstance)
	:D3DApp(hInstance)
{

}

BoxApp::~BoxApp()
{
}

bool BoxApp::Initialize()
{
    if (!D3DApp::Initialize()) return false;

    // 先清空命令列表
    ThrowIfFailed(MCommandList->Reset(MCommandAllocator.Get(), nullptr));

    // 创建CBV的描述符堆
    BuildDescriptorHeaps();
    // 创建cb资源以及cbv
    BuildConstantBuffers();
    BuildRootSignature();

    // 编译shader为二进制，构建输入布局
    BuildShadersAndInputLayout();
    BuildBoxGeometry();
    BuildPSO();

  
    // indexbuffer， vertexbuffer写入上传堆的命令这里就执行了
    ThrowIfFailed(MCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { MCommandList.Get() };
    MCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    FlushCommandQueue();

    return true;
}

void BoxApp::BuildDescriptorHeaps()
{
    /*
     * 在initialize阶段我们已经创建一个RTV描述符堆，一个DSV的描述符堆。我们现在要传矩阵就对应创建一个CBV的描述符堆
     */
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;// 这个堆
    cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(MD3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&MCbvHeap)));

}

void BoxApp::BuildRootSignature()
{
   CD3DX12_ROOT_PARAMETER slotRootParamater[1];// 我们现在只需要传递一个constantbuffer进去，因此就一个根参数

   // 使用描述符表作为根参数
   CD3DX12_DESCRIPTOR_RANGE cbvTable;
   cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);// 类型: CBV, 数量: 1, 寄存器: b0
   slotRootParamater[0].InitAsDescriptorTable(1, &cbvTable); 
   

   /*
   * D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT 允许使用 IA 阶段的 顶点缓冲 + 输入布局（Input Layout:意味着可以在输入装配阶段使用下面的命令
   *   IASetVertexBuffers
   *   IASetIndexBuffer
   *   InputLayout
   */
   // 1 一个根参数，根参数数组，0个静态采样器，允许输入装配器输入布局
   CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParamater, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
   
   /*
   * 序列化：将根签名转换成GPU可理解的二进制格式
   * 
   * 
   */
   Microsoft::WRL::ComPtr<ID3DBlob> serializeRootSig = nullptr;
   Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
   HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializeRootSig.GetAddressOf(), errorBlob.GetAddressOf());
   if (errorBlob != nullptr)
   {
       ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
   }

   ThrowIfFailed(MD3dDevice->CreateRootSignature(0, serializeRootSig->GetBufferPointer(), serializeRootSig->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
    
}

void BoxApp::BuildConstantBuffers()
{
    MObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(MD3dDevice.Get(), 1, true);
    UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    D3D12_GPU_VIRTUAL_ADDRESS cbAddress = MObjectCB->Resource()->GetGPUVirtualAddress();

    int boxCBBuffIndex = 0;
    cbAddress += boxCBBuffIndex * objCBByteSize;
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    cbvDesc.BufferLocation = cbAddress;
    cbvDesc.SizeInBytes = objCBByteSize;

    MD3dDevice->CreateConstantBufferView(&cbvDesc, MCbvHeap->GetCPUDescriptorHandleForHeapStart());

}


void BoxApp::BuildShadersAndInputLayout()
{
    HRESULT hr = S_OK;
    std::string shaderPath = "F:/workspace/BaseTestEngine/Source/Shaders/Chapter6/color.hlsl";
    MvsByteCode = D3DUtil::CompileShader(shaderPath, nullptr, "VS", "vs_5_0");
    MpsByteCode = D3DUtil::CompileShader(shaderPath, nullptr, "PS", "ps_5_0");

    //我们的顶点只有一个 struct Vertex 	D3D12_INPUT_ELEMENT_DESC
     /*struct D3D12_INPUT_ELEMENT_DESC
     {
     LPCSTR SemanticName; // 语义名称
     UINT SemanticIndex; // 同语义的不同索引下标
     DXGI_FORMAT Format; // 该成员的数据格式
     UINT InputSlot; //输入槽 0 ~ 15
     UINT AlignedByteOffset; //在buffer结构中的偏移位置
     D3D12_INPUT_CLASSIFICATION InputSlotClass; // PER_VERTEX_DATA PER_INSTANCE_DATA是instance技术用的
     UINT InstanceDataStepRate;// 目前为0, 要用instance这里写1
     }
 *
 */
    InputLayout.push_back({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
    InputLayout.push_back({ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });

}

void BoxApp::BuildBoxGeometry()
{
    std::array<Vertex, 8> vertices =
    {
        Vertex({ DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::White) }),
        Vertex({ DirectX::XMFLOAT3(-1.0f, +1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Black) }),
        Vertex({ DirectX::XMFLOAT3(+1.0f, +1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Red) }),
        Vertex({ DirectX::XMFLOAT3(+1.0f, -1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Green) }),
        Vertex({ DirectX::XMFLOAT3(-1.0f, -1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Blue) }),
        Vertex({ DirectX::XMFLOAT3(-1.0f, +1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Yellow) }),
        Vertex({ DirectX::XMFLOAT3(+1.0f, +1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Cyan) }),
        Vertex({ DirectX::XMFLOAT3(+1.0f, -1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Magenta) })
    };

    std::array<std::uint16_t, 36> indices =
    {
        // front face
        0, 1, 2,
        0, 2, 3,

        // back face
        4, 6, 5,
        4, 7, 6,

        // left face
        4, 5, 1,
        4, 1, 0,

        // right face
        3, 2, 6,
        3, 6, 7,

        // top face
        1, 5, 6,
        1, 6, 2,

        // bottom face
        4, 0, 3,
        4, 3, 7
    };


    const UINT vByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT iByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    mBoxGeo = std::make_unique<MeshGeometry>();
    mBoxGeo->name = "boxgeo";
    ThrowIfFailed(D3DCreateBlob(vByteSize, &mBoxGeo->VertexBufferCPU));
    CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vByteSize);

    ThrowIfFailed(D3DCreateBlob(iByteSize, &mBoxGeo->IndexBufferCPU));
    CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), iByteSize);

    mBoxGeo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(MD3dDevice.Get(), MCommandList.Get(), vertices.data(), vByteSize, mBoxGeo->VertexBufferUploader);

    mBoxGeo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(MD3dDevice.Get(), MCommandList.Get(), indices.data(), iByteSize, mBoxGeo->IndexBufferUploader);

    mBoxGeo->VertexByteStride = sizeof(Vertex);
    mBoxGeo->VertexBufferByteSize = vByteSize;
    mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    mBoxGeo->IndexBufferByteSize = iByteSize;

    SubMeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;//// 从索引缓冲区的开头开始
    submesh.BaseVertexLocation = 0;  // 顶点缓冲区偏移为 0
    mBoxGeo->DrawArgs["box"] = submesh;
}

void BoxApp::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { InputLayout.data(), (UINT)InputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = {reinterpret_cast<BYTE*>(MvsByteCode->GetBufferPointer())};
    psoDesc.PS = {reinterpret_cast<BYTE*>(MpsByteCode->GetBufferPointer())};

    /*光栅化状态
    *   - 使用默认值：
    - FillMode = D3D12_FILL_MODE_SOLID（实心填充）
    - CullMode = D3D12_CULL_MODE_BACK（背面剔除）
    - FrontCounterClockwise = FALSE（顺时针为正面）
    - DepthBias = 0 等

    */
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

    /*
    *  默认不混合：RenderTarget[0].BlendEnable = FALSE
    */
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    /*默认启用深度测试：
    - DepthEnable = TRUE
    - DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL
    - DepthFunc = D3D12_COMPARISON_FUNC_LESS（深度更小通过）*/
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DSVFormat = MDepthStencilFormat;
    /*：控制哪些采样点被启用。
  - UINT_MAX 表示所有采样点都启用（32位全1）。*/
    psoDesc.SampleMask = UINT_MAX;

    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    /*  - 数量：1个渲染目标（交换链的后缓冲区）。
  - 格式：mBackBufferFormat（如 DXGI_FORMAT_R8G8B8A8_UNORM）。*/
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = MBackBufferFormat;

    psoDesc.SampleDesc.Count = M4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = M4xMsaaState ? (M4xMSAAQuality - 1) : 0;

    ThrowIfFailed(MD3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}

void BoxApp::FillInputLayout()
{


}

void BoxApp::OnResize()
{
    D3DApp::OnResize();
    DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

void BoxApp::Update(const GameTimer& gt)
{
    // Convert Spherical to Cartesian coordinates.
    float x = mRadius * sinf(mPhi) * cosf(mTheta);
    float z = mRadius * sinf(mPhi) * sinf(mTheta);
    float y = mRadius * cosf(mPhi);

    // Build the view matrix.
    DirectX::XMVECTOR pos = DirectX::XMVectorSet(x, y, z, 1.0f);
    DirectX::XMVECTOR target = DirectX::XMVectorZero();
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);

    DirectX::XMMATRIX world = XMLoadFloat4x4(&mWorld);
    DirectX::XMMATRIX proj = XMLoadFloat4x4(&mProj);
    DirectX::XMMATRIX worldViewProj = world * view * proj;

    // Update the constant buffer with the latest worldViewProj matrix.
    ObjectConstants objConstants;
    XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
    MObjectCB->CopyData(0, objConstants);
}

void BoxApp::Draw(const GameTimer& gt)
{
    ThrowIfFailed(MCommandAllocator->Reset());
    ThrowIfFailed(MCommandList->Reset(MCommandAllocator.Get(), mPSO.Get()));

    MCommandList->RSSetViewports(1, &MScreenViewPort);
    MCommandList->RSSetScissorRects(1, &MScissorRect);

    CD3DX12_RESOURCE_BARRIER BackBufferTransition = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    MCommandList->ResourceBarrier(1, &BackBufferTransition);

    //0 nullptr 搭配意思是清除整个RT
    MCommandList->ClearRenderTargetView(CurrentBackBufferView(), DirectX::Colors::LightSteelBlue, 0, nullptr);
    MCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    D3D12_CPU_DESCRIPTOR_HANDLE depthstencilvview = DepthStencilView();
    D3D12_CPU_DESCRIPTOR_HANDLE currentbackbufferview = CurrentBackBufferView();
    MCommandList->OMSetRenderTargets(1, &currentbackbufferview, true, &depthstencilvview);


    ID3D12DescriptorHeap* descriptorHeaps[] = { MCbvHeap.Get() };
    MCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    MCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    //在DirectX12的Input Assembler阶段，确实可以有多个顶点缓冲区视图（Vertex Buffer Views），但只能有一个索引缓冲区视图（Index Buffer View）; 索引是对顶点的整体引用，GPU会从每个顶点缓冲区中读取相同位置（相同索引）的数据，然后组合成完整的顶点。
    D3D12_VERTEX_BUFFER_VIEW vertesbufferview = mBoxGeo->VertexBufferView();
    MCommandList->IASetVertexBuffers(0, 1, &vertesbufferview);
    D3D12_INDEX_BUFFER_VIEW indexbufferview = mBoxGeo->IndexBufferView();
    MCommandList->IASetIndexBuffer(&indexbufferview);
    MCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    MCommandList->SetGraphicsRootDescriptorTable(0, MCbvHeap->GetGPUDescriptorHandleForHeapStart());

    MCommandList->DrawIndexedInstanced(mBoxGeo->DrawArgs["box"].IndexCount, 1, 0, 0, 0);

    CD3DX12_RESOURCE_BARRIER BackBufferTransitionPresent = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    MCommandList->ResourceBarrier(1, &BackBufferTransitionPresent);

    ThrowIfFailed(MCommandList->Close());
    ID3D12CommandList* cmdList[] = { MCommandList.Get()};
    MCommandQueue->ExecuteCommandLists(_countof(cmdList), cmdList);

    ThrowIfFailed(MSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;
    FlushCommandQueue();
}

void BoxApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(MhMainWnd);
}

void BoxApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = DirectX::XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = DirectX::XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

        // Restrict the angle mPhi.
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if ((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.005 unit in the scene.
        float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
        float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void BoxApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

#endif