#include "RHITransition.h"
#include "DynamicRHI.h"
#include <algorithm>
#include <new>

uint64 GRHITransitionPrivateData_SizeInBytes = 0;
uint64 GRHITransitionPrivateData_AlignInBytes = 0;

FRHITransition* RHICreateTransition(const FRHITransitionCreateInfo& CreateInfo)
{
    const size_t Size = static_cast<size_t>(GRHITransitionPrivateData_SizeInBytes);
    const size_t Alignment = static_cast<size_t>(GRHITransitionPrivateData_AlignInBytes);
    if (!GDynamicRHI || !Size || !Alignment || (Alignment & (Alignment - 1)))
        throw std::logic_error("RHI transition storage is not initialized");
    const size_t Offset = (sizeof(FRHITransition) + Alignment - 1) & ~(Alignment - 1);
    if (Size > (std::numeric_limits<size_t>::max)() - Offset)
        throw std::overflow_error("Transition allocation overflow");
    const size_t AllocationAlignment = (std::max)(Alignment, alignof(FRHITransition));
    void* Storage = ::operator new(Offset + Size, std::align_val_t(AllocationAlignment));
    auto* Transition = new (Storage) FRHITransition();
    Transition->Owner = GDynamicRHI;
    Transition->PrivateOffset = Offset;
    Transition->PrivateSize = Size;
    Transition->PrivateAlignment = Alignment;
    Transition->AllocationAlignment = AllocationAlignment;
    try
    {
        // placement new 后端私有对象；失败时其构造器负责销毁已构造的成员。
        GDynamicRHI->RHICreateTransition(Transition, CreateInfo);
    }
    catch (...)
    {
        Transition->~FRHITransition();
        ::operator delete(Storage, std::align_val_t(AllocationAlignment));
        throw;
    }
    return Transition;
}

void RHIReleaseTransition(FRHITransition* Transition) noexcept
{
    if (!Transition) return;
    const size_t Alignment = Transition->AllocationAlignment;
    Transition->Owner->RHIReleaseTransition(Transition);
    Transition->~FRHITransition();
    ::operator delete(Transition, std::align_val_t(Alignment));
}
