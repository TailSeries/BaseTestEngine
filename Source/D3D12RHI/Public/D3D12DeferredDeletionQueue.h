#pragma once
#include "D3D12RHIModule.h"
#include "RHIResources.h" // FRHIResource  TRefCountPtr

// UE: FRHIResource::MarkForDelete + DeleteResources（侵入式引用计数 + 待删队列）
// 精简：队列持有 TRefCountPtr 保活，直到 GPU 完成对应 Fence 才移除引用；最后一个引用消失时对象才析构。

class D3D12RHIMODULE FD3D12DeferredDeletionQueue
{
public:
	// 把资源交给队列保活（refcount +1），记下"要等到的 fence 值"
	void Enqueue(TRefCountPtr<FRHIResource> Resource, uint64 FenceValue);

	// 移除 FenceValue <= CompletedValue 的条目；其他持有者仍可继续保活资源。
	void ReleaseCompleted(uint64 CompletedValue);

private:
	struct FEntry
	{
		TRefCountPtr<FRHIResource> Resource; // 跟当前这个fence强绑定的资源
		uint64 FenceValue = 0;
	};
	std::vector<FEntry> Pending;
};
