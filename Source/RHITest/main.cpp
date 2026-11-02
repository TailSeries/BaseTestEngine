#include <Windows.h>
#include <DirectXMath.h>
#include "DynamicRHI.h"
#include "RHICommandList.h"
#include "TestPlatform.h"
#include "D3DShaderCompiler.h"
#include "TestShaders.h"
#include <cstddef>
#include <cstring>
#include <fstream>
#include <string>
#include "Logging/Log.h"

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
        if (bValidate) { Width = 640; Height = 480; ValidateStageCCompiler(); }
		/*1. 获取创建好的即时命令列表*/
        RHICmdList = &FRHICommandListExecutor::GetImmediateCommandList();


		/*2. 创建窗口的viewport
		 * 已经一步到位将backbuffer包装成FD3D12Texture，以及创建了对应的RTV了
		 */
		Viewport = RHICreateViewport(hwnd, Width, Height, false, PF_R8G8B8A8_UNORM);


		/*
		 * 3. 创建深度纹理及 DSV，最终在函数FD3D12Device::CreateDepthBuffer里会创建对应的DSV出来
		 */
		FRHITextureDesc DepthDesc(Width, Height, PF_D32_FLOAT);
		DepthDesc.Flags = ETextureCreateFlags::DepthStencilTargetable;
		DepthDesc.ClearValue = FClearValueBinding(1.0f, 0);
		DepthBuffer = RHICreateTexture(DepthDesc, nullptr);

		/*
		 * 4. 准备立方体的顶点与索引
		 */
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

		/*
		 * 5. 创建VB 和 IB
		 */
		// 我们要求创建一个uploadbuffer上的顶点buffer区
		std::vector<uint8> VertexData(16 + sizeof(Cube));// +16 验证一下顶点缓冲的Offset功能
        std::memcpy(VertexData.data() + 16, Cube, sizeof(Cube));
        FRHIBufferDesc VBDesc(static_cast<uint32>(VertexData.size()), sizeof(Vertex),
            EBufferUsageFlags::VertexBuffer | EBufferUsageFlags::Dynamic);
        VB = RHICreateBuffer(VBDesc, VertexData.data());


		FRHIBufferDesc IBDesc(sizeof(Indices), sizeof(uint16), EBufferUsageFlags::IndexBuffer | EBufferUsageFlags::Dynamic);
		IB = RHICreateBuffer(IBDesc, Indices);


		/*
		 * 6. 编译 Shader，并创建 RHI Shader 对象
		 */
        // 阶段 C：编译产物同时拥有字节码、参数映射和反射得到的资源范围。
        // 编译器属于平台工具层，RHI 只消费编译结果，不依赖 RenderCore。
        const auto VSOutput = CompileD3DShader({g_ShaderSrc, "VSMain", SF_Vertex});
        const auto PSOutput = CompileD3DShader({g_ShaderSrc, "PSMain", SF_Pixel});
        if (!VSOutput.bSucceeded || !PSOutput.bSucceeded)
            throw std::runtime_error(VSOutput.Diagnostics + PSOutput.Diagnostics);
        // 参数成员在构造时按名字解析一次；TShaderRef 将元信息与 RHI 资源关联。
        VertexShader = CreateShader<FTestVS>(VSOutput);
        PixelShader = CreateShader<FTestPS>(PSOutput);



		/*
		 * 7. 定义顶点声明 默认语义名是ATTRIBUTE
		 */
		static_assert(sizeof(Vertex) <= 65535);
		static_assert(offsetof(Vertex, Pos) <= 255);
		static_assert(offsetof(Vertex, UV) <= 255);
		FVertexDeclarationElementList Elements{
			FVertexElement(0, static_cast<uint8>(offsetof(Vertex, Pos)),VET_Float3, 0, static_cast<uint16>(sizeof(Vertex))),
			FVertexElement(0, static_cast<uint8>(offsetof(Vertex, UV)),VET_Float2, 1, static_cast<uint16>(sizeof(Vertex)))
		};

		/*
		 * 8.汇总固定状态，创建 PSO
		 */
		FGraphicsPipelineStateInitializer Initialier;
		TRefCountPtr<FRHIVertexDeclaration> VertexDeclaration = RHICreateVertexDeclaration(Elements);
		Initialier.BoundShaderState = FBoundShaderStateInput(VertexDeclaration, VertexShader.GetVertexShader(), PixelShader.GetPixelShader());
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

		// 注意这一步内部会根据 Initialier.BoundShaderState 创建出对应的根签名
        PSO = RHICreateGraphicsPipelineState(Initialier);


        // 全屏 Pass 使用同一个 Shader，WVP 为单位矩阵，关闭深度写入/比较限制。
        Initialier.DepthStencilState = RHICreateDepthStencilState(FDepthStencilStateInitializerRHI(false, CF_Always));
        FullscreenPSO = RHICreateGraphicsPipelineState(Initialier);
        const Vertex Quad[] = {
            {{-1,-1,0},{0,1}}, {{-1,1,0},{0,0}}, {{1,1,0},{1,0}}, {{1,-1,0},{1,1}}
        };
        const uint16 QuadIndices[] = {0,1,2,0,2,3};
        FullscreenVB = RHICreateBuffer(FRHIBufferDesc(sizeof(Quad), sizeof(Vertex),
            EBufferUsageFlags::VertexBuffer | EBufferUsageFlags::Dynamic), Quad);
        FullscreenIB = RHICreateBuffer(FRHIBufferDesc(sizeof(QuadIndices), sizeof(uint16),
            EBufferUsageFlags::IndexBuffer | EBufferUsageFlags::Dynamic), QuadIndices);
        FRHITextureDesc SceneDesc(Width, Height, PF_R8G8B8A8_UNORM);
        SceneDesc.Flags = ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource;
        SceneDesc.ClearValue = FClearValueBinding(std::array<float, 4>{0.2f, 0.4f, 0.8f, 1.0f});
        SceneColor = RHICreateTexture(SceneDesc);

        SceneColorSRV = RHICreateShaderResourceView(SceneColor, FRHIViewDesc::CreateTextureSRV().Build());


		//9. 创建棋盘纹理，再创建它的 SRV
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
		// 创建 SRV 只是准备好描述符，真正绑定到 PS 的 t0，要等绘制时调用 SetShaderResourceViewParameter()。
		TexSRV = RHICreateShaderResourceView(Tex, ViewDesc);

		/*
		 * 10. 预上传另一张纹理，供运行时切换
		 * Tex ───────────────→ 纹理
			                       ↑
			TexSRV → FRHIView ──────┘ 持有资源引用
		 */
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
        // 第一帧 SceneColor 初始为 RTV，后续帧为 SRV；Unknown 让后端使用已知追踪状态。
        RHICmdList->Transition(FRHITransitionInfo(SceneColor.get(), ERHIAccess::Unknown, ERHIAccess::RTV));
        PassInfo.ColorRenderTargets[0].RenderTarget = SceneColor.get();
		PassInfo.ColorRenderTargets[0].Action = ERenderTargetActions::Clear_Store;
		PassInfo.DepthStencilRenderTarget.DepthStencilTarget = DepthBuffer.get();
		/*
		* 深度：	Pass 开始时 用指定的深度清除值清空，Pass 结束时 保留深度结果，供后续 Pass 使用
		* 模板：Pass 开始时 不要求保留原来的模板内容， Pass 结束时 不要求保留模板结果
		*/
		PassInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::Clear_Store, ERenderTargetActions::DontLoad_DontStore);
		RHICmdList->BeginRenderPass(PassInfo, "ScenePass");

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

			// Shader 参数成员已从反射表解析 CB 的绑定位置，这里只提交数据。
			VertexShader->SetParameters(*RHICmdList, &Constants, sizeof(FrameCB));
		}
		PixelShader->SetParameters(*RHICmdList, TexSRV.get());
		RHICmdList->SetStreamSource(0, VB.get(), 16); // Buffer 前 16 字节为前缀，Stride 仍来自声明。
		RHICmdList->DrawIndexedPrimitive(IB.get(), 36);
		RHICmdList->EndRenderPass();
        // 第二个真正绘制 Pass：前一 Pass 的输出成为本 Pass 的输入。
        // 两条描述一次提交：RTV -> SRV；Present -> RTV。
        const FRHITransitionInfo BetweenPasses[] = {
            {SceneColor.get(), ERHIAccess::RTV, ERHIAccess::SRVGraphicsPixel},
            {BackBuffer.get(), ERHIAccess::Present, ERHIAccess::RTV}
        };
        RHICmdList->Transition(BetweenPasses);
        PassInfo.ColorRenderTargets[0].RenderTarget = BackBuffer.get();
        RHICmdList->BeginRenderPass(PassInfo, "CompositePass");
        RHICmdList->SetGraphicsPipelineState(FullscreenPSO.get());
        FrameCB Identity;
        DirectX::XMStoreFloat4x4(&Identity.WVP, DirectX::XMMatrixIdentity());
        VertexShader->SetParameters(*RHICmdList, &Identity, sizeof(Identity));
        PixelShader->SetParameters(*RHICmdList, SceneColorSRV.get());
        RHICmdList->SetStreamSource(0, FullscreenVB.get());
        RHICmdList->DrawIndexedPrimitive(FullscreenIB.get(), 6);
        RHICmdList->EndRenderPass();
        // 第三个空 Pass 保留 Stage A 的 Load/Store 回归检查，不需要往返 PRESENT。

        PassInfo.ColorRenderTargets[0].Action = ERenderTargetActions::Load_Store;
        PassInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(
            ERenderTargetActions::Load_Store, ERenderTargetActions::DontLoad_DontStore);
        RHICmdList->BeginRenderPass(PassInfo, "PreservePass");
        RHICmdList->EndRenderPass();
        RHICmdList->Transition(FRHITransitionInfo(BackBuffer.get(), ERHIAccess::RTV, ERHIAccess::Present));
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
        ValidateStageB(Viewport.get(), DepthBuffer.get());
        for (uint32 Index = 0; Index < 120; ++Index)
        {
            g_SwapTexture = true;
            DrawTriangle();
        }
        DrawTriangle(false);
        ValidateRenderedImage(RHIGetViewportBackBuffer(Viewport.get()).get(), "stage-b-frame.ppm");
        WaitForGPU();
        // 采样输出必须与离屏结果逐像素一致，比仅检查“有非背景像素”更严格。
        if (ReadbackTextureRGBA(SceneColor.get()) != ReadbackTextureRGBA(RHIGetViewportBackBuffer(Viewport.get()).get()))
            throw std::runtime_error("Composite output differs from offscreen scene");
        std::ofstream("stage-b-checks.txt", std::ios::app) << "PASS offscreen/composite exact RGBA comparison after 121 frames\n";
        CheckDebugMessages();
    }
