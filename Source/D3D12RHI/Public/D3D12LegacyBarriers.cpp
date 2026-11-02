#include "D3D12LegacyBarriers.h"
#include "D3D12Resources.h"
#include "D3D12DynamicRHI.h"
#include <algorithm>
#include <new>

FD3D12LegacyBarriersTransitionData::FD3D12LegacyBarriersTransitionData(const FRHITransitionCreateInfo& Info)
    : SrcPipelines(Info.SrcPipelines), DstPipelines(Info.DstPipelines), CreateFlags(Info.Flags)
{
    if (SrcPipelines != ERHIPipeline::Graphics || DstPipelines != ERHIPipeline::Graphics ||
        CreateFlags != ERHITransitionCreateFlags::None)
        throw std::invalid_argument("Only default Graphics-to-Graphics transitions are supported");
    if (!Info.TransitionInfos.empty())
        TransitionInfos.assign(Info.TransitionInfos.begin(), Info.TransitionInfos.end());
}

void FD3D12DynamicRHI::RHICreateTransition(FRHITransition* Transition, const FRHITransitionCreateInfo& Info)
{
    new (Transition->GetPrivateData<FD3D12LegacyBarriersTransitionData>()) FD3D12LegacyBarriersTransitionData(Info);
}
void FD3D12DynamicRHI::RHIReleaseTransition(FRHITransition* Transition) noexcept
{
    Transition->GetPrivateData<FD3D12LegacyBarriersTransitionData>()->~FD3D12LegacyBarriersTransitionData();
}

