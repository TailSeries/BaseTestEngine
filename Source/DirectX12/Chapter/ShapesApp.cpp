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
	BuildPSOs();
	BuildRenderItems();
	BuildFrameResources();
	BuildDescriptorHeaps();
	BuildConstantBufferViews();

	// —Close 不是"结束命令"，而是"封存命令让它可以被执行"
	ThrowIfFailed(MCommandList->Close());
	ID3D12CommandList* cmdLists[] = { MCommandList.Get() };
	MCommandQueue->ExecuteCommandLists(1, cmdLists);
	FlushCommandQueue();
	return true;
}

void ShapesApp::OnResize()
{
	D3DApp::OnResize();

	// 尺寸变化的时候调整相机
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);

}

void ShapesApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	UpdateCamera(gt);

	// cpu 先准备处理下一帧了
	mCurrentFrameResourceIndex = (mCurrentFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrentFrameResourceIndex].get();

	// ① 判断：CPU侧要更新到的 FrameResource 上有没有在用的命令，且 GPU 还没执行完
	if (mCurrFrameResource->Fence != 0 && MD3dFence->GetCompletedValue() < mCurrFrameResource->Fence)  // 上轮有录命令（Fence=0 说明还没用过） GPU 还没跑到上轮设的 fence
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(MD3dFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
	UpdateObjectCBs(gt);
	UpdateMainPassCB(gt);
}

void ShapesApp::UpdateCamera(const GameTimer& gt)
{
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);

}


void ShapesApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			e->NumFramesDirty--;
		}
	}
}

void ShapesApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMVECTOR viewdet = XMMatrixDeterminant(view);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&viewdet, view);
	XMVECTOR projdet = XMMatrixDeterminant(proj);
	XMMATRIX invProj = XMMatrixInverse(&projdet, proj);
	XMVECTOR projviewdet = XMMatrixDeterminant(viewProj);
	XMMATRIX invViewProj = XMMatrixInverse(&projviewdet, viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)MClientWidth, (float)MClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / MClientWidth, 1.0f / MClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}


void ShapesApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	ThrowIfFailed(cmdListAlloc->Reset());

	if (mIsWireFrame)
	{
		ThrowIfFailed(MCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
	}
	else
	{
		ThrowIfFailed(MCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
	}
	MCommandList->RSSetViewports(1, &MScreenViewPort);
	MCommandList->RSSetScissorRects(1, &MScissorRect);

	CD3DX12_RESOURCE_BARRIER PresentToRT = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	MCommandList->ResourceBarrier(1, &PresentToRT);

	MCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	MCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);


	// 注意 GPU 写入的是资源，但这里传的句柄是驱动用来"定位描述符"的 OMSetRenderTargets 需要的是"描述符的地址"，不是资源本身。而"定位描述符、把它记录进命令列表"是 CPU 在录制命令时做的事——所以传 CPU 句柄。
	//CPU 句柄 │ RTV/DSV 描述符在 CPU 录制时被记录进命令列表，GPU 从命令列表里直接读描述符，不需要运行时去堆里查
	D3D12_CPU_DESCRIPTOR_HANDLE backbufferviewhandle = CurrentBackBufferView();
	D3D12_CPU_DESCRIPTOR_HANDLE depthstencilviewhandle = DepthStencilView();
	MCommandList->OMSetRenderTargets(1, &backbufferviewhandle, true, &depthstencilviewhandle);


	ID3D12DescriptorHeap* descriptorHeaps[] = {mCbvHeap.Get()};
	MCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	MCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	/*
	 * 为什么这里又是GPU handle？
	 * GPU 句柄 │ 描述符表不把描述符拷进命令列表，而是告诉 GPU"运行时要到堆里这个 GPU 地址去取"——所以 GPU 需要 GPU 句柄去堆里查
	 */
	int passCbvIndex = mPassCbvOffset + mCurrentFrameResourceIndex;
	CD3DX12_GPU_DESCRIPTOR_HANDLE passCbvHandle{ mCbvHeap->GetGPUDescriptorHandleForHeapStart() };
	passCbvHandle.Offset(passCbvIndex, MCBVSRVUAVDescriptorSize);
	MCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);


	DrawRenderItems(MCommandList.Get(), mOpaqueRitems);

	CD3DX12_RESOURCE_BARRIER RTTpPresent = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	MCommandList->ResourceBarrier(1, &RTTpPresent);

	ThrowIfFailed(MCommandList->Close());

	ID3D12CommandList* cmdsLists[]= {MCommandList.Get()};
	MCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(MSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;
	mCurrFrameResource->Fence = ++CurrentFence;

	MCommandQueue->Signal(MD3dFence.Get(), CurrentFence);
}

