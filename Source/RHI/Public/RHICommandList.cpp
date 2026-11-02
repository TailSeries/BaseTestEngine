#include "RHICommandList.h"
#include "DynamicRHI.h"

void FRHIComputeCommandList::Transition(std::span<const FRHITransitionInfo> Infos)
{
    if (Infos.empty()) return;
    const FRHITransitionCreateInfo CreateInfo(ERHIPipeline::Graphics, ERHIPipeline::Graphics,
        ERHITransitionCreateFlags::None, Infos);
    // 对照 UE TransitionInternal 的 Bypass 手动释放路径；这里用堆分配 + RAII 代替 MemStack。
    std::unique_ptr<FRHITransition, decltype(&RHIReleaseTransition)> Object(
        RHICreateTransition(CreateInfo), &RHIReleaseTransition);
    const FRHITransition* Pointer = Object.get();
    const std::span<const FRHITransition* const> Batch(&Pointer, 1);
    Context->RHIBeginTransitions(Batch);
    Context->RHIEndTransitions(Batch);
}

void FRHICommandListImmediate::EndDrawingViewport(
    FRHIViewport* Viewport, const FRHIPresentArgs& PresentArgs)
{
    GDynamicRHI->RHIEndDrawingViewport(*this, Viewport, PresentArgs);
}
