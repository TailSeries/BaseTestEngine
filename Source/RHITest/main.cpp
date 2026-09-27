#include <Windows.h>
#include <DirectXMath.h>
#include "DynamicRHI.h"
#include "RHICommandList.h"
#include "TestPlatform.h"
#include <cstddef>
#include <cstring>
#include <fstream>
#include <string>

struct FrameCB { DirectX::XMFLOAT4X4 WVP; };
struct Vertex { float Pos[3]; float UV[2]; };
static const char* g_ShaderSrc = R"(
cbuffer CB:register(b0)
{
	float4x4 WVP;
};
Texture2D gTex:register(t0);
SamplerState gSamp:register(s0);

struct VSInput { 
	float3 Pos : ATTRIBUTE0; 
	float2 UV:ATTRIBUTE1; 
};

struct PSInput { 
	float4 Pos : SV_POSITION; 
	float2 UV : TEXCOORD; 
};

PSInput VSMain(VSInput v) { 
	PSInput o; 
	o.Pos =  mul(float4(v.Pos, 1.0), WVP); //float4(v.Pos, 1.0);
	o.UV = v.UV; 
	return o; 
}

float4 PSMain(PSInput i) : SV_TARGET { 
	return gTex.Sample(gSamp, i.UV); 
}
)";
static bool g_Running = true;
static bool g_SwapTexture = false;   // ← 加
class RHITestPeriod1
{
public:



	void Initialize(HWND hwnd, bool bValidate)
	{
        if (bValidate) { Width = 640; Height = 480; }
        RHICmdList = &FRHICommandListExecutor::GetImmediateCommandList();
		// Viewport 经 RHI 创建；交换链、RTV 和 backbuffer 包装由后端初始化。
		Viewport = RHICreateViewport(hwnd, Width, Height, false, PF_R8G8B8A8_UNORM);


		FRHITextureDesc DepthDesc(Width, Height, PF_D32_FLOAT);
		DepthDesc.Flags = ETextureCreateFlags::DepthStencilTargetable;
		DepthDesc.ClearValue = FClearValueBinding(1.0f, 0);
		DepthBuffer = RHICreateTexture(DepthDesc, nullptr);

// 深度纹理及其 DSV 由 RHI 创建路径一并初始化。
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
		std::vector<uint8> VertexData(16 + sizeof(Cube));
        std::memcpy(VertexData.data() + 16, Cube, sizeof(Cube));
        FRHIBufferDesc VBDesc(static_cast<uint32>(VertexData.size()), sizeof(Vertex),
            EBufferUsageFlags::VertexBuffer | EBufferUsageFlags::Dynamic);
        VB = RHICreateBuffer(VBDesc, VertexData.data());


		FRHIBufferDesc IBDesc(sizeof(Indices), sizeof(uint16), EBufferUsageFlags::IndexBuffer | EBufferUsageFlags::Dynamic);
		IB = RHICreateBuffer(IBDesc, Indices);



        // 编译适配器只返回字节码；Shader 资源仍由 RHI 创建。
        const auto VSCode = CompileTestShader(g_ShaderSrc, "VSMain", "vs_5_0");
        const auto PSCode = CompileTestShader(g_ShaderSrc, "PSMain", "ps_5_0");
        FRHICreateShaderDesc VSDesc{std::span<const uint8>(VSCode)};
        FRHICreateShaderDesc PSDesc{std::span<const uint8>(PSCode)};
		// 必须与实际编译出的 Shader 资源布局一致。
		// 当前 VS 使用 b0；PS 使用 t0、s0，均为 space0。
		VSDesc.ResourceCounts.NumCBs = 1;
		PSDesc.ResourceCounts.NumSRVs = 1;
		PSDesc.ResourceCounts.NumSamplers = 1;

		TRefCountPtr<FRHIVertexShader> VertexShader = RHICreateVertexShader(VSDesc);
		TRefCountPtr<FRHIPixelShader> PixelShader = RHICreatePixelShader(PSDesc);





		//5. root signature(2 参 + 1 static sampler + 允许输入布局就行), 已经交给RHI后端了，我们这略过



		//6. PSO

		static_assert(sizeof(Vertex) <= 65535);
		static_assert(offsetof(Vertex, Pos) <= 255);
		static_assert(offsetof(Vertex, UV) <= 255);

		FGraphicsPipelineStateInitializer Initialier;

		FVertexDeclarationElementList Elements{
			FVertexElement(0, static_cast<uint8>(offsetof(Vertex, Pos)),VET_Float3, 0, static_cast<uint16>(sizeof(Vertex))),
			FVertexElement(0, static_cast<uint8>(offsetof(Vertex, UV)),VET_Float2, 1, static_cast<uint16>(sizeof(Vertex)))
		};
		TRefCountPtr<FRHIVertexDeclaration> VertexDeclaration = RHICreateVertexDeclaration(Elements);
		Initialier.BoundShaderState = FBoundShaderStateInput(VertexDeclaration, VertexShader, PixelShader);
		FRasterizerStateInitializerRHI RasterInitializer(FM_Solid, CM_None, false);
		Initialier.RasterizerState = RHICreateRasterizerState(RasterInitializer);
		FBlendStateInitializerRHI BlendStateInitializer;
		Initialier.BlendState = RHICreateBlendState(BlendStateInitializer);
		FDepthStencilStateInitializerRHI DepthStencilStateInitializer(true, CF_Less);
		Initialier.DepthStencilState = RHICreateDepthStencilState(DepthStencilStateInitializer);
		Initialier.PrimitiveType = PT_TriangleList;
		Initialier.RenderTargetsEnabled = 1;
		Initialier.RenderTargetFormats[0] = PF_R8G8B8A8_UNORM;
		Initialier.DepthStencilTargetFormat = PF_D32_FLOAT;
		Initialier.NumSamples = 1;

		PSO = RHICreateGraphicsPipelineState(Initialier);


		//9. 测试一个棋盘纹理
		const uint32 TW = 256, TH = 256;
		std::vector<uint32> Pixels(TW * TH);
		for (uint32 y = 0; y < TH; ++y)
			for (uint32 x = 0; x < TW; ++x)
			{
				bool c = ((x >> 5) ^ (y >> 5)) & 1;              // 32px 棋盘
				Pixels[y * TW + x] = c ? 0xFFFFFFFFu : 0xFF404040u; // 小端 AABBGGRR：白 / 深灰
			}

		FRHITextureDesc TexDesc(TW, TH, PF_R8G8B8A8_UNORM);
		TexDesc.Flags = ETextureCreateFlags::ShaderResource;

		Tex = RHICreateTexture(TexDesc, Pixels.data());
		const FRHIViewDesc ViewDesc =
			FRHIViewDesc::CreateTextureSRV()
			.SetMipRange(0, 1)
			.Build();
		// 未指定 Format，沿用纹理格式。
		TexSRV = RHICreateShaderResourceView(Tex, ViewDesc);

        // 两张纹理在初始化阶段上传。运行时切换只创建/替换 SRV，避免上传等待
        // 把上一帧 GPU 工作全部排空，掩盖在飞资源的生命周期问题。
        TextureVariants[0] = Tex;
        for (uint32& Pixel : Pixels)
            Pixel = Pixel == 0xFFFFFFFFu ? 0xFF00FFFFu : 0xFFFF00FFu;
        TextureVariants[1] = RHICreateTexture(TexDesc, Pixels.data());

	}

