#pragma once
#include "RHIResources.h"

class FRHICommandListBase;

// 对照 UE RenderResource.h 的生命周期入口。当前单线程同步执行，不建立渲染线程任务队列。
// 这是拥有 RHI 引用的上层资源，不继承 FRHIResource，也不是 GPU Fence 或延迟删除队列。
class FRenderResource
{
public:
    FRenderResource() = default;
    FRenderResource(const FRenderResource&) = delete;
    FRenderResource& operator=(const FRenderResource&) = delete;
    FRenderResource(FRenderResource&&) = delete;
    FRenderResource& operator=(FRenderResource&&) = delete;
    virtual ~FRenderResource();

    // 必须在 RHIInit 后调用；重复初始化不重复创建资源。
    // InitRHI 抛错时调用 ReleaseRHI 清理部分结果，恢复未初始化状态，可以重试。
    void InitResource(FRHICommandListBase& RHICmdList);
    // 只释放本对象拥有的 RHI 引用，不销毁 C++ 对象、不等待 GPU、不自动 DeferredDelete。
    // 调用方必须先完成 GPU 同步，或采用与最后一次 GPU 使用匹配的延迟释放方案。
    void ReleaseResource();
    bool IsInitialized() const { return State == EState::Initialized; }

protected:
    // 派生类仅实现具体资源的创建/释放；外部统一走 InitResource/ReleaseResource。
    virtual void InitRHI(FRHICommandListBase& RHICmdList) {}
    // 教学实现要求释放不抛异常，并能处理初始化过程中只创建了一部分资源的情况。
    virtual void ReleaseRHI() noexcept {}

private:
    enum class EState { Uninitialized, Initializing, Initialized, Releasing };
    EState State = EState::Uninitialized;
};

// UE 的 Begin* 会安排渲染线程工作；本 Demo 直接执行，函数返回即完成 CPU 侧处理。
// 参数仅在本次调用中借用；未来改为异步时必须重新处理对象存活与完成同步。
void BeginInitResource(FRenderResource* Resource);
void BeginReleaseResource(FRenderResource* Resource);

// UE 的上层 Buffer 资源基类。具体 CPU 数据/上传方式由派生类实现 InitRHI。
class FVertexBuffer : public FRenderResource
{
public:
    ~FVertexBuffer() override;
    const TRefCountPtr<FRHIBuffer>& GetRHI() const;
protected:
    void InitRHI(FRHICommandListBase& RHICmdList) override = 0;
    void ReleaseRHI() noexcept override;
    TRefCountPtr<FRHIBuffer> VertexBufferRHI;
};

class FIndexBuffer : public FRenderResource
{
public:
    ~FIndexBuffer() override;
    const TRefCountPtr<FRHIBuffer>& GetRHI() const;
protected:
    void InitRHI(FRHICommandListBase& RHICmdList) override = 0;
    void ReleaseRHI() noexcept override;
    TRefCountPtr<FRHIBuffer> IndexBufferRHI;
};
