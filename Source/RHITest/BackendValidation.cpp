#include "TestPlatform.h"
#include "D3D12DynamicRHI.h"
#include "D3D12Device.h"
#include "D3D12CommandContext.h"
#include "D3D12CommandList.h"
#include "D3D12Viewport.h"
#include "D3D12View.h"
#include <d3d12sdklayers.h>
#include <fstream>
#include <functional>
#include <cstring>

namespace
{
    FD3D12Device* Device()
    {
        return static_cast<FD3D12DynamicRHI*>(GDynamicRHI)->GetDevice();
    }
    void Require(bool Condition, const char* Message)
    {
        if (!Condition) throw std::runtime_error(Message);
    }
    void ExpectFailure(const std::function<void()>& Operation)
    {
        try { Operation(); }
        catch (const std::exception&) { return; }
        throw std::runtime_error("Expected invalid operation to fail");
    }
    // 即使验证抛异常，也解除 GPU gate，避免退出时等待永不完成的 Fence。
    struct FQueueGate
    {
        ComPtr<ID3D12Fence> Fence;
        explicit FQueueGate(FD3D12Device* D)
        {
            VERIFY_D3D12(D->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)));
            VERIFY_D3D12(D->GetQueue(ED3D12QueueType::Direct).GetD3DQueue()->Wait(Fence.Get(), 1));
        }
        void Open() { VERIFY_D3D12(Fence->Signal(1)); }
        ~FQueueGate() { Fence->Signal(1); }
    };
}

void CheckDebugMessages()
{
    if (!GDynamicRHI) return;
    ComPtr<ID3D12InfoQueue> Info;
    if (FAILED(Device()->GetDevice()->QueryInterface(IID_PPV_ARGS(&Info)))) return;
    for (UINT64 Index = 0; Index < Info->GetNumStoredMessagesAllowedByRetrievalFilter(); ++Index)
    {
        SIZE_T Size = 0;
        VERIFY_D3D12(Info->GetMessage(Index, nullptr, &Size));
        std::vector<uint8> Storage(Size);
        auto* Message = reinterpret_cast<D3D12_MESSAGE*>(Storage.data());
        VERIFY_D3D12(Info->GetMessage(Index, Message, &Size));
        if (Message->Severity <= D3D12_MESSAGE_SEVERITY_WARNING)
            throw std::runtime_error(std::string("D3D12 validation: ") + Message->pDescription);
    }
    Info->ClearStoredMessages();
}

