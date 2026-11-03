#include <Windows.h>
#include <DirectXMath.h>
#include "DynamicRHI.h"
#include "RHICommandList.h"
#include "TestPlatform.h"
#include "D3DShaderCompiler.h"
#include "TestShaders.h"
#include "TestMeshResources.h"
#include "LocalVertexFactory.h"
#include "MeshPassProcessor.h"
#include <cstddef>
#include <cstring>
#include <fstream>
#include <string>
#include "Logging/Log.h"

#include "RHITestPeriod1.h"
struct FrameCB { DirectX::XMFLOAT4X4 WVP; };
struct Vertex { float Pos[3]; float UV[2]; };

struct RHITestPeriod1::FImplementation
{
public:



	void Initialize(void* hwnd, bool bValidate, uint32 InWidth, uint32 InHeight)
	{
        Width = InWidth; Height = InHeight;
        if (!Width || !Height) throw std::invalid_argument("Nonzero viewport size required");
        if (bValidate) { Width = 640; Height = 480; ValidateStageCCompiler(); ValidateStageCVertexFactory(); }
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
        VB.SetData(VertexData.data(), static_cast<uint32>(VertexData.size()), sizeof(Vertex));
        IB.SetData(Indices, sizeof(Indices), sizeof(uint16));
        BeginInitResource(&VB);
        BeginInitResource(&IB);

		/*
		 * 6. 编译 Shader，并创建 RHI Shader 对象
		 */
        // 注册只描述如何编译；Map 统一创建并复用 Shader 实例，Draw 不再编译。
        RegisterTestShaderTypes();
        GlobalShaderMap.CompileMissing(CompileD3DShader, LoadTestShaderSource);
        VertexShader = GlobalShaderMap.GetShader<FTestVS>();
        PixelShader = GlobalShaderMap.GetShader<FTestPS>();



		/*
		 * 7. 定义顶点声明 默认语义名是ATTRIBUTE
		 */
        // 同一 Buffer、流起点和 Stride 的两个属性自动合并为 Stream 0。
        FLocalVertexFactory::FDataType CubeData;
        CubeData.PositionComponent = FVertexStreamComponent(&VB, 16, offsetof(Vertex, Pos), sizeof(Vertex), VET_Float3);
        CubeData.TextureCoordinates.push_back(FVertexStreamComponent(&VB, 16, offsetof(Vertex, UV), sizeof(Vertex), VET_Float2));
        CubeFactory.SetData(CubeData);
        BeginInitResource(&CubeFactory);

		/*
		 * 8.汇总固定状态，创建 PSO
		 */
		FGraphicsPipelineStateInitializer Initialier;
		TRefCountPtr<FRHIVertexDeclaration> VertexDeclaration = CubeFactory.GetDeclaration();
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

		// Processor 构造器本身只是保存模板
        SceneProcessor = std::make_unique<FMeshPassProcessor>(Initialier);
        // 如果 GetPipelineState 缓存未命中、进入 RHI PSO 创建路径时，会处理对应根签名；
        PSO = SceneProcessor->GetPipelineState(CubeFactory);


        // 全屏 Pass 使用同一个 Shader，WVP 为单位矩阵，关闭深度写入/比较限制。
        Initialier.DepthStencilState = RHICreateDepthStencilState(FDepthStencilStateInitializerRHI(false, CF_Always));
        const Vertex Quad[] = {
            {{-1,-1,0},{0,1}}, {{-1,1,0},{0,0}}, {{1,1,0},{1,0}}, {{1,-1,0},{1,1}}
        };
        const uint16 QuadIndices[] = {0,1,2,0,2,3};
        // 全屏网格拆成两条流，实际验证 Position 与 UV 按同一个顶点索引读取。
        float Positions[4][3], UVs[4][2];
        for (uint32 Index = 0; Index < 4; ++Index)
        {
            std::memcpy(Positions[Index], Quad[Index].Pos, sizeof(Positions[Index]));
            std::memcpy(UVs[Index], Quad[Index].UV, sizeof(UVs[Index]));
        }
        FullscreenVB.SetData(Positions, sizeof(Positions), sizeof(Positions[0]));
        FullscreenUV.SetData(UVs, sizeof(UVs), sizeof(UVs[0]));
        FullscreenIB.SetData(QuadIndices, sizeof(QuadIndices), sizeof(uint16));
        BeginInitResource(&FullscreenVB);
        BeginInitResource(&FullscreenUV);
        BeginInitResource(&FullscreenIB);
        FLocalVertexFactory::FDataType QuadData;
        QuadData.PositionComponent = FVertexStreamComponent(&FullscreenVB, 0, sizeof(Positions[0]), VET_Float3);
        QuadData.TextureCoordinates.push_back(FVertexStreamComponent(&FullscreenUV, 0, sizeof(UVs[0]), VET_Float2));
        FullscreenFactory.SetData(QuadData);
        BeginInitResource(&FullscreenFactory);
        Initialier.BoundShaderState.VertexDeclarationRHI = FullscreenFactory.GetDeclaration();
        CompositeProcessor = std::make_unique<FMeshPassProcessor>(Initialier);
        FullscreenPSO = CompositeProcessor->GetPipelineState(FullscreenFactory);
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
        if (bValidate) ValidateStageCMeshDraw(Initialier, VertexShader->GetConstantsParameter(),
            PixelShader->GetTextureParameter(), TexSRV);

	}

