#pragma once
/*
 * UE 结构:FD3D12CommandAllocator(Device*, QueueType) 包 ID3D12CommandAllocator;FD3D12CommandList 包 ID3D12GraphicsCommandList(多版本,我们只留基础版)。
 */
#include "D3D12RHIModule.h"
#include "D3D12RHIPrivate.h"
#include "D3D12Queue.h" // ED3D12QueueType / GetD3DCommandListType
class FD3D12Device;

// UE: class FD3D12CommandAllocator（D3D12CommandList.h）
class D3D12RHIMODULE FD3D12CommandAllocator
{
public:
	FD3D12CommandAllocator(FD3D12Device* InDevice, ED3D12QueueType QueueType);
	~FD3D12CommandAllocator();

	ID3D12CommandAllocator* GetAllocator() const { return Allocator.Get(); }
	void Reset();// 回收内存复用（GPU必须已经跑完命令）
private:
	ComPtr<ID3D12CommandAllocator> Allocator;
};

// UE: class FD3D12CommandList（D3D12CommandList.h，GraphicsCommandList1..10 多版本）
class D3D12RHIMODULE FD3D12CommandList
{
public:
	FD3D12CommandList(FD3D12Device* InDevice, FD3D12CommandAllocator* InAllocator, ED3D12QueueType QueueType);
	~FD3D12CommandList();
	ID3D12GraphicsCommandList* GetCommandList() const { return CommandList.Get(); }
	void Reset(FD3D12CommandAllocator* InAllocator);
	void Close();
private:
	ComPtr<ID3D12GraphicsCommandList> CommandList;
};