void ValidateBackend(FRHIViewport* Viewport, FRHITexture* Depth, FRHIGraphicsPipelineState* PSO, FRHIBuffer* VB, FRHIBuffer* IB)
{
    std::ofstream Checks("stage-a-checks.txt");
    auto* D = Device();
    auto& Context = D->GetDefaultCommandContext();
    Context.WaitForGPU();

    // 固定容量、耗尽、重复释放、越界与复用，不依赖 assert。
    FD3D12DescriptorHeap Heap(D, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
    const uint32 A = Heap.Allocate(), B = Heap.Allocate();
    ExpectFailure([&] { Heap.Allocate(); });
    ExpectFailure([&] { Heap.GetCPUHandle(2); });
    ExpectFailure([&] { Heap.GetGPUHandle(0); });
    Heap.Free(A);
    ExpectFailure([&] { Heap.Free(A); });
    Require(Heap.Allocate() == A, "Descriptor slot was not reused");
    Heap.Free(A); Heap.Free(B);
    ExpectFailure([&] { Context.EndFrame(); });
    ExpectFailure([&] { Context.EndRenderPass(); });

    Checks << "PASS descriptor exhaustion/bounds/double-free/reuse and frame pairing\n";

    ExpectFailure([&] { RHICreateVertexShader(FRHICreateShaderDesc{std::span<const uint8>{}}); });
    Context.BeginFrame();
    Context.SetGraphicsPipelineState(PSO);
    const float Constants[16]{};
    for (uint32 Index = 0; Index < 256; ++Index)
        Context.SetShaderConstants(0, Constants, sizeof(Constants));
    ExpectFailure([&] { Context.SetShaderConstants(0, Constants, sizeof(Constants)); });
    Context.EndFrame();
    Context.WaitForGPU();
    Checks << "PASS empty shader rejection and bounded constant arena\n";

    // 重建次数超过 RTV 堆容量：必须复用旧 View 归还的槽位。
    auto* VP = static_cast<FD3D12Viewport*>(Viewport);
    const auto Original = VP->GetBackBuffer()->GetDesc();
    const uint32 RTVCount = D->GetRenderTargetDescriptorHeap()->GetAllocatedCount();
    for (uint32 Index = 0; Index < 12; ++Index)
    {
        VP->Resize(Original.Width + 1, Original.Height);
        VP->Resize(Original.Width, Original.Height);
    }
    Require(D->GetRenderTargetDescriptorHeap()->GetAllocatedCount() == RTVCount,
        "RTV descriptors leaked during Resize");
    const uint32 DSVCount = D->GetDepthStencilDescriptorHeap()->GetAllocatedCount();
    for (uint32 Index = 0; Index < 24; ++Index)
    {
        FRHITextureDesc Desc(32, 32, PF_D32_FLOAT);
        Desc.Flags = ETextureCreateFlags::DepthStencilTargetable;
        Desc.ClearValue = FClearValueBinding(1.0f, 0);
        auto Texture = RHICreateTexture(Desc);
    }
    Require(D->GetDepthStencilDescriptorHeap()->GetAllocatedCount() == DSVCount, "DSV leak");

    Checks << "PASS 24 backbuffer resize operations and 24 DSV create/destroy cycles\n";

    FRHITextureDesc Desc(2, 2, PF_R8G8B8A8_UNORM);
    Desc.Flags = ETextureCreateFlags::ShaderResource;
    const uint32 Pixels[4] = {0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF};
    auto Texture = RHICreateTexture(Desc, Pixels); // 上传在 gate 之前完成。
    auto View = RHICreateShaderResourceView(Texture, FRHIViewDesc::CreateTextureSRV().Build());
    std::weak_ptr<FRHITexture> WeakTexture = Texture;
    std::weak_ptr<FRHIShaderResourceView> WeakView = View;
    const auto OldHandle = static_cast<FD3D12ShaderResourceView_RHI*>(View.get())->GetGPUHandle().ptr;
    auto BackBuffer = RHIGetViewportBackBuffer(Viewport);
    Context.BeginFrame(); // 先等待可复用帧槽，再人为阻塞 GPU。
    ExpectFailure([&] { Context.BeginFrame(); });
    ExpectFailure([&] { Context.BeginRenderPass(FRHIRenderPassInfo{}, "Invalid"); });
    FQueueGate Gate(D);
    FRHIRenderPassInfo Pass;
    Pass.ColorRenderTargets[0].RenderTarget = BackBuffer.get();
    Pass.ColorRenderTargets[0].Action = ERenderTargetActions::Clear_Store;
    Pass.DepthStencilRenderTarget.DepthStencilTarget = Depth;
    Pass.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(
        ERenderTargetActions::Clear_Store, ERenderTargetActions::DontLoad_DontStore);
    auto& Cmd = FRHICommandListExecutor::GetImmediateCommandList();
    Cmd.Transition(FRHITransitionInfo(BackBuffer.get(), ERHIAccess::Present, ERHIAccess::RTV));
    Context.BeginRenderPass(Pass, "LifetimeValidation");
    ExpectFailure([&] { Context.BeginRenderPass(Pass, "Nested"); });
    Context.SetGraphicsPipelineState(PSO);
    const float Identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    Context.SetShaderConstants(0, Identity, sizeof(Identity));
    Context.SetShaderResourceViewParameter(0, View.get());
    Context.SetStreamSource(0, VB, 16);
    ExpectFailure([&] { Context.SetStreamSource(0, VB, VB->GetSize() + 1); });
    ExpectFailure([&] { Context.DrawIndexedPrimitive(IB, IB->GetSize()); });
    Context.DrawIndexedPrimitive(IB, 36);
    Context.EndRenderPass();
    Cmd.Transition(FRHITransitionInfo(BackBuffer.get(), ERHIAccess::RTV, ERHIAccess::Present));
    Context.DeferredDelete(View);
    View.reset(); Texture.reset();
    Require(!WeakView.expired() && !WeakTexture.expired(), "PendingDeletes failed to retain resources");
    Context.EndFrame();
    const auto& Q = D->GetQueue(ED3D12QueueType::Direct);
    Require(Q.Fence.D3DFence->GetCompletedValue() < Q.Fence.NextCompletionValue - 1,
        "GPU gate did not hold the submitted frame");
    Require(!WeakView.expired() && !WeakTexture.expired(), "In-flight resource released early");
    // GPU 尚未执行该 draw：新 View 必须使用另一个槽位。
    auto Another = RHICreateShaderResourceView(WeakTexture.lock(), FRHIViewDesc::CreateTextureSRV().Build());
    Require(static_cast<FD3D12ShaderResourceView_RHI*>(Another.get())->GetGPUHandle().ptr != OldHandle,
        "In-flight descriptor overwritten");
    Another.reset();
    Gate.Open();
    Context.WaitForGPU();
    Require(WeakView.expired() && WeakTexture.expired(), "Completed resources were not released");
    // LIFO 空闲链：旧 View 在 WaitForGPU 中最后释放，因此新 View 应重用它的槽。
    auto ReuseTexture = RHICreateTexture(Desc, Pixels);
    auto ReuseView = RHICreateShaderResourceView(ReuseTexture, FRHIViewDesc::CreateTextureSRV().Build());
    Require(static_cast<FD3D12ShaderResourceView_RHI*>(ReuseView.get())->GetGPUHandle().ptr == OldHandle,
        "Completed SRV descriptor was not recycled");
    CheckDebugMessages();
    Checks << "PASS real GPU-gated indexed draw, retained SRV/texture, protected slot, fence completion and reuse\n";
}

std::vector<uint8> ReadbackTextureRGBA(FRHITexture* Texture)
{
    auto* D = Device();
    auto& Q = D->GetQueue(ED3D12QueueType::Direct);
    auto* Native = static_cast<FD3D12Texture*>(Texture)->GetResource()->GetResource();
    const auto Desc = Native->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT Layout{};
    UINT64 TotalBytes = 0;
    D->GetDevice()->GetCopyableFootprints(&Desc, 0, 1, 0, &Layout, nullptr, nullptr, &TotalBytes);
    D3D12_HEAP_PROPERTIES Props{}; Props.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC Buffer{};
    Buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    Buffer.Width = TotalBytes; Buffer.Height = 1; Buffer.DepthOrArraySize = 1;
    Buffer.MipLevels = 1; Buffer.SampleDesc.Count = 1; Buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> Readback;
    VERIFY_D3D12(D->GetDevice()->CreateCommittedResource(&Props, D3D12_HEAP_FLAG_NONE,
        &Buffer, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&Readback)));
    FD3D12CommandAllocator Alloc(D, ED3D12QueueType::Direct);
    FD3D12CommandList List(D, &Alloc, ED3D12QueueType::Direct);
    List.Reset(&Alloc);
    auto* CL = List.GetCommandList();
    D3D12_RESOURCE_BARRIER Barrier{};
    Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    Barrier.Transition.pResource = Native;
    Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    // 独立诊断命令列表按同一 Direct Queue 顺序提交，并恢复调用前的状态。
    // 不把诊断用的临时 COPY_SOURCE 泄露给常规 Context 的状态追踪。
    Barrier.Transition.StateBefore = static_cast<FD3D12Texture*>(Texture)->GetResource()->GetState();
    Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    if (Barrier.Transition.StateBefore != Barrier.Transition.StateAfter) CL->ResourceBarrier(1, &Barrier);
    D3D12_TEXTURE_COPY_LOCATION Src{}; Src.pResource = Native;
    Src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION Dst{}; Dst.pResource = Readback.Get();
    Dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; Dst.PlacedFootprint = Layout;
    CL->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
    std::swap(Barrier.Transition.StateBefore, Barrier.Transition.StateAfter);
    if (Barrier.Transition.StateBefore != Barrier.Transition.StateAfter) CL->ResourceBarrier(1, &Barrier);
    List.Close();
    ID3D12CommandList* Lists[] = {CL};
    Q.GetD3DQueue()->ExecuteCommandLists(1, Lists);
    Q.WaitCPU(Q.Signal(Q.Fence));
    void* Mapped = nullptr;
    D3D12_RANGE Range{0, static_cast<SIZE_T>(TotalBytes)};
    VERIFY_D3D12(Readback->Map(0, &Range, &Mapped));
    const auto* Bytes = static_cast<const uint8*>(Mapped) + Layout.Offset;
    std::vector<uint8> Result(static_cast<size_t>(Desc.Width) * Desc.Height * 4);
    for (uint32 Y = 0; Y < Desc.Height; ++Y)
        std::memcpy(Result.data() + Y * Desc.Width * 4,
            Bytes + Y * Layout.Footprint.RowPitch, static_cast<size_t>(Desc.Width) * 4);
    D3D12_RANGE NoWrites{0,0}; Readback->Unmap(0, &NoWrites);
    return Result;
}

