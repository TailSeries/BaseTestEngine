#include <Windows.h>
#include "D3D12Adapter.h"    // 链了 D3D12RHI，其 Public 目录已在 include 路径
#include "D3D12Viewport.h"
#include "D3D12Resources.h"
#include "D3D12CommandList.h"
#include "D3D12Shader.h"
#include "D3D12RootSignature.h"
#include "D3D12PipelineState.h"
struct Vertex { float Pos[3]; float Color[4];};
static const char* g_ShaderSrc = R"(
struct VSInput { float3 Pos : POSITION; float4 Color : COLOR; };
struct PSInput { float4 Pos : SV_POSITION; float4 Color : COLOR; };
PSInput VSMain(VSInput v) { PSInput o; o.Pos = float4(v.Pos, 1.0); o.Color = v.Color; return o; }
float4 PSMain(PSInput i) : SV_TARGET { return i.Color; }
)";

class RHITestPeriod1
{
public:
    void InitializedD3D12Device(HWND hwnd)
    {
        // 1. Adapter → Device → Queues
        FD3D12AdapterDesc Desc;
        FD3D12Adapter::FindAdapter(Desc); //  失败可自行判断处理
        Adapter = std::make_unique<FD3D12Adapter>(Desc);
        Adapter->InitializeDevices(); // 初始化设备的时候，构造器直接建了三条队列，每条队列还建好了fence

        Device = Adapter->GetDevice();
        Queue = &Device->GetQueue(ED3D12QueueType::Direct);

        // 2.viewport （swapchain + RTV）
        Viewport = std::make_unique<FD3D12Viewport>(Adapter.get(), hwnd, Width, Height, DXGI_FORMAT_R8G8B8A8_UNORM, 2);
        Viewport->Init(); //  创建了swapchin rtv堆，并且创建与backbuffer相关联的rtv，并将这些rtv与对应的backbuffer关联了起来。

        //3 顶点缓冲
        Vertex Triangle[3] = {
    { {  0.0f,  0.5f, 0.0f }, { 1, 0, 0, 1 } },
    { {  0.5f, -0.5f, 0.0f }, { 0, 1, 0, 1 } },
    { { -0.5f, -0.5f, 0.0f }, { 0, 0, 1, 1 } },
        };

        // 我们要求创建一个uploadbuffer上的顶点buffer区
        FRHIBufferDesc VBDesc(sizeof(Triangle), sizeof(Vertex), EBufferUsageFlags::VertexBuffer | EBufferUsageFlags::Dynamic);
        VB = Device->CreateBuffer(VBDesc, Triangle); // 我们这里还只是创建了一个uploadbuffer上的东西

        VBV.BufferLocation = VB->GetResource()->GetGPUVirtualAddress();
        VBV.SizeInBytes = (uint32)sizeof(Triangle);
        VBV.StrideInBytes = (uint32)sizeof(Vertex);

        // 4. 编译shader（blob只在建pso的时候用，局部即可）
        ComPtr<ID3DBlob> VSBlob = CompileShader(g_ShaderSrc, "VSMain", "vs_5_0");
        ComPtr<ID3DBlob> PSBlob = CompileShader(g_ShaderSrc, "PSMain", "ps_5_0");

        //5. root signature(空 + 允许输入布局就行)
        D3D12_ROOT_SIGNATURE_DESC RSDesc = {};
        RSDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        RootSig = std::make_unique<FD3D12RootSignature>(Device, RSDesc);


        //6. PSO
        D3D12_INPUT_ELEMENT_DESC InputElems[]={
	        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };
        D3D12_RASTERIZER_DESC Raster = {};
        Raster.FillMode = D3D12_FILL_MODE_SOLID;
        Raster.CullMode = D3D12_CULL_MODE_NONE;
        Raster.DepthClipEnable = true;

        D3D12_BLEND_DESC Blend={};
        Blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        D3D12_DEPTH_STENCIL_DESC DepthStencil = {}; // DepthEnable / StencilEnable 默认 FALSE

        D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {};
        PSODesc.pRootSignature = RootSig->GetRootSignature();
        PSODesc.VS = { VSBlob->GetBufferPointer(), VSBlob->GetBufferSize() };
        PSODesc.PS = {PSBlob->GetBufferPointer(), PSBlob->GetBufferSize()};
        PSODesc.InputLayout = {InputElems, 2};
        PSODesc.RasterizerState = Raster;
        PSODesc.BlendState = Blend;
        PSODesc.DepthStencilState = DepthStencil;
        PSODesc.SampleMask = UINT_MAX;
        PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        PSODesc.NumRenderTargets = 1;
        PSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        PSODesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        PSODesc.SampleDesc.Count = 1;//
        PSO = std::make_unique<FD3D12PipelineState>(Device, PSODesc);

        //7. 命令分配器 + 列表
        CmdAlloc = std::make_unique<FD3D12CommandAllocator>(Device, ED3D12QueueType::Direct);
        CmdList = std::make_unique<FD3D12CommandList>(Device, CmdAlloc.get(), ED3D12QueueType::Direct);

        //8. 视口/裁剪
        VP = {0.0f, 0.0f, (float)Width, (float)Height, 0.0f, 1.0f};
        Scissor = {0, 0, (LONG)Width, (LONG)Height};

    }
    void DrawTriangle()
    {
        CmdAlloc->Reset();
        CmdList->Reset(CmdAlloc.get());
        ID3D12GraphicsCommandList* CL = CmdList->GetCommandList();

        ID3D12Resource* Backbuffer = Viewport->GetBackBuffer();
        D3D12_CPU_DESCRIPTOR_HANDLE RTV = Viewport->GetCurrentBackBufferRTV();

        // barrier PRESENT>>RENDER_TARGET
        D3D12_RESOURCE_BARRIER Barrier = {};
        Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        Barrier.Transition.pResource = Backbuffer;
        Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        CL->ResourceBarrier(1, &Barrier);

        // 清屏
        CL->OMSetRenderTargets(1, &RTV, false, nullptr);
        const float ClearColor[4] = { 0.2f, 0.4f, 0.8f, 1.0f };
        CL->ClearRenderTargetView(RTV, ClearColor, 0, nullptr);

        // 画图
        CL->RSSetViewports(1, &VP);
        CL->RSSetScissorRects(1, &Scissor);
        CL->SetGraphicsRootSignature(RootSig->GetRootSignature());
        CL->SetPipelineState(PSO->GetPipelineState());
        CL->IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        // 这里没传indexbuffer，传的时候输入装配器会按indexbuffer读，但是不传的话直接顺序0 1 2 3 4  5  6...读取，自动123 一个三角形，456一个三角形
        CL->IASetVertexBuffers(0, 1, &VBV);
        CL->DrawInstanced(3, 1, 0, 0);

        //barrier
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        CL->ResourceBarrier(1, &Barrier);

        CmdList->Close();
        ID3D12CommandList* List[]= {CL};
        Queue->GetD3DQueue()->ExecuteCommandLists(1, List);
        Viewport->PresentInternal(1);
        Queue->WaitCPU(Queue->Signal(Queue->Fence));
    };
private:
    uint32 Width = 1280;
    uint32 Height = 720;
    std::unique_ptr<FD3D12Adapter> Adapter;
    std::unique_ptr<FD3D12Viewport> Viewport;
    FD3D12Device* Device = nullptr;// 非拥有，指向Adapter内部
    FD3D12Queue* Queue = nullptr;// 非拥有

    TRefCountPtr<FD3D12Buffer> VB;
    D3D12_VERTEX_BUFFER_VIEW VBV{};
    std::unique_ptr<FD3D12RootSignature> RootSig;
    std::unique_ptr<FD3D12PipelineState> PSO;
    std::unique_ptr<FD3D12CommandAllocator> CmdAlloc;
    std::unique_ptr<FD3D12CommandList> CmdList;

    D3D12_VIEWPORT VP{};
    D3D12_RECT Scissor{};
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
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
    wc.lpszClassName = "RHITestWindow"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);
    HWND hwnd = CreateWindowEx(0, "RHITestWindow", "BaseTestEngine RHI - Triangle",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
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
    return 0;   // 最后一帧 DrawTriangle 已 Flush，GPU 已空闲
}