	void DrawTriangle(bool bPresent = true)
	{
		RHICmdList->BeginFrame();
        MaybeSwapTexture(); // 先回收已完成帧的引用，再申请新 View。
		TRefCountPtr<FRHITexture> BackBuffer = RHIGetViewportBackBuffer(Viewport.get());
		FRHIRenderPassInfo PassInfo;
		PassInfo.ColorRenderTargets[0].RenderTarget = BackBuffer.get();
		PassInfo.ColorRenderTargets[0].Action = ERenderTargetActions::Clear_Store;
		PassInfo.DepthStencilRenderTarget.DepthStencilTarget = DepthBuffer.get();
		PassInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::Clear_Store, ERenderTargetActions::DontLoad_DontStore);
		RHICmdList->BeginRenderPass(PassInfo, "MainPass");

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
		RHICmdList->SetShaderResourceViewParameter(0, TexSRV.get());
		RHICmdList->SetStreamSource(0, VB.get(), 16); // Buffer 前 16 字节为前缀，Stride 仍来自声明。
		RHICmdList->DrawIndexedPrimitive(IB.get(), 36);
		RHICmdList->EndRenderPass();
        // 第二个空 Pass 使用 Load_Store；用于确认附件内容保留路径。
        PassInfo.ColorRenderTargets[0].Action = ERenderTargetActions::Load_Store;
        PassInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(
            ERenderTargetActions::Load_Store, ERenderTargetActions::DontLoad_DontStore);
        RHICmdList->BeginRenderPass(PassInfo, "PreservePass");
        RHICmdList->EndRenderPass();
        RHICmdList->EndFrame();
        RHICmdList->EndDrawingViewport(Viewport.get(), FRHIPresentArgs(FrameNumber++, bPresent, true));
	}
	void WaitForGPU()
	{
		RHICmdList->WaitForGPU();
	}

	void MaybeSwapTexture()
	{
		if (!g_SwapTexture)
		{
			return;
		}
		g_SwapTexture = false;
        Variant = (Variant + 1) % TextureVariants.size();
        TRefCountPtr<FRHITexture> NewTex = TextureVariants[Variant];

		const FRHIViewDesc ViewDesc = FRHIViewDesc::CreateTextureSRV().SetMipRange(0, 1).Build();
		TRefCountPtr<FRHIShaderResourceView> NewSRV = RHICreateShaderResourceView(NewTex, ViewDesc);
		// 旧 SRV 持有旧纹理。
		// 将旧 SRV 保留到 GPU 完成，同时保住它引用的纹理。
		RHICmdList->DeferredDelete(TexSRV);
		TexSRV = std::move(NewSRV);
		Tex = std::move(NewTex);
	}
    void Validate()
    {
        ValidateBackend(Viewport.get(), DepthBuffer.get(), PSO.get(), VB.get(), IB.get());
        for (uint32 Index = 0; Index < 120; ++Index)
        {
            g_SwapTexture = true;
            DrawTriangle();
        }
        DrawTriangle(false);
        ValidateRenderedImage(RHIGetViewportBackBuffer(Viewport.get()).get(), "stage-a-frame.ppm");
        WaitForGPU();
        CheckDebugMessages();
    }