private:
    TShaderRef<FTestVS> VertexShader;
    TShaderRef<FTestPS> PixelShader;
    TRefCountPtr<FRHITexture> SceneColor;
    TRefCountPtr<FRHIShaderResourceView> SceneColorSRV;
    TRefCountPtr<FRHIGraphicsPipelineState> FullscreenPSO;
    TRefCountPtr<FRHIBuffer> FullscreenVB, FullscreenIB;
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

	HWND hwnd = CreateWindowEx(0, "RHITestWindow", "BaseTestEngine RHI - Stage B Two Passes",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left,   // ← 用反算后的整窗宽
		rc.bottom - rc.top,   // ← 整窗高
		nullptr, nullptr, hInst, nullptr);
    ShowWindow(hwnd, bValidate ? SW_HIDE : nCmdShow);
    try
    {
        CoreLog::FLogOptions LogOptions;
        LogOptions.FilePath = "Logs/RHITest.log";
        LogOptions.Console = false;
        CoreLog::Initialize(LogOptions);
        RHIInit(CreateTestRHI(bValidate));
        CORE_LOG(LogTemp, Log, "Stage B: single Direct Queue, whole-resource transitions, offscreen + composite");
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
        if (bValidate) std::ofstream("stage-b-validation.txt") << "PASS: Stage B transitions, two-pass rendering, Stage A regression, 120 swaps, image readback, clean debug queue, shutdown\n";
        if (IsWindow(hwnd)) DestroyWindow(hwnd);
        return 0;
    }
    catch (const std::exception& Error)
    {
        CORE_LOG(LogTemp, Error, "Stage B failed: {}", Error.what());
        std::ofstream("stage-b-validation.txt") << "FAIL: " << Error.what() << '\n';
        if (!bValidate) MessageBoxA(hwnd, Error.what(), "RHI error", MB_OK | MB_ICONERROR);
        return 1;
    }
}
