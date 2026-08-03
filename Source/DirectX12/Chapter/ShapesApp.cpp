#include "ShapesApp.h"
#include "Common/GeometryGenerator.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace DirectX12;

ShapesApp::ShapesApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{

}
ShapesApp::~ShapesApp()
{
    if (MD3dDevice != nullptr)
        FlushCommandQueue();
}


bool ShapesApp::Initialize()
{
	if (!D3DApp::Initialize())
	{
		return false;
	}

	ThrowIfFailed(MCommandList->Reset(MCommandAllocator.Get(), nullptr));
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	//...


}

void ShapesApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);  // 类型 CBV，数量 1，register b0

	CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1); // 类型 CBV，数量 1，register b1

	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	/*参数 0 = per-object CBV（b0），参数 1 = per-pass CBV（b1）。"Descriptor table" 类型表示"这个参数指向一张描述符堆里的表"，表的内容可以在不同 draw 间快速切换（后面 Draw 里就是这么干的）*/
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);// 指向 b0 那张表
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1); // 指向 b1 那张表

	/*
	 * D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT 是个关键标志：它允许管线使用 IA（Input Assembler）的 input layout，即顶点数据的输入布局由 BuildShadersAndInputLayout 那边定义。不设这个标志，顶点输入布局会被禁用——这是最常见的踩坑点之一。
	 */
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	/*
	 * 根签名不能直接把结构体给设备，必须先序列化成字节流（D3D12SerializeRootSignature），再把字节流交给 CreateRootSignature。
	 * 版本用 _VERSION_1。错误时 errorBlob 里会有一段可读的错误文本（中文环境尤其注意：它是英文的），代码把它打到 VS 的 Output 窗口。
	 */
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(MD3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())
	))
}

void ShapesApp::BuildShadersAndInputLayout()
{
	std::string shaderPath = "F:/workspace/BaseTestEngine/Source/Shaders/Chapter7/color.hlsl";
	mShaders["standardVs"] = D3DUtil::CompileShader(shaderPath, nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = D3DUtil::CompileShader(shaderPath, nullptr, "PS", "ps_5_1");


	//已经有了SemanticIndex为什么还需要指定InputSlot呢？
	/*
	 * SemanticIndex 指的是第几个可变输入寄存器
	 *  InputSlot 指向 IASetVertexBuffers 绑定的第 N 个顶点缓冲。
	 */
	mInputLayout = 
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},

	};
}

void ShapesApp::BuildShapeGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);



}