	void DrawTriangle(bool bPresent, float DeltaSeconds)
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

        FrameCB Constants;
		// WVP 常量
		{
			using namespace DirectX;
			if (bAnimating) Angle += 0.6f * DeltaSeconds;
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
			XMStoreFloat4x4(&Constants.WVP, XMMatrixTranspose(WVP));

        }
        const std::vector<FMeshDrawCommand> SceneCommands = BuildMeshCommands(
            *SceneProcessor, CubeFactory, IB, 12, Constants, TexSRV);
        for (const FMeshDrawCommand& Command : SceneCommands) Command.SubmitDraw(*RHICmdList);
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
        FrameCB Identity;
        DirectX::XMStoreFloat4x4(&Identity.WVP, DirectX::XMMatrixIdentity());
        const std::vector<FMeshDrawCommand> CompositeCommands = BuildMeshCommands(
            *CompositeProcessor, FullscreenFactory, FullscreenIB, 2, Identity, SceneColorSRV);
        for (const FMeshDrawCommand& Command : CompositeCommands) Command.SubmitDraw(*RHICmdList);
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
		if (!bSwapTexture)
		{
			return;
		}
		bSwapTexture = false;
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
        ValidateBackend(Viewport.get(), DepthBuffer.get(), PSO.get(), VB.GetRHI().get(), IB.GetRHI().get());
        ValidateStageB(Viewport.get(), DepthBuffer.get());
        for (uint32 Index = 0; Index < 120; ++Index)
        {
            bSwapTexture = true;
            DrawTriangle(true, 1.0f / 60.0f);
        }
        DrawTriangle(false, 1.0f / 60.0f);
        ValidateRenderedImage(RHIGetViewportBackBuffer(Viewport.get()).get(), "stage-b-frame.ppm");
        WaitForGPU();
        // 采样输出必须与离屏结果逐像素一致，比仅检查“有非背景像素”更严格。
        if (ReadbackTextureRGBA(SceneColor.get()) != ReadbackTextureRGBA(RHIGetViewportBackBuffer(Viewport.get()).get()))
            throw std::runtime_error("Composite output differs from offscreen scene");
        std::ofstream("stage-b-checks.txt", std::ios::app) << "PASS offscreen/composite exact RGBA comparison after 121 frames\n";
        CheckDebugMessages();
    }
    // 交换链、深度和离屏目标始终保持同一物理像素尺寸。
    void Resize(uint32 NewWidth, uint32 NewHeight)
    {
        if (!NewWidth || !NewHeight || (NewWidth == Width && NewHeight == Height)) return;
        // RHIResizeViewport 等待帧完成并清理在飞引用；本对象不缓存 backbuffer。
        RHIResizeViewport(Viewport.get(), NewWidth, NewHeight);
        SceneColorSRV.reset();
        SceneColor.reset();
        DepthBuffer.reset();
        Width = NewWidth;
        Height = NewHeight;
        FRHITextureDesc DepthDesc(Width, Height, PF_D32_FLOAT);
        DepthDesc.Flags = ETextureCreateFlags::DepthStencilTargetable;
        DepthDesc.ClearValue = FClearValueBinding(1.0f, 0);
        DepthBuffer = RHICreateTexture(DepthDesc);
        FRHITextureDesc SceneDesc(Width, Height, PF_R8G8B8A8_UNORM);
        SceneDesc.Flags = ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource;
        SceneDesc.ClearValue = FClearValueBinding(std::array<float, 4>{0.2f, 0.4f, 0.8f, 1.0f});
        SceneColor = RHICreateTexture(SceneDesc);
        SceneColorSRV = RHICreateShaderResourceView(SceneColor, FRHIViewDesc::CreateTextureSRV().Build());
    }
    bool bSwapTexture = false;
    bool bAnimating = true;
    float Angle = 0.0f;
