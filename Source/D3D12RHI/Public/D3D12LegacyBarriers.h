#pragma once
#include "RHITransition.h"
#include "D3D12RHIModule.h"
#include "D3D12RHIPrivate.h"
#include <vector>

class FD3D12CommandContext;
class FD3D12Resource;
class FD3D12Device;

// UE：D3D12LegacyBarriers.cpp 的同名数据；省略跨管线同步点和资源别名数据。
// vector 拥有描述副本，但描述中的资源指针仍是借用，不能代替 Fence 生命周期管理。
struct FD3D12LegacyBarriersTransitionData
{
    ERHIPipeline SrcPipelines, DstPipelines;
    ERHITransitionCreateFlags CreateFlags;
    std::vector<FRHITransitionInfo> TransitionInfos;
    // 单线程简化：Begin/End 只能在同一个 Context、同一帧配对，不可重复执行。
    mutable const FD3D12CommandContext* Context = nullptr;
    mutable bool bEnded = false;
    explicit FD3D12LegacyBarriersTransitionData(const FRHITransitionCreateInfo& Info);
};

// 先解析并验证，再录制：失败批次不会留下部分状态修改或部分 GPU 命令。
struct FD3D12TransitionPlan
{
    std::vector<D3D12_RESOURCE_BARRIER> Barriers;
    std::vector<std::pair<FD3D12Resource*, D3D12_RESOURCE_STATES>> FinalStates;
};
D3D12RHIMODULE FD3D12TransitionPlan BuildTransitionPlan(
    FD3D12Device* Device, std::span<const FRHITransitionInfo> Infos);