private:
    uint64 FrameNumber = 0;
    size_t Variant = 0;
    std::array<TRefCountPtr<FRHITexture>, 2> TextureVariants;
	uint32 Width = 1920;
	uint32 Height = 1080;
	TRefCountPtr<FRHIViewport> Viewport;
	TRefCountPtr<FRHIGraphicsPipelineState> PSO;
	TRefCountPtr<FRHIBuffer> VB;
	TRefCountPtr<FRHIBuffer> IB;
	TRefCountPtr<FRHITexture> DepthBuffer;

	TRefCountPtr<FRHITexture> Tex;
	TRefCountPtr<FRHIShaderResourceView> TexSRV;
	// 借用 RHI 启动阶段建立的即时命令列表。
    FRHICommandListImmediate* RHICmdList = nullptr;
};



LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_DESTROY)
	{
		g_Running = false;
		PostQuitMessage(0);
		return 0;
	}
	if (msg == WM_KEYDOWN && wParam == VK_SPACE)   // ← 加：空格触发换纹理
	{
		g_SwapTexture = true;
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR CommandLine, int nCmdShow)
{
    const bool bValidate = std::string(CommandLine).find("--validate") != std::string::npos;
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	WNDCLASS wc = {};
	wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
	wc.lpszClassName = "RHITestWindow"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClass(&wc);
	int32 Width = bValidate ? 640 : 1920;
	int32 Height = bValidate ? 480 : 1080;
	RECT rc = { 0, 0, (LONG)Width, (LONG)Height };     // 想要的客户区
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE); // 加上标题栏+边框，算出整窗尺寸

	HWND hwnd = CreateWindowEx(0, "RHITestWindow", "BaseTestEngine RHI - Textured Cube",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left,   // ← 用反算后的整窗宽
		rc.bottom - rc.top,   // ← 整窗高
		nullptr, nullptr, hInst, nullptr);
    ShowWindow(hwnd, bValidate ? SW_HIDE : nCmdShow);
    try
    {
        RHIInit(CreateTestRHI(bValidate));
        {
            RHITestPeriod1 App;
            App.Initialize(hwnd, bValidate);
            if (bValidate)
            {
                App.Validate();
            }
            else
            {
                MSG Msg{};
                while (g_Running)
                {
                    while (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
                    {
                        TranslateMessage(&Msg);
                        DispatchMessage(&Msg);
                    }
                    if (g_Running) App.DrawTriangle();
                }
            }
            App.WaitForGPU();
        } // 所有上层资源先释放，再关闭 RHI。
        CheckDebugMessages();
        RHIExit();
        if (bValidate) std::ofstream("stage-a-validation.txt") << "PASS: descriptor reuse, GPU-gated lifetime, resize, render-pass clear/load, 120 swaps, image readback, clean debug queue, shutdown\n";
        if (IsWindow(hwnd)) DestroyWindow(hwnd);
        return 0;
    }
    catch (const std::exception& Error)
    {
        std::ofstream("stage-a-validation.txt") << "FAIL: " << Error.what() << '\n';
        if (!bValidate) MessageBoxA(hwnd, Error.what(), "RHI error", MB_OK | MB_ICONERROR);
        return 1;
    }
}