namespace
{
    constexpr D3D12_RESOURCE_STATES ReadMask = static_cast<D3D12_RESOURCE_STATES>(
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
        D3D12_RESOURCE_STATE_COPY_SOURCE | D3D12_RESOURCE_STATE_DEPTH_READ |
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER |
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    bool IsRead(D3D12_RESOURCE_STATES State) { return State != 0 && (State & ~ReadMask) == 0; }

    D3D12_RESOURCE_STATES TranslateAccess(ERHIAccess Access, const FRHITransitionInfo& Info, FD3D12Resource* Resource)
    {
        // Unknown 只允许作为 AccessBefore；它不是 COMMON，也不是丢弃内容。
        constexpr ERHIAccess Supported = ERHIAccess::Present | ERHIAccess::VertexOrIndexBuffer |
            ERHIAccess::SRVMask | ERHIAccess::CopySrc | ERHIAccess::CopyDest |
            ERHIAccess::RTV | ERHIAccess::DSVRead | ERHIAccess::DSVWrite;
        if (Access == ERHIAccess::Unknown || EnumHasAnyFlags(Access, ~Supported))
            throw std::invalid_argument("Unknown destination or unsupported access (UAV is not implemented)");
        const uint32 Bits = static_cast<uint32>(Access);
        const ERHIAccess Exclusive = ERHIAccess::Present | ERHIAccess::RTV | ERHIAccess::DSVWrite | ERHIAccess::CopyDest;
        if (EnumHasAnyFlags(Access, Exclusive) && (Bits & (Bits - 1)))
            throw std::invalid_argument("Exclusive write/present access cannot be combined");
        const bool bTexture = Info.Type == FRHITransitionInfo::EType::Texture;
        const auto Flags = Resource->GetDesc().Flags;
        if (EnumHasAnyFlags(Access, ERHIAccess::Present) && (!bTexture || !static_cast<FD3D12Texture*>(Info.Texture)->IsBackBuffer()))
            throw std::invalid_argument("Present requires a swap-chain texture");
        if (EnumHasAnyFlags(Access, ERHIAccess::RTV) && (!bTexture || !(Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)))
            throw std::invalid_argument("RTV access requires a render-target texture");
        if (EnumHasAnyFlags(Access, ERHIAccess::DSVRead | ERHIAccess::DSVWrite) &&
            (!bTexture || !(Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)))
            throw std::invalid_argument("DSV access requires a depth texture");
        if (EnumHasAnyFlags(Access, ERHIAccess::SRVMask) &&
            (!bTexture || !EnumHasAnyFlags(Info.Texture->GetDesc().Flags, ETextureCreateFlags::ShaderResource) ||
                (Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)))
            throw std::invalid_argument("SRV access requires shader-resource capability (buffer SRV not implemented)");
        if (EnumHasAnyFlags(Access, ERHIAccess::VertexOrIndexBuffer) &&
            (bTexture || !EnumHasAnyFlags(Info.Buffer->GetUsage(), EBufferUsageFlags::VertexBuffer | EBufferUsageFlags::IndexBuffer)))
            throw std::invalid_argument("Vertex/index access requires matching buffer usage");

        D3D12_RESOURCE_STATES State = D3D12_RESOURCE_STATE_COMMON;
        if (EnumHasAnyFlags(Access, ERHIAccess::RTV)) State |= D3D12_RESOURCE_STATE_RENDER_TARGET;
        if (EnumHasAnyFlags(Access, ERHIAccess::DSVWrite)) State |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
        if (EnumHasAnyFlags(Access, ERHIAccess::DSVRead)) State |= D3D12_RESOURCE_STATE_DEPTH_READ;
        if (EnumHasAnyFlags(Access, ERHIAccess::SRVGraphicsPixel)) State |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        if (EnumHasAnyFlags(Access, ERHIAccess::SRVGraphicsNonPixel | ERHIAccess::SRVCompute)) State |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        if (EnumHasAnyFlags(Access, ERHIAccess::CopySrc)) State |= D3D12_RESOURCE_STATE_COPY_SOURCE;
        if (EnumHasAnyFlags(Access, ERHIAccess::CopyDest)) State |= D3D12_RESOURCE_STATE_COPY_DEST;
        if (EnumHasAnyFlags(Access, ERHIAccess::VertexOrIndexBuffer))
        {
            if (EnumHasAnyFlags(Info.Buffer->GetUsage(), EBufferUsageFlags::VertexBuffer)) State |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            if (EnumHasAnyFlags(Info.Buffer->GetUsage(), EBufferUsageFlags::IndexBuffer)) State |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
        }
        // D3D12 PRESENT 与 COMMON 的原生数值均为 0，但 RHI 访问语义仍然不同。
        return State;
    }
}

FD3D12TransitionPlan BuildTransitionPlan(FD3D12Device* Device, std::span<const FRHITransitionInfo> Infos)
{
    FD3D12TransitionPlan Plan;
    for (const auto& Info : Infos)
    {
        if (!Info.IsWholeResource() || Info.Flags != EResourceTransitionFlags::None)
            throw std::invalid_argument("Only whole-resource default transitions are implemented");
        FD3D12Resource* Resource = nullptr;
        switch (Info.Type)
        {
        case FRHITransitionInfo::EType::Texture:
            if (Info.Texture) Resource = static_cast<FD3D12Texture*>(Info.Texture)->GetResource();
            break;
        case FRHITransitionInfo::EType::Buffer:
            if (Info.Buffer) Resource = static_cast<FD3D12Buffer*>(Info.Buffer)->GetResource();
            break;
        default: break;
        }
        if (!Resource || Resource->GetParentDevice() != Device)
            throw std::invalid_argument("Transition requires a valid resource from this device");

        // 同一批次允许同资源多次转换；查的是本批次的预测状态，而非旧的全局记录。
        auto It = std::find_if(Plan.FinalStates.begin(), Plan.FinalStates.end(),
            [Resource](const auto& Pair) { return Pair.first == Resource; });
        const auto Before = It == Plan.FinalStates.end() ? Resource->GetState() : It->second;
        if (Info.AccessBefore != ERHIAccess::Unknown)
        {
            const auto Expected = TranslateAccess(Info.AccessBefore, Info, Resource);
            if (Before != Expected && !(IsRead(Before) && IsRead(Expected) && (Before & Expected) == Expected))
                throw std::logic_error("AccessBefore does not match tracked resource state");
        }
        auto After = TranslateAccess(Info.AccessAfter, Info, Resource);
        if (Resource->GetHeapType() == D3D12_HEAP_TYPE_UPLOAD)
        {
            // UPLOAD 永远保持 GENERIC_READ；声明其允许的读用途不会发出状态转换。
            if (!IsRead(After) || (After & D3D12_RESOURCE_STATE_GENERIC_READ) != After)
                throw std::invalid_argument("UPLOAD buffers cannot transition to writable states");
            After = Before;
        }
        else if (IsRead(Before) && IsRead(After))
        {
            // 只读状态可以合并；扩大读集合需要屏障，缩小到已有子集则无需屏障。
            After |= Before;
        }
        if (It == Plan.FinalStates.end()) Plan.FinalStates.emplace_back(Resource, After);
        else It->second = After;
    }
    // 一个 Transition 批次内部没有 Draw/Copy。对重复资源先验证整个访问链，
    // 最后只发出初态 -> 终态，避免同一 ResourceBarrier 调用中重复屏障的调试警告。
    for (const auto& [Resource, After] : Plan.FinalStates)
    {
        const auto Before = Resource->GetState();
        if (Before == After) continue;
        D3D12_RESOURCE_BARRIER Barrier{};
        Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        Barrier.Transition = {Resource->GetResource(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, Before, After};
        Plan.Barriers.push_back(Barrier);
    }
    return Plan;
}