public:
    // 当前由示例提供 WVP/纹理，尚未建立 MaterialRenderProxy/SceneView。
    // Build 只生成 CPU 绘制快照，SubmitDraw 才录制命令。
    std::vector<FMeshDrawCommand> BuildMeshCommands(FMeshPassProcessor& Processor,
        const FVertexFactory& Factory, const FIndexBuffer& IndexBuffer, uint32 NumPrimitives,
        const FrameCB& Constants, const TRefCountPtr<FRHIShaderResourceView>& Texture)
    {
        FMeshBatch Mesh;
        Mesh.VertexFactory = &Factory;
        FMeshBatchElement Element;
        Element.IndexBuffer = &IndexBuffer;
        Element.NumPrimitives = NumPrimitives;
        Mesh.Elements.push_back(Element);
        FMeshDrawShaderBindings Bindings;
        Bindings.Add(VertexShader->GetConstantsParameter(), SF_Vertex, &Constants, sizeof(Constants));
        Bindings.Add(PixelShader->GetTextureParameter(), SF_Pixel, Texture);
        std::vector<FMeshDrawCommand> Commands;
        Processor.AddMeshBatch(Mesh, Bindings, Commands);
        return Commands;
    }
    std::unique_ptr<FMeshPassProcessor> SceneProcessor, CompositeProcessor;
    FGlobalShaderMap GlobalShaderMap;
    TShaderRef<FTestVS> VertexShader;
    TShaderRef<FTestPS> PixelShader;
    TRefCountPtr<FRHITexture> SceneColor;
    TRefCountPtr<FRHIShaderResourceView> SceneColorSRV;
    TRefCountPtr<FRHIGraphicsPipelineState> FullscreenPSO;
    // 声明顺序保证 Factory 先析构，借用的 Buffer 后析构。
    FTestVertexBuffer FullscreenVB, FullscreenUV;
    FTestIndexBuffer FullscreenIB;
    FLocalVertexFactory FullscreenFactory;
    uint64 FrameNumber = 0;
    size_t Variant = 0;
    std::array<TRefCountPtr<FRHITexture>, 2> TextureVariants;
	uint32 Width = 1920;
	uint32 Height = 1080;
	TRefCountPtr<FRHIViewport> Viewport;
	TRefCountPtr<FRHIGraphicsPipelineState> PSO;
    FTestVertexBuffer VB;
    FTestIndexBuffer IB;
    FLocalVertexFactory CubeFactory;
	TRefCountPtr<FRHITexture> DepthBuffer;

	TRefCountPtr<FRHITexture> Tex;
	TRefCountPtr<FRHIShaderResourceView> TexSRV;
	// 借用 RHI 启动阶段建立的即时命令列表。
    FRHICommandListImmediate* RHICmdList = nullptr;
};

RHITestPeriod1::RHITestPeriod1() : Implementation(std::make_unique<FImplementation>()) {}
RHITestPeriod1::~RHITestPeriod1() = default;
void RHITestPeriod1::Initialize(void* Window, bool Validate, std::uint32_t Width, std::uint32_t Height)
{
    Implementation->Initialize(Window, Validate, Width, Height);
}
void RHITestPeriod1::DrawTriangle(bool Present, float DeltaSeconds) { Implementation->DrawTriangle(Present, DeltaSeconds); }
void RHITestPeriod1::Resize(std::uint32_t Width, std::uint32_t Height) { Implementation->Resize(Width, Height); }
void RHITestPeriod1::RequestTextureSwap() { Implementation->bSwapTexture = !Implementation->bSwapTexture; }
void RHITestPeriod1::SetAnimating(bool Enabled) { Implementation->bAnimating = Enabled; }
void RHITestPeriod1::WaitForGPU() { Implementation->WaitForGPU(); }
void RHITestPeriod1::Validate() { Implementation->Validate(); }
void RHITestPeriod1::ValidateOutput(const char* OutputPath)
{
    auto& App = *Implementation;
    ValidateRenderedImage(RHIGetViewportBackBuffer(App.Viewport.get()).get(), OutputPath);
    if (ReadbackTextureRGBA(App.SceneColor.get()) != ReadbackTextureRGBA(RHIGetViewportBackBuffer(App.Viewport.get()).get()))
        throw std::runtime_error("Editor composite output differs from offscreen scene");
}
