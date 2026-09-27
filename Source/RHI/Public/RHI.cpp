#include "DynamicRHI.h"
#include "RHICommandList.h"
#include <stdexcept>

FDynamicRHI* GDynamicRHI = nullptr;
namespace
{
    std::unique_ptr<FDynamicRHI> BackendOwner;
    std::unique_ptr<FRHICommandListImmediate> ImmediateCommandList;
}

void RHIInit(std::unique_ptr<FDynamicRHI> Backend)
{
    if (!Backend || GDynamicRHI) throw std::logic_error("Invalid or repeated RHIInit");
    Backend->Init();
    auto List = std::make_unique<FRHICommandListImmediate>(Backend->RHIGetDefaultContext());
    BackendOwner = std::move(Backend);
    GDynamicRHI = BackendOwner.get();
    ImmediateCommandList = std::move(List);
}

void RHIExit()
{
    if (!GDynamicRHI) return;
    ImmediateCommandList->WaitForGPU();
    ImmediateCommandList.reset();
    GDynamicRHI->Shutdown();
    GDynamicRHI = nullptr;
    BackendOwner.reset();
}

FRHICommandListImmediate& FRHICommandListExecutor::GetImmediateCommandList()
{
    if (!ImmediateCommandList) throw std::logic_error("RHIInit must precede command list access");
    return *ImmediateCommandList;
}