void ValidateRenderedImage(FRHITexture* Texture, const char* OutputPath)
{
    const auto Pixels = ReadbackTextureRGBA(Texture);
    const auto& Desc = Texture->GetDesc();
    std::ofstream Image(OutputPath, std::ios::binary);
    Image << "P6\n" << Desc.Width << ' ' << Desc.Height << "\n255\n";
    uint64 Changed = 0;
    const bool CornerMatches = Pixels[0] == 51 && Pixels[1] == 102 && Pixels[2] == 204;
    for (size_t I = 0; I < Pixels.size(); I += 4)
    {
        Image.write(reinterpret_cast<const char*>(Pixels.data() + I), 3);
        if (Pixels[I] != 51 || Pixels[I+1] != 102 || Pixels[I+2] != 204) ++Changed;
    }
    std::ofstream("stage-a-checks.txt", std::ios::app) << "Image: " << Desc.Width << "x" << Desc.Height
        << ", non-background pixels=" << Changed << ", corner matches clear=" << CornerMatches << '\n';
    Require(CornerMatches, "Clear color did not reach the backbuffer");
    Require(Changed > 1000 && Changed < static_cast<uint64>(Desc.Width) * Desc.Height / 2,
        "Cube image missing, or Load_Store erased the prior pass");
}
