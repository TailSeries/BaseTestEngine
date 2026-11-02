#include "TestPlatform.h"
#include "D3D12DynamicRHI.h"
#include "D3D12Device.h"
#include "D3D12CommandContext.h"
#include "D3D12LegacyBarriers.h"
#include <fstream>
#include <functional>

namespace
{
    void Check(bool Value, const char* Message) { if (!Value) throw std::runtime_error(Message); }
    void Reject(const std::function<void()>& Operation)
    {
        try { Operation(); } catch (const std::exception&) { return; }
        throw std::runtime_error("Stage B: invalid input was accepted");
    }
    using FTransitionOwner = std::unique_ptr<FRHITransition, decltype(&RHIReleaseTransition)>;
}

void ValidateStageB(FRHIViewport* Viewport, FRHITexture* Depth)
{
    auto* D = static_cast<FD3D12DynamicRHI*>(GDynamicRHI)->GetDevice();
    auto& Context = D->GetDefaultCommandContext();
    auto& Cmd = FRHICommandListExecutor::GetImmediateCommandList();
    std::ofstream Report("stage-b-checks.txt");
    auto BackBuffer = RHIGetViewportBackBuffer(Viewport);
    FRHITextureDesc Desc(Depth->GetDesc().Width, Depth->GetDesc().Height, PF_R8G8B8A8_UNORM);
    Desc.Flags = ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource;
    Desc.ClearValue = FClearValueBinding(std::array<float, 4>{0.2f,0.4f,0.8f,1});
    auto A = RHICreateTexture(Desc), B = RHICreateTexture(Desc);
    auto* RA = static_cast<FD3D12Texture*>(A.get())->GetResource();
    auto* RB = static_cast<FD3D12Texture*>(B.get())->GetResource();
    const FRHITransitionInfo ToRead(A.get(), ERHIAccess::RTV, ERHIAccess::SRVGraphicsPixel);
    Reject([&] { Cmd.Transition(ToRead); }); // 无帧时拒绝；临时转换对象应自动释放。
    Reject([&] { FRHISubresourceRange Invalid(65536, 0, 0); });
    Reject([&] { FTransitionOwner T(RHICreateTransition({ERHIPipeline::Graphics, ERHIPipeline::AsyncCompute}), RHIReleaseTransition); });
    Cmd.BeginFrame();
    const uint64 Start = Context.GetBarrierCount();
    Cmd.Transition(std::span<const FRHITransitionInfo>{});
    Cmd.Transition({A.get(), ERHIAccess::RTV, ERHIAccess::RTV});
    Check(Context.GetBarrierCount() == Start, "Redundant transition emitted a barrier");
    Cmd.Transition(ToRead);
    Cmd.Transition({A.get(), ERHIAccess::SRVGraphicsPixel, ERHIAccess::SRVGraphics});
    const uint64 Merged = Context.GetBarrierCount();
    Cmd.Transition({A.get(), ERHIAccess::SRVGraphics, ERHIAccess::SRVGraphicsPixel});
    Check(Context.GetBarrierCount() == Merged, "Read subset should not emit barrier");
    Check(RA->GetState() == (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE), "Read states not merged");
    Cmd.Transition({A.get(), ERHIAccess::Unknown, ERHIAccess::RTV});

    Reject([&] { Cmd.Transition({A.get(), ERHIAccess::CopyDest, ERHIAccess::RTV}); });
    Reject([&] { Cmd.Transition({A.get(), ERHIAccess::RTV, ERHIAccess::Unknown}); });
    Reject([&] { Cmd.Transition({A.get(), ERHIAccess::RTV, ERHIAccess::RTV | ERHIAccess::SRVGraphicsPixel}); });
    Reject([&] { Cmd.Transition({A.get(), ERHIAccess::RTV, ERHIAccess::Present}); });
    Reject([&] { Cmd.Transition({A.get(), ERHIAccess::RTV, ERHIAccess::UAVGraphics}); });
    Reject([&] { Cmd.Transition(FRHITransitionInfo{}); });
    Reject([&] { Cmd.Transition({A.get(), ERHIAccess::RTV, ERHIAccess::CopySrc, EResourceTransitionFlags::None, 0}); });
    const FRHITransitionInfo InvalidBatch[] = {
        {A.get(), ERHIAccess::RTV, ERHIAccess::CopySrc},
        {B.get(), ERHIAccess::RTV, ERHIAccess::DSVWrite}
    };
    const auto BeforeFailure = Context.GetBarrierCount();
    Reject([&] { Cmd.Transition(InvalidBatch); });
    Check(RA->GetState() == D3D12_RESOURCE_STATE_RENDER_TARGET && Context.GetBarrierCount() == BeforeFailure,
        "Failed batch partially mutated state or emitted commands");
    const FRHITransitionInfo Repeated[] = {
        {A.get(), ERHIAccess::RTV, ERHIAccess::CopyDest},
        {A.get(), ERHIAccess::CopyDest, ERHIAccess::RTV}
    };
    Cmd.Transition(Repeated);
    Check(RA->GetState() == D3D12_RESOURCE_STATE_RENDER_TARGET, "Repeated-resource batch failed");
    Report << "PASS repeated resource transitions in one batch\n";
    Report << "PASS no-frame/empty/redundant/read-merge/Unknown/bad-before/null/range/access validation and atomic batch\n";

    // 复制局部描述后修改原数组，证明后端持有自己的副本；检查尾部存储对齐。
    FRHITransitionInfo Local[] = {ToRead};
    FTransitionOwner Object(RHICreateTransition({ERHIPipeline::Graphics, ERHIPipeline::Graphics,
        ERHITransitionCreateFlags::None, Local}), RHIReleaseTransition);
    Local[0].AccessAfter = ERHIAccess::CopyDest;
    const auto* Data = Object->GetPrivateData<FD3D12LegacyBarriersTransitionData>();
    Check(Data->TransitionInfos[0].AccessAfter == ERHIAccess::SRVGraphicsPixel, "Transition descriptors were not copied");
    Check(reinterpret_cast<uintptr_t>(Data) % alignof(FD3D12LegacyBarriersTransitionData) == 0, "Private data misaligned");
    const FRHITransition* Pointer = Object.get();
    std::span<const FRHITransition* const> Batch(&Pointer, 1);
    Reject([&] { Context.RHIEndTransitions(Batch); });
    Context.RHIBeginTransitions(Batch);
    Reject([&] { Context.RHIBeginTransitions(Batch); });
    Reject([&] { Context.EndFrame(); });
    Context.RHIEndTransitions(Batch);
    Reject([&] { Context.RHIEndTransitions(Batch); });
    Cmd.Transition({A.get(), ERHIAccess::Unknown, ERHIAccess::RTV});
    Report << "PASS aligned tail storage, owned descriptor copy, Begin/End pairing\n";

    // UPLOAD 的 native state 不得离开 GENERIC_READ；DEFAULT buffer 提交后会 decay。
    auto Upload = RHICreateBuffer({64, 4, EBufferUsageFlags::Dynamic | EBufferUsageFlags::VertexBuffer});
    auto Default = RHICreateBuffer({64, 4, EBufferUsageFlags::Static | EBufferUsageFlags::VertexBuffer});
    Cmd.Transition({Upload.get(), ERHIAccess::Unknown, ERHIAccess::VertexOrIndexBuffer});
    Reject([&] { Cmd.Transition({Upload.get(), ERHIAccess::Unknown, ERHIAccess::CopyDest}); });
    Cmd.Transition({Default.get(), ERHIAccess::Unknown, ERHIAccess::CopyDest});
    Cmd.Transition({Default.get(), ERHIAccess::CopyDest, ERHIAccess::VertexOrIndexBuffer});

    FRHIRenderPassInfo Pass;
    Pass.ColorRenderTargets[0].RenderTarget = A.get();
    Pass.ColorRenderTargets[0].Action = ERenderTargetActions::Clear_Store;
    Pass.DepthStencilRenderTarget.DepthStencilTarget = Depth;
    Pass.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::Clear_Store, ERenderTargetActions::DontLoad_DontStore);
    Cmd.BeginRenderPass(Pass, "StageBValidationClear");
    Reject([&] { Cmd.Transition(ToRead); });
    Cmd.EndRenderPass();
    Reject([&] { Cmd.CopyTexture(A.get(), B.get()); });
    const FRHITransitionInfo Copies[] = {
        {A.get(), ERHIAccess::RTV, ERHIAccess::CopySrc},
        {B.get(), ERHIAccess::RTV, ERHIAccess::CopyDest}
    };
    Cmd.Transition(Copies);
    Cmd.CopyTexture(A.get(), B.get());
    Cmd.Transition({B.get(), ERHIAccess::CopyDest, ERHIAccess::SRVGraphicsPixel});
    Reject([&] { Cmd.BeginRenderPass(Pass, "MissingRTVTransition"); });
    Cmd.EndFrame();
    Cmd.WaitForGPU();
    Check(static_cast<FD3D12Buffer*>(Default.get())->GetResource()->GetState() == D3D12_RESOURCE_STATE_COMMON, "Default buffer decay was not tracked");
    Cmd.BeginFrame();
    Cmd.Transition({Default.get(), ERHIAccess::Unknown, ERHIAccess::CopyDest});
    Cmd.Transition({Depth, ERHIAccess::DSVWrite, ERHIAccess::DSVRead});
    Cmd.Transition({Depth, ERHIAccess::DSVRead, ERHIAccess::DSVWrite});
    Cmd.EndFrame();
    Cmd.WaitForGPU();
    Report << "PASS upload restrictions, default buffer decay, copy command, depth transition, pass validation\n";
    const auto Copied = ReadbackTextureRGBA(B.get());
    for (size_t I = 0; I < Copied.size(); I += 4)
        Check(Copied[I] == 51 && Copied[I+1] == 102 && Copied[I+2] == 204 && Copied[I+3] == 255,
            "Texture copy content mismatch");
    Report << "PASS GPU copy readback: every RGBA pixel matches clear color\n";
    CheckDebugMessages();
}
