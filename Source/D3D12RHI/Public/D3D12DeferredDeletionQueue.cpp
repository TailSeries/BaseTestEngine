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
	// remove_if 压缩保留条目，erase 删除尾部；移除已完成条目的引用，最后一个引用消失时对象才析构。
	Pending.erase(
		std::remove_if(Pending.begin(), Pending.end(), [CompletedValue](const FEntry& E) {return E.FenceValue <= CompletedValue; }), Pending.end()
	);

}
