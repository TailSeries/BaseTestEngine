#include "RenderResource.h"
#include "DynamicRHI.h"
#include "RHICommandList.h"
#include "Logging/Log.h"
#include <stdexcept>

FRenderResource::~FRenderResource()
{
    // 基类析构期间不能再通过虚调用执行派生类 ReleaseRHI。
    // 所以派生类/拥有者必须在派生对象仍完整时先调用 ReleaseResource。
    if (State != EState::Uninitialized)
        CORE_LOG(LogTemp, Error, "FRenderResource destroyed before ReleaseResource");
}

void FRenderResource::InitResource(FRHICommandListBase& RHICmdList)
{
    if (State == EState::Initialized) return;
    if (State != EState::Uninitialized) throw std::logic_error("Reentrant render resource initialization");
    // 与 UE 不同，本节不支持在 RHI 创建前注册资源、之后统一补做 InitRHI。
    if (!GDynamicRHI) throw std::logic_error("RHIInit must precede InitResource");
    State = EState::Initializing;
    try
    {
        InitRHI(RHICmdList);
        State = EState::Initialized;
    }
    catch (...)
    {
        State = EState::Releasing;
        ReleaseRHI();
        State = EState::Uninitialized;
        throw;
    }
}

void FRenderResource::ReleaseResource()
{
    if (State == EState::Uninitialized) return;
    if (State != EState::Initialized) throw std::logic_error("Reentrant render resource release");
    State = EState::Releasing;
    ReleaseRHI();
    State = EState::Uninitialized;
}

void BeginInitResource(FRenderResource* Resource)
{
    if (!Resource) throw std::invalid_argument("Null render resource");
    if (!GDynamicRHI) throw std::logic_error("RHIInit must precede BeginInitResource");
    Resource->InitResource(FRHICommandListExecutor::GetImmediateCommandList());
}

void BeginReleaseResource(FRenderResource* Resource)
{
    if (!Resource) throw std::invalid_argument("Null render resource");
    Resource->ReleaseResource();
}

// 这两个基类只持有一份 Buffer 引用；派生类若新增其他 RHI 资源，须在自己的析构器先释放。
FVertexBuffer::~FVertexBuffer() { ReleaseResource(); }
void FVertexBuffer::ReleaseRHI() noexcept { VertexBufferRHI.reset(); }
const TRefCountPtr<FRHIBuffer>& FVertexBuffer::GetRHI() const
{
    if (!IsInitialized() || !VertexBufferRHI) throw std::logic_error("Vertex buffer is not initialized");
    return VertexBufferRHI;
}
FIndexBuffer::~FIndexBuffer() { ReleaseResource(); }
void FIndexBuffer::ReleaseRHI() noexcept { IndexBufferRHI.reset(); }
const TRefCountPtr<FRHIBuffer>& FIndexBuffer::GetRHI() const
{
    if (!IsInitialized() || !IndexBufferRHI) throw std::logic_error("Index buffer is not initialized");
    return IndexBufferRHI;
}