void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& rItems)
{
	UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	ID3D12Resource* objectCB = mCurrFrameResource->ObjectCB->Resource();

	for (size_t i = 0; i < rItems.size(); i++)
	{
		RenderItem* ri = rItems[i];
		{
			/*
			 * 所有 RenderItem 的 Geo 是同一个对象，所以这里每帧重复 Set 顶点缓冲其实是"冗余"的
			 * 那为什么龙书还这么写？教学通用性：假设"每个 RenderItem 可以有不同 Geo"
			 */
			D3D12_VERTEX_BUFFER_VIEW vertexbufferview = ri->Geo->VertexBufferView();
			cmdList->IASetVertexBuffers(0, 1, &vertexbufferview);
			D3D12_INDEX_BUFFER_VIEW indexbufferview = ri->Geo->IndexBufferView();
			cmdList->IASetIndexBuffer(&indexbufferview);
			cmdList->IASetPrimitiveTopology(ri->PrimitiveType);
		}


		UINT cbvIndex = mCurrentFrameResourceIndex * (UINT)mOpaqueRitems.size() + ri->ObjCBIndex;
		CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, MCBVSRVUAVDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);
		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}


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
	mShaders["standardVS"] = D3DUtil::CompileShader(shaderPath, nullptr, "VS", "vs_5_1");
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
	//创建基础图形
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);


	//假设我们所有的mesh都放在一个顶点buffer中，那么我们这里记录每个mesh的顶点偏移
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// 假设我们所有的mesh都放在一个indice buffer中，那么我们这里记录每个mesh的indice偏移
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();


	// 构建submesh，submesh的顶点和索引就用上面的偏移
	SubMeshGeometry boxSubMesh;
	boxSubMesh.IndexCount = (UINT)box.Indices32.size();
	boxSubMesh.BaseVertexLocation = boxVertexOffset;
	boxSubMesh.StartIndexLocation = boxIndexOffset;


	SubMeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubMeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubMeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;


	// 展开定点，打包进一个vertexbuffer里面
	auto totalVertexCount = box.Vertices.size() + grid.Vertices.size() + sphere.Vertices.size() + cylinder.Vertices.size();
	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::DarkGreen);
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::ForestGreen);
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Crimson);
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::SteelBlue);
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), box.GetIndices16().begin(), box.GetIndices16().end());
	indices.insert(indices.end(), grid.GetIndices16().begin(), grid.GetIndices16().end());
	indices.insert(indices.end(), sphere.GetIndices16().begin(), sphere.GetIndices16().end());
	indices.insert(indices.end(), cylinder.GetIndices16().begin(), cylinder.GetIndices16().end());

	const UINT vbByteSize = vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = indices.size() * sizeof(Vertex);


	// pack 之后的mesh内存
	auto geo = std::make_unique<MeshGeometry>();
	geo->name = "shapeGeo";
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));// 就是malloc罢了, 不过它有两个接口  GetBufferPointer GetBufferSize 让你一次性拿地址和大小
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	// defaultbuffer的引用
	geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(MD3dDevice.Get(), MCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(MD3dDevice.Get(), MCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubMesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	mGeometries[geo->name] = std::move(geo);
}

void ShapesApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
	//PSO 上挂的根签名不是"默认值"，它根本不被用于绘制——GPU 绘制时只认命令列表当前 Set 的那个根签名

	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	opaquePsoDesc.InputLayout = D3D12_INPUT_LAYOUT_DESC{ mInputLayout.data(), (UINT)(mInputLayout.size()) };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();

	opaquePsoDesc.VS = D3D12_SHADER_BYTECODE{ mShaders["standardVS"]->GetBufferPointer(), mShaders["standardVS"]->GetBufferSize() };
	opaquePsoDesc.PS = D3D12_SHADER_BYTECODE{ mShaders["opaquePS"]->GetBufferPointer(), mShaders["opaquePS"]->GetBufferSize() };

	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC{ D3D12_DEFAULT };

	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC{ D3D12_DEFAULT };
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC{ D3D12_DEFAULT };
	opaquePsoDesc.SampleMask = UINT_MAX;

	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = MBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = M4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = M4xMsaaState ? (M4xMSAAQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = MDepthStencilFormat;
	ThrowIfFailed(MD3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));


	// wireframe 形式的pso
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(MD3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));

}

