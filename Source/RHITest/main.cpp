#include <Windows.h>
#include "D3D12Adapter.h"    // 链了 D3D12RHI，其 Public 目录已在 include 路径
#include "D3D12Viewport.h"
#include "D3D12Resources.h"
#include "D3D12CommandList.h"
#include "D3D12Shader.h"
#include "D3D12RootSignature.h"
#include "D3D12PipelineState.h"
#include <DirectXMath.h>
#include "D3D12DynamicRHI.h"
#include "D3D12DynamicRHI.h"
#include "RHICommandList.h"
#include "D3D12CommandContext.h"
struct FrameCB { DirectX::XMFLOAT4X4 WVP; };
struct Vertex { float Pos[3]; float UV[2]; };
static const char* g_ShaderSrc = R"(
cbuffer CB:register(b0)
{
	float4x4 WVP;
};
Texture2D gTex:register(t0);
SamplerState gSamp:register(s0);
struct VSInput { float3 Pos : POSITION; float2 UV:TEXCOORD; };
struct PSInput { float4 Pos : SV_POSITION; float2 UV : TEXCOORD; };
PSInput VSMain(VSInput v) { 
PSInput o; 
o.Pos =  mul(float4(v.Pos, 1.0), WVP); //float4(v.Pos, 1.0);
o.UV = v.UV; 
return o; }
float4 PSMain(PSInput i) : SV_TARGET { return gTex.Sample(gSamp, i.UV); }
)";

class RHITestPeriod1
{
public:
	void InitializedD3D12Device(HWND hwnd)
	{
		// 1. 通过RHI抽象建后端
		RHI = std::make_unique<FD3D12DynamicRHI>();
		GDynamicRHI = RHI.get();// 全局分发入口指向它；之后 RHICreateBuffer/Texture 会走这里
		GDynamicRHI->Init();


		Device = RHI->GetDevice();     // 过渡期：Viewport/Queue/CmdList/SRV/DepthBuffer 仍需具体 Device
		Queue = &Device->GetQueue(ED3D12QueueType::Direct);

		// 2.viewport （swapchain + RTV）
		Viewport = std::make_unique<FD3D12Viewport>(RHI->GetAdapter(), hwnd, Width, Height, DXGI_FORMAT_R8G8B8A8_UNORM, 2);
		Viewport->Init(); //  创建了swapchin rtv堆，并且创建与backbuffer相关联的rtv，并将这些rtv与对应的backbuffer关联了起来。

		DSVHeap = std::make_unique<FD3D12DescriptorHeap>(Device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
		DepthBuffer = Device->CreateDepthBuffer(Width, Height);
		Device->GetDevice()->CreateDepthStencilView(DepthBuffer->GetResource(), nullptr, DSVHeap->GetCPUHandle(0));

		//3 顶点缓冲
		Vertex Cube[24] = {
			{{-0.5f,-0.5f, 0.5f},{0,1}},{{-0.5f, 0.5f, 0.5f},{0,0}},{{ 0.5f, 0.5f, 0.5f},{1,0}},{{ 0.5f,-0.5f, 0.5f},{1,1}}, // +Z
			{{ 0.5f,-0.5f,-0.5f},{0,1}},{{ 0.5f, 0.5f,-0.5f},{0,0}},{{-0.5f, 0.5f,-0.5f},{1,0}},{{-0.5f,-0.5f,-0.5f},{1,1}}, // -Z
			{{ 0.5f,-0.5f, 0.5f},{0,1}},{{ 0.5f, 0.5f, 0.5f},{0,0}},{{ 0.5f, 0.5f,-0.5f},{1,0}},{{ 0.5f,-0.5f,-0.5f},{1,1}}, // +X
			{{-0.5f,-0.5f,-0.5f},{0,1}},{{-0.5f, 0.5f,-0.5f},{0,0}},{{-0.5f, 0.5f, 0.5f},{1,0}},{{-0.5f,-0.5f, 0.5f},{1,1}}, // -X
			{{-0.5f, 0.5f, 0.5f},{0,1}},{{-0.5f, 0.5f,-0.5f},{0,0}},{{ 0.5f, 0.5f,-0.5f},{1,0}},{{ 0.5f, 0.5f, 0.5f},{1,1}}, // +Y
			{{-0.5f,-0.5f,-0.5f},{0,1}},{{-0.5f,-0.5f, 0.5f},{0,0}},{{ 0.5f,-0.5f, 0.5f},{1,0}},{{ 0.5f,-0.5f,-0.5f},{1,1}}, // -Y
		};
		uint16 Indices[36] = {
			0,1,2, 0,2,3,      4,5,6, 4,6,7,
			8,9,10, 8,10,11,   12,13,14, 12,14,15,
			16,17,18, 16,18,19, 20,21,22, 20,22,23,
		};


		// 我们要求创建一个uploadbuffer上的顶点buffer区
		FRHIBufferDesc VBDesc(sizeof(Cube), sizeof(Vertex), EBufferUsageFlags::VertexBuffer | EBufferUsageFlags::Dynamic);
		VB = RHICreateBuffer(VBDesc, Cube);


		FRHIBufferDesc IBDesc(sizeof(Indices), sizeof(uint16), EBufferUsageFlags::IndexBuffer | EBufferUsageFlags::Dynamic);
		IB = RHICreateBuffer(IBDesc, Indices);



		// 4. 编译shader（blob只在建pso的时候用，局部即可）
		ComPtr<ID3DBlob> VSBlob = CompileShader(g_ShaderSrc, "VSMain", "vs_5_0");
		ComPtr<ID3DBlob> PSBlob = CompileShader(g_ShaderSrc, "PSMain", "ps_5_0");

		//5. root signature(2 参 + 1 static sampler + 允许输入布局就行)
		// SRV 通过描述表的形式给
		D3D12_DESCRIPTOR_RANGE SRVRange = {};
		SRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		SRVRange.NumDescriptors = 1;
		SRVRange.BaseShaderRegister = 0; //t0
		SRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;//，这段在整个 descriptor table 里的偏移(以 descriptor 个数计)。就是-1默认表示"紧跟在上一段后面自动排",不用手动算偏移。这是最省心的写法:一个 table 里如果有多段(比如先一段 CBV、再一段 SRV),每段都写 APPEND,D3D12 就自动按顺序首尾相接。
		
		// 根参数两个，一个用来传递WVP矩阵，一个用来指明SRV
		D3D12_ROOT_PARAMETER RootParams[2] = {};
		RootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;// b0(VS)
		RootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		RootParams[0].Descriptor.ShaderRegister = 0;
		RootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		RootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		RootParams[1].DescriptorTable.NumDescriptorRanges = 1;
		RootParams[1].DescriptorTable.pDescriptorRanges = &SRVRange;

		// 静态采样器
		D3D12_STATIC_SAMPLER_DESC Samp = {};
		Samp.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;// 纹理缩小 放大 两层mip之间都直接使用point采样
		Samp.AddressU = Samp.AddressV = Samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		Samp.ShaderRegister = 0;
		Samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		Samp.MaxLOD = D3D12_FLOAT32_MAX;

		D3D12_ROOT_SIGNATURE_DESC RSDesc = {};
		RSDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		RSDesc.NumParameters = 2;
		RSDesc.pParameters = RootParams;
		RSDesc.pStaticSamplers = &Samp;
		RSDesc.NumStaticSamplers = 1;
		RootSig = std::make_unique<FD3D12RootSignature>(Device, RSDesc);


		//6. PSO
		D3D12_INPUT_ELEMENT_DESC InputElems[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		};
		D3D12_RASTERIZER_DESC Raster = {};
		Raster.FillMode = D3D12_FILL_MODE_SOLID;
		Raster.CullMode = D3D12_CULL_MODE_NONE;
		Raster.DepthClipEnable = true;

		D3D12_BLEND_DESC Blend = {};
		Blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		D3D12_DEPTH_STENCIL_DESC DepthStencil = {}; // DepthEnable / StencilEnable 默认 FALSE
		DepthStencil.DepthEnable = true;
		DepthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;  // ← 通过的像素写入深度
		DepthStencil.DepthFunc = D3D12_COMPARISON_FUNC::D3D12_COMPARISON_FUNC_LESS;//  ← 更近(z 更小)才通过
		DepthStencil.StencilEnable = false;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {};
		PSODesc.pRootSignature = RootSig->GetRootSignature();
		PSODesc.VS = { VSBlob->GetBufferPointer(), VSBlob->GetBufferSize() };
		PSODesc.PS = { PSBlob->GetBufferPointer(), PSBlob->GetBufferSize() };
		PSODesc.InputLayout = { InputElems, 2 };
		PSODesc.RasterizerState = Raster;
		PSODesc.BlendState = Blend;
		PSODesc.DepthStencilState = DepthStencil;
		PSODesc.SampleMask = UINT_MAX;
		PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		PSODesc.NumRenderTargets = 1;
		PSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		PSODesc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // ← 必须和深度缓冲格式一致
		PSODesc.SampleDesc.Count = 1;//
		PSO = std::make_unique<FD3D12PipelineState>(Device, PSODesc, RootSig.get());


		//9. 测试一个棋盘纹理
		SRVHeap = std::make_unique<FD3D12DescriptorHeap>(Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8, true);
		const uint32 TW = 256, TH = 256;
		std::vector<uint32> Pixels(TW* TH);
		for (uint32 y = 0; y < TH; ++y)
			for (uint32 x = 0; x < TW; ++x)
			{
				bool c = ((x >> 5) ^ (y >> 5)) & 1;              // 32px 棋盘
				Pixels[y * TW + x] = c ? 0xFFFFFFFFu : 0xFF404040u; // 小端 AABBGGRR：白 / 深灰
			}

		FRHITextureDesc TexDesc(TW, TH, PF_R8G8B8A8_UNORM);
		Tex = RHICreateTexture(TexDesc, Pixels.data());
		// SRV 还没抽象，过渡期 downcast（Tex 现在是 FRHITexture*）
		Device->CreateShaderResourceView(static_cast<FD3D12Texture*>(Tex.get()), SRVHeap.get());

		// 命令录制两层：context 接入帧缓冲/描述符基础设施，RHICmdList 包着它
		Context = std::make_unique<FD3D12CommandContext>();
		Context->Init(Device, Queue, Viewport.get(), DSVHeap.get(), SRVHeap.get());
		RHICmdList = std::make_unique<FRHICommandList>(Context.get());
	}

	void DrawTriangle()
	{
		RHICmdList->BeginFrame();
		const float ClearColor[4]={0.2f, 0.4f, 0.8f, 1.0f};
		RHICmdList->BeginRenderPass(ClearColor);
		RHICmdList->SetGraphicsPipelineState(PSO.get());
		// WVP 常量
		{
			using namespace DirectX;
			static float Angle = 0.0f;
			Angle += 0.01f;
			XMMATRIX World = XMMatrixRotationZ(Angle) * XMMatrixRotationX(Angle) * XMMatrixRotationY(Angle);
			XMMATRIX View = XMMatrixLookAtLH(
				XMVectorSet(0, 0, -3, 1),
				XMVectorSet(0, 0, 0, 1),
				XMVectorSet(0, 1, 0, 1));
			XMMATRIX Proj = XMMatrixPerspectiveFovLH(
				XMConvertToRadians(60),
				(float)Width / Height,
				0.1f,
				100.0f);

			XMMATRIX WVP = World * View * Proj;
			FrameCB Constants;
			XMStoreFloat4x4(&Constants.WVP, XMMatrixTranspose(WVP));

			RHICmdList->SetShaderConstants(0, &Constants, sizeof(FrameCB));
		}
		RHICmdList->SetTexture(1, Tex.get());
		RHICmdList->SetStreamSource(0, VB.get());
		RHICmdList->DrawIndexedPrimitive(IB.get(), 36);
		RHICmdList->EndRenderPass();
		RHICmdList->EndFrame();
	}
	void WaitForGPU()
	{
		RHICmdList->WaitForGPU();
	}
private:
	uint32 Width = 1280;
	uint32 Height = 720;
	std::unique_ptr<FD3D12DynamicRHI> RHI;
	std::unique_ptr<FD3D12Viewport> Viewport;
	FD3D12Device* Device = nullptr;// 非拥有，指向Adapter内部
	FD3D12Queue* Queue = nullptr;// 非拥有
	TRefCountPtr<FRHIBuffer> VB;
	TRefCountPtr<FRHIBuffer> IB;
	std::unique_ptr<FD3D12RootSignature> RootSig;
	std::unique_ptr<FD3D12PipelineState> PSO;
	std::unique_ptr<FD3D12DescriptorHeap> DSVHeap;
	std::unique_ptr<FD3D12Resource> DepthBuffer;
	std::unique_ptr<FD3D12DescriptorHeap> SRVHeap;
	TRefCountPtr<FRHITexture> Tex;
	// 命令录制两层（帧管理/CB ring/VBV/IBV/视口 都搬进 context 了）
	std::unique_ptr<FD3D12CommandContext> Context;
	std::unique_ptr<FRHICommandList> RHICmdList;
};

static bool g_Running = true;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_DESTROY)
	{
		g_Running = false;
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	WNDCLASS wc = {};
	wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
	wc.lpszClassName = "RHITestWindow"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClass(&wc);
	int32 Width = 1280;
	int32 Height = 720;
	RECT rc = { 0, 0, (LONG)Width, (LONG)Height };     // 想要的客户区
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE); // 加上标题栏+边框，算出整窗尺寸

	HWND hwnd = CreateWindowEx(0, "RHITestWindow", "BaseTestEngine RHI - Triangle",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left,   // ← 用反算后的整窗宽
		rc.bottom - rc.top,   // ← 整窗高
		nullptr, nullptr, hInst, nullptr);
	ShowWindow(hwnd, nCmdShow);

	RHITestPeriod1 App;
	App.InitializedD3D12Device(hwnd);

	MSG msg = {};
	while (g_Running)
	{
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		App.DrawTriangle();
	}
	App.WaitForGPU();   //不再每帧 Flush，退出前等 GPU 把在飞的帧跑完，再让 App 析构销毁资源
	return 0;   // 最后一帧 DrawTriangle 已 Flush，GPU 已空闲
}
