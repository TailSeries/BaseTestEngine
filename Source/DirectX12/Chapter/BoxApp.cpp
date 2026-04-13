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


void BoxApp::FillInputLayout()
{

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
    InputLayout.push_back({"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
    InputLayout.push_back({"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
}

#endif