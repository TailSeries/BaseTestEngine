#pragma once
#include "D3D12RHIModule.h"
#include "RHIResources.h" // FRHIResource  TRefCountPtr

// UE: FRHIResource::MarkForDelete + DeleteResources（侵入式引用计数 + 待删队列）
// 精简：不改侵入式，用队列持一份 shared_ptr 拷贝保活，直到 GPU 越过 fence 才释放

class D3D12RHIMODULE FD3D12DeferredDeletionQueue
{
public:
	// 把资源交给队列保活（refcount +1），记下"要等到的 fence 值"
	void Enqueue(TRefCountPtr<FRHIResource> Resource, uint64 FenceValue);

	// GPU 已越过 CompletedValue 的条目：丢弃（shared_ptr 释放 → 真 free）
	void ReleaseCompleted(uint64 CompletedValue);

private:
	struct FEntry
	{
		TRefCountPtr<FRHIResource> Resource; // 跟当前这个fence强绑定的资源
		uint64 FenceValue = 0;
	};
	std::vector<FEntry> Pending;
};