void ShapesApp::BuildRenderItems()
{
	auto boxRitem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(2, 2, 2) * XMMatrixTranslation(0, 0.5, 0));
	boxRitem->ObjCBIndex = 0;
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = MathHelper::Identity4x4();
	gridRitem->ObjCBIndex = 1;
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	mAllRitems.push_back(std::move(gridRitem));
	UINT objCBIndex = 2;
	for (int i = 0; i < 5; ++i)
	{
		auto leftCylRitem = std::make_unique<RenderItem>();
		auto rightCylRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

		XMStoreFloat4x4(&leftCylRitem->World, rightCylWorld);
		leftCylRitem->ObjCBIndex = objCBIndex++;
		leftCylRitem->Geo = mGeometries["shapeGeo"].get();
		leftCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4(&rightCylRitem->World, leftCylWorld);
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Geo = mGeometries["shapeGeo"].get();
		rightCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Geo = mGeometries["shapeGeo"].get();
		leftSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Geo = mGeometries["shapeGeo"].get();
		rightSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		mAllRitems.push_back(std::move(leftCylRitem));
		mAllRitems.push_back(std::move(rightCylRitem));
		mAllRitems.push_back(std::move(leftSphereRitem));
		mAllRitems.push_back(std::move(rightSphereRitem));
	}


	for (auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());

}

void ShapesApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; i++)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(MD3dDevice.Get(), 1, mAllRitems.size()));
	}

}


void ShapesApp::BuildDescriptorHeaps()
{
	UINT objCount = (UINT)mOpaqueRitems.size();

	//每个物体一个常量描述符 + 每个pass一个常量描述符
	UINT numDescriptors = (objCount + 1) * gNumFrameResources;



	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = numDescriptors;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed(MD3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap)));

}

void ShapesApp::BuildConstantBufferViews()
{
	UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT objCount = (UINT)mOpaqueRitems.size();
	mPassCbvOffset = objCount * gNumFrameResources;
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		ID3D12Resource* objectCB = mFrameResources[frameIndex]->ObjectCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS curframecbAddress = objectCB->GetGPUVirtualAddress();
		// 每个物体自己的CBV
		for (UINT i = 0; i < objCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS curobjcbAddress = curframecbAddress + i * objCBByteSize;
			int heapIndex = frameIndex * objCount + i;
			//创建时（CPU 写）→ 必须 CPU 句柄；绑定时（GPU 读）→ 必须 GPU 句柄。
			CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
				mCbvHeap->GetCPUDescriptorHandleForHeapStart());

			handle.Offset(heapIndex, MCBVSRVUAVDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = curobjcbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			MD3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	// 每个pass的CBV
	UINT passCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto passCB = mFrameResources[frameIndex]->PassCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		int heapIndex = mPassCbvOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, MCBVSRVUAVDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;

		MD3dDevice->CreateConstantBufferView(&cbvDesc, handle);
	}

}



// 鼠标操作
void ShapesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(MhMainWnd);
}


void ShapesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void ShapesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta += dx;
		mPhi += dy;

		// Restrict the angle mPhi.
		mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to 0.2 unit in the scene.
		float dx = 0.05f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.05f * static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void ShapesApp::OnKeyboardInput(const GameTimer& gt)
{
	if (GetAsyncKeyState('1') & 0x8000)
		mIsWireFrame = true;
	else
		mIsWireFrame = false;
}