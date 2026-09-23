#include "D3D12DeferredDeletionQueue.h"
#include <algorithm>
#include <utility>


void FD3D12DeferredDeletionQueue::Enqueue(TRefCountPtr<FRHIResource> Resource, uint64 FenceValue)
{
	if (!Resource)
	{
		return;
	}
	Pending.push_back({ std::move(Resource), FenceValue });
}


void FD3D12DeferredDeletionQueue::ReleaseCompleted(uint64 CompletedValue)
{
	// 把"GPU 已越过 fence"的条目移到尾部再一次性 erase——它们的 shared_ptr 析构 = 真 free
	Pending.erase(
		std::remove_if(Pending.begin(), Pending.end(), [CompletedValue](const FEntry& E) {return E.FenceValue <= CompletedValue; }), Pending.end()
	);

}
