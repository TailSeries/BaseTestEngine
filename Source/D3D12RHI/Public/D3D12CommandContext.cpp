#include "D3D12CommandContext.h"
#include "D3D12Device.h"
#include "D3D12Descriptors.h"
#include "D3D12CommandList.h"
#include "D3D12Resources.h"// FD3D12Buffer / FD3D12Texture
#include "D3D12PipelineState.h"
#include "RHIResources.h"// FRHIBufferDesc
#include "RHIDefinitions.h"   // EBufferUsageFlags
#include "D3D12RootSignature.h"
#include <cstring>
#include <stdexcept>
#include "D3D12View.h"
#include "D3D12LegacyBarriers.h"
#include <algorithm>
FD3D12CommandContext::FD3D12CommandContext() = default;

FD3D12CommandContext::~FD3D12CommandContext() = default;

void FD3D12CommandContext::Init(FD3D12Device* InDevice)
{
    if (!InDevice || Device) throw std::logic_error("Invalid or repeated Context initialization");
    Device = InDevice;
    Queue = &Device->GetQueue(ED3D12QueueType::Direct);
	SRVHeap = Device->GetResourceDescriptorHeap();
	for (uint32 i = 0; i < FrameCount; i++)
	{
		// 每帧一个命令分配器
		CmdAllocs[i] = std::make_unique<FD3D12CommandAllocator>(Device, ED3D12QueueType::Direct);

		// 每帧一块 upload 常量 arena；每次绑定分配独立的 256 字节对齐区域。
		FRHIBufferDesc CBDesc(64 * 1024, 0, EBufferUsageFlags::ConstantBuffer | EBufferUsageFlags::Dynamic);
		CBs[i] = Device->CreateBuffer(CBDesc, nullptr);
	}
	CmdList = std::make_unique<FD3D12CommandList>(Device, CmdAllocs[0].get(), ED3D12QueueType::Direct);
}

void FD3D12CommandContext::BeginFrame()
{
    if (!Device || bFrameOpen) throw std::logic_error("Invalid BeginFrame");
	if (bInsideRenderPass)
	{
		throw std::logic_error(
			"End the active render pass before changing frames");
	}
	Slot = FrameIndex % FrameCount;
	Queue->WaitCPU(FrameFenceValue[Slot]);// 复用前等这个 slot 上一轮 GPU 活干完（稳态秒过）

	// M4-b：释放 GPU 已越过 fence 的延迟删除资源
	DeletionQueue.ReleaseCompleted(Queue->Fence.D3DFence->GetCompletedValue());

	CmdAllocs[Slot]->Reset();
	CmdList->Reset(CmdAllocs[Slot].get());
	CurrentRootSignature = nullptr;
	CurrentGraphicsPipelineState = nullptr;
    ConstantOffset = 0;
    bFrameOpen = true;
    SubmittedBuffers.clear();
}

void FD3D12CommandContext::BeginRenderPass(const FRHIRenderPassInfo& Info, const char* Name)
{
    RequireFrame();
	(void)Name; // 后续接入 GPU 调试事件。
	if (bInsideRenderPass)
	{
		throw std::logic_error("A render pass is already active");
	}
	const auto& Color = Info.ColorRenderTargets[0];
	const auto& Depth = Info.DepthStencilRenderTarget;
	FD3D12Texture* ColorTexture = static_cast<FD3D12Texture*>(Color.RenderTarget);
	if (!Color.RenderTarget ||
		Color.ResolveTarget ||
		Color.MipIndex != 0 ||
		Color.ArraySlice != -1)
	{
		throw std::invalid_argument(
			"Current pass requires a color texture, mip 0");
	}

    FD3D12RenderTargetView* ColorView = ColorTexture->GetRenderTargetView();
    if (ColorTexture->GetParentDevice() != Device || !ColorView || !ColorView->IsInitialized())
        throw std::invalid_argument("Color attachment requires an initialized RTV on this device");

	for (uint32 Index = 1; Index < Info.ColorRenderTargets.size(); ++Index)
	{
		const auto& Extra = Info.ColorRenderTargets[Index];

		if (Extra.RenderTarget || Extra.ResolveTarget ||
			Extra.Action != ERenderTargetActions::DontLoad_DontStore)
		{
			throw std::invalid_argument(
				"Only one color attachment is supported");
		}
	}

	if (!Depth.DepthStencilTarget ||
		Depth.ResolveTarget)
	{
		throw std::invalid_argument(
			"Current pass requires a depth texture without resolve");
	}

    auto* DepthTexture = static_cast<FD3D12Texture*>(Depth.DepthStencilTarget);
    FD3D12DepthStencilView* DepthView = DepthTexture->GetDepthStencilView();
    if (DepthTexture->GetParentDevice() != Device || !DepthView || !DepthView->IsInitialized())
    {
        throw std::invalid_argument("Depth attachment requires a valid DSV on this device");
    }
    // 当前只提供可写 DSV；即使允许 DSVRead 状态转换，也不能用这个 View 作为只读 DSV。
    const FRHITextureDesc& ColorDesc = Color.RenderTarget->GetDesc();

	const FRHITextureDesc& DepthDesc =Depth.DepthStencilTarget->GetDesc();

	if (ColorDesc.Format != PF_R8G8B8A8_UNORM ||
		DepthDesc.Format != PF_D32_FLOAT ||
		ColorDesc.Width != DepthDesc.Width ||
		ColorDesc.Height != DepthDesc.Height)
	{
		throw std::invalid_argument(
			"Incompatible color and depth attachments");
	}

	const ERenderTargetActions DepthAction = GetDepthActions(Depth.Action);

	const ERenderTargetActions StencilAction =	GetStencilActions(Depth.Action);

	const auto IsSupportedAction = [](ERenderTargetActions Action)
		{
			return Action == ERenderTargetActions::Clear_Store ||
				Action == ERenderTargetActions::Load_Store;
		};

	if (!IsSupportedAction(Color.Action) ||
		!IsSupportedAction(DepthAction) ||
		StencilAction != ERenderTargetActions::DontLoad_DontStore)
	{
		throw std::invalid_argument(
			"Only Load/Store or Clear/Store is supported; D32 has no stencil");
	}

	const bool bClearColor = 	GetLoadAction(Color.Action) == ERenderTargetLoadAction::EClear;

	const bool bClearDepth = 	GetLoadAction(DepthAction) == ERenderTargetLoadAction::EClear;

	// 先验证清除值，再开始录制命令。
	std::array<float, 4> ClearColor{};
	float ClearDepth = 1.0f;
	uint32 ClearStencil = 0;

	if (bClearColor)
	{
		ClearColor = ColorDesc.ClearValue.GetClearColor();
	}

	if (bClearDepth)
	{
		DepthDesc.ClearValue.GetDepthStencil(
			ClearDepth, ClearStencil);
	}

	ID3D12GraphicsCommandList* CL = CmdList->GetCommandList();

    // RenderPass 只检查并使用附件；状态变化必须由外部 Transition 显式声明。
    if (ColorTexture->GetResource()->GetState() != D3D12_RESOURCE_STATE_RENDER_TARGET ||
        DepthTexture->GetResource()->GetState() != D3D12_RESOURCE_STATE_DEPTH_WRITE)
        throw std::logic_error("Transition color/depth attachments before BeginRenderPass");

	const D3D12_CPU_DESCRIPTOR_HANDLE RTV =
		ColorView->GetCPUHandle();

	const D3D12_CPU_DESCRIPTOR_HANDLE DSV =
		DepthView->GetCPUHandle();

	// 注意这里这个接口，同时绑定了颜色附件和深度附件
	CL->OMSetRenderTargets(1, &RTV, false, &DSV);

	if (bClearColor)
	{
		CL->ClearRenderTargetView(
			RTV, ClearColor.data(), 0, nullptr);
	}

	if (bClearDepth)
	{
		CL->ClearDepthStencilView(
			DSV,
			D3D12_CLEAR_FLAG_DEPTH,
			ClearDepth,
			0,
			0,
			nullptr);
	}

	D3D12_VIEWPORT VP{
		0.0f,
		0.0f,
		static_cast<float>(ColorDesc.Width),
		static_cast<float>(ColorDesc.Height),
		0.0f,
		1.0f
	};

	D3D12_RECT Scissor{
		0,
		0,
		static_cast<LONG>(ColorDesc.Width),
		static_cast<LONG>(ColorDesc.Height)
	};

	CL->RSSetViewports(1, &VP);
	CL->RSSetScissorRects(1, &Scissor);

	CurrentColorTarget = ColorTexture;
	bInsideRenderPass = true;
}

void FD3D12CommandContext::SetGraphicsPipelineState(FRHIGraphicsPipelineState* PSO)
{
    RequireFrame();
	if (!PSO)
	{
		throw std::invalid_argument("Graphics PSO is null");
	}
	FD3D12GraphicsPipelineState* D3DPSO = static_cast<FD3D12GraphicsPipelineState*>(PSO);
	CurrentGraphicsPipelineState = D3DPSO;
	CurrentRootSignature = D3DPSO->RootSignature;


	ID3D12GraphicsCommandList* CL = CmdList->GetCommandList();

	// UE：PSO 打包根签名，一起设
	CL->SetGraphicsRootSignature(CurrentRootSignature->GetRootSignature());

	// 当前每次设置 PSO 时绑定资源堆；必须先于描述符表绑定，后续由 StateCache 去重。
	ID3D12DescriptorHeap* Heaps[] = {SRVHeap->GetHeap()};
	CL->SetDescriptorHeaps(1, Heaps);
	CL->SetPipelineState(D3DPSO->PipelineState->GetPipelineState());
}

void FD3D12CommandContext::SetShaderConstants(uint32 BufferIndex, const void* Data, uint32 Size)
{
    RequireFrame();
	if (!CurrentRootSignature)
	{
		throw std::logic_error("Set graphics PSO before shader constants");
	}

	if (BufferIndex != 0)
	{
		throw std::invalid_argument("Only VS b0 is supported");
	}

	const int32 RootSlot = CurrentRootSignature->GetRootParameterSlot(VS_RootCBVs);
	if (RootSlot < 0)
	{
		throw std::logic_error("Current root signature has no VS b0");
	}
	// 简化：root CBV + 每帧常量 arena；每次绑定使用独立区域，避免后一个 draw 覆写前一个 draw。
	FD3D12Buffer* CB = CBs[Slot].get();

	if (!Data || Size == 0 || Size > CB->GetSize() - ConstantOffset)
	{
		throw std::invalid_argument("Invalid shader constants");
	}

	const uint32 AlignedSize = (Size + 255u) & ~255u;
    if (AlignedSize > CB->GetSize() - ConstantOffset)
        throw std::overflow_error("Frame constant arena exhausted");
    memcpy(static_cast<uint8*>(CB->GetMappedData()) + ConstantOffset, Data, Size);
	ID3D12GraphicsCommandList* CL = CmdList->GetCommandList();
	CL->SetGraphicsRootConstantBufferView(static_cast<UINT>(RootSlot), CB->GetResource()->GetGPUVirtualAddress() + ConstantOffset);
    ConstantOffset += AlignedSize;
}

void FD3D12CommandContext::SetShaderResourceViewParameter(uint32 ResourceIndex,FRHIShaderResourceView* View)
{
    RequireFrame();
	if (!CurrentRootSignature)
	{
		throw std::logic_error("Set graphics PSO before SRV");
	}

	if (ResourceIndex != 0 || !View)
	{
		throw std::invalid_argument(
			"Only a valid PS t0 shader resource view is supported");
	}

	const int32 RootSlot = CurrentRootSignature->GetRootParameterSlot(PS_SRVs);
	if (RootSlot < 0)
	{
		throw std::logic_error(
			"Current root signature has no PS SRV table");
	}

	FD3D12ShaderResourceView_RHI* D3DView = static_cast<FD3D12ShaderResourceView_RHI*>(View);

    auto* Texture = static_cast<FD3D12Texture*>(View->GetResource());
    if (Texture->GetParentDevice() != Device ||
        !(Texture->GetResource()->GetState() & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))
        throw std::logic_error("Transition texture to pixel SRV before binding");
    ID3D12GraphicsCommandList* CL = CmdList->GetCommandList();
    CL->SetGraphicsRootDescriptorTable(static_cast<UINT>(RootSlot),D3DView->GetGPUHandle());
}

void FD3D12CommandContext::SetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBuffer, uint32 Offset)
{
    RequireFrame();
	if (!CurrentGraphicsPipelineState)
	{
		throw std::logic_error(
			"Set graphics PSO before vertex streams");
	}

	const auto& StreamStrides = CurrentGraphicsPipelineState->StreamStrides;
	if (StreamIndex >= StreamStrides.size())
	{
		throw std::out_of_range("Vertex stream index is out of range");
	}
	D3D12_VERTEX_BUFFER_VIEW VBV = {};
	if (VertexBuffer)
	{
        FD3D12Buffer* VB = static_cast<FD3D12Buffer*>(VertexBuffer);
        if (VB->GetParentDevice() != Device || !(VB->GetResource()->GetState() & D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER))
            throw std::logic_error("Vertex buffer requires vertex-readable state on this device");
		if (Offset > VB->GetSize())
		{
			throw std::out_of_range(
				"Vertex stream offset exceeds buffer size");
		}


		VBV.BufferLocation = VB->GetResource()->GetGPUVirtualAddress() + Offset;
		VBV.SizeInBytes = VB->GetSize() - Offset;
		// 步长由当前 PSO 的顶点声明决定。这里没有强行要求 Stride > 0，也没有要求它等于 VB->GetStride()。Buffer 自己的 Stride 已不再决定顶点输入布局。
		VBV.StrideInBytes = StreamStrides[StreamIndex] ;
	}

	// 空 Buffer 对应零初始化的 View，用于解除该槽的绑定
	ID3D12GraphicsCommandList* CL = CmdList->GetCommandList();
	CL->IASetVertexBuffers(StreamIndex, 1, &VBV);
}

void FD3D12CommandContext::DrawIndexedPrimitive(FRHIBuffer* IndexBuffer, uint32 IndexCount)
{
    RequireFrame();
    if (!bInsideRenderPass || !CurrentGraphicsPipelineState || !IndexBuffer)
        throw std::logic_error("Indexed draw requires a pass, PSO and index buffer");
    if ((IndexBuffer->GetStride() != 2 && IndexBuffer->GetStride() != 4) ||
        IndexCount > IndexBuffer->GetSize() / IndexBuffer->GetStride())
        throw std::invalid_argument("Invalid index stride or draw range");
    FD3D12Buffer* IB = static_cast<FD3D12Buffer*>(IndexBuffer);
    if (IB->GetParentDevice() != Device || !(IB->GetResource()->GetState() & D3D12_RESOURCE_STATE_INDEX_BUFFER))
        throw std::logic_error("Index buffer requires index-readable state on this device");
	D3D12_INDEX_BUFFER_VIEW IBV ={};
	IBV.BufferLocation = IB->GetResource()->GetGPUVirtualAddress();
	IBV.SizeInBytes = IB->GetSize();
	IBV.Format = (IB->GetStride() == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	ID3D12GraphicsCommandList* CL = CmdList->GetCommandList();
	CL->IASetIndexBuffer(&IBV);
	CL->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	CL->DrawIndexedInstanced(IndexCount, 1, 0, 0, 0);
}

void FD3D12CommandContext::EndRenderPass()
{
    RequireFrame();
	if (!bInsideRenderPass)
	{
		throw std::logic_error("No render pass is active");
	}

    // Pass 结束不推测后续用途：下一步可能是采样、复制、继续写入或 Present。
    // 解除原生附件绑定，随后由调用方声明下一次访问。
    CmdList->GetCommandList()->OMSetRenderTargets(0, nullptr, false, nullptr);
	bInsideRenderPass = false;
    CurrentColorTarget = nullptr;
}


void FD3D12CommandContext::EndFrame()
{
    RequireFrame();
	if (bInsideRenderPass)
	{
		throw std::logic_error(
			"End the active render pass before changing frames");
	}
    if (OpenTransitions != 0) throw std::logic_error("Unpaired Begin/EndTransitions at EndFrame");
    CmdList->Close();
	ID3D12CommandList* Lists[] = {CmdList->GetCommandList()};
	Queue->GetD3DQueue()->ExecuteCommandLists(1, Lists);
	FrameFenceValue[Slot] = Queue->Signal(Queue->Fence); // 只记一下对应slot帧需要等的fencevalue，不用等

	for (auto& Resource : PendingDeletes)
	{
		DeletionQueue.Enqueue(std::move(Resource), FrameFenceValue[Slot]);
	}
	PendingDeletes.clear();

    // D3D12 的 buffer 在 ExecuteCommandLists 批次完成后衰减为 COMMON。
    // CPU 这里预测下一批命令的起始状态；不表示 GPU 此刻已经执行完成。
    for (auto* Resource : SubmittedBuffers) Resource->SetState(D3D12_RESOURCE_STATE_COMMON);
    SubmittedBuffers.clear();
    FrameIndex++;
    bFrameOpen = false;
    CurrentGraphicsPipelineState = nullptr;
    CurrentRootSignature = nullptr;
}

void FD3D12CommandContext::WaitForGPU()
{
	// 未提交的命令不能通过等待队列 Fence 证明完成。
    if (bFrameOpen) throw std::logic_error("EndFrame before waiting for GPU");
	const uint64 CompletionValue = Queue->Signal(Queue->Fence);
	for (auto& Resource : PendingDeletes)
	{
		DeletionQueue.Enqueue(std::move(Resource), CompletionValue);
	}
	PendingDeletes.clear();

	Queue->WaitCPU(CompletionValue);
	DeletionQueue.ReleaseCompleted(
		Queue->Fence.D3DFence->GetCompletedValue());
}

void FD3D12CommandContext::DeferredDelete(TRefCountPtr<FRHIResource> Resource)
{
	// 此处只保留引用；EndFrame 用实际 Signal 返回值标记，GPU 完成后由 DeletionQueue 释放。
	if (Resource)
	{
		PendingDeletes.push_back(std::move(Resource));
	}
}









void FD3D12CommandContext::RequireFrame() const
{
    if (!bFrameOpen) throw std::logic_error("Command requires an active frame");
}

// Begin 在当前单管线路径只记录配对关系；不是 D3D12 BEGIN_ONLY 屏障。
// 跨队列 release/acquire、Fence 和真正 split barrier 是后续扩展。
void FD3D12CommandContext::RHIBeginTransitions(std::span<const FRHITransition* const> Transitions)
{
    RequireFrame();
    if (bInsideRenderPass) throw std::logic_error("Transitions must be outside render passes");
    for (size_t I = 0; I < Transitions.size(); ++I)
    {
        if (!Transitions[I]) throw std::invalid_argument("Null transition");
        const auto* Data = Transitions[I]->GetPrivateData<FD3D12LegacyBarriersTransitionData>();
        if (Data->Context || Data->bEnded ||
            std::find(Transitions.begin(), Transitions.begin() + I, Transitions[I]) != Transitions.begin() + I)
            throw std::logic_error("Transition cannot begin twice");
    }
    for (const auto* Transition : Transitions)
        Transition->GetPrivateData<FD3D12LegacyBarriersTransitionData>()->Context = this;
    OpenTransitions += static_cast<uint32>(Transitions.size());
}

void FD3D12CommandContext::RHIEndTransitions(std::span<const FRHITransition* const> Transitions)
{
    RequireFrame();
    if (bInsideRenderPass) throw std::logic_error("Transitions must be outside render passes");
    std::vector<FRHITransitionInfo> Infos;
    for (size_t I = 0; I < Transitions.size(); ++I)
    {
        if (!Transitions[I]) throw std::invalid_argument("Null transition");
        const auto* Data = Transitions[I]->GetPrivateData<FD3D12LegacyBarriersTransitionData>();
        if (Data->Context != this || Data->bEnded ||
            std::find(Transitions.begin(), Transitions.begin() + I, Transitions[I]) != Transitions.begin() + I)
            throw std::logic_error("EndTransitions requires one matching Begin on this context");
        Infos.insert(Infos.end(), Data->TransitionInfos.begin(), Data->TransitionInfos.end());
    }
    // 本批 End 消费 Begin；即使计划验证失败，也不会留下无法结束帧的配对计数。
    // 失败对象不可重试，调用方修正参数后创建新的 Transition。
    for (const auto* Transition : Transitions)
        Transition->GetPrivateData<FD3D12LegacyBarriersTransitionData>()->bEnded = true;
    OpenTransitions -= static_cast<uint32>(Transitions.size());
    auto Plan = BuildTransitionPlan(Device, Infos);
    // 一切可能分配内存的操作先完成，避免提交 GPU 命令后因分配失败漏记 CPU 状态。
    SubmittedBuffers.reserve(SubmittedBuffers.size() + Plan.FinalStates.size());
    for (const auto& [Resource, State] : Plan.FinalStates)
        if (Resource->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER &&
            Resource->GetHeapType() == D3D12_HEAP_TYPE_DEFAULT &&
            std::find(SubmittedBuffers.begin(), SubmittedBuffers.end(), Resource) == SubmittedBuffers.end())
            SubmittedBuffers.push_back(Resource);
    if (!Plan.Barriers.empty())
        CmdList->GetCommandList()->ResourceBarrier(static_cast<UINT>(Plan.Barriers.size()), Plan.Barriers.data());
    BarrierCount += Plan.Barriers.size();
    for (const auto& [Resource, State] : Plan.FinalStates) Resource->SetState(State);
}

void FD3D12CommandContext::RHICopyTexture(FRHITexture* Source, FRHITexture* Dest)
{
    RequireFrame();
    if (bInsideRenderPass || !Source || !Dest || Source == Dest)
        throw std::invalid_argument("CopyTexture requires two distinct textures outside a pass");
    auto* Src = static_cast<FD3D12Texture*>(Source);
    auto* Dst = static_cast<FD3D12Texture*>(Dest);
    if (Src->GetParentDevice() != Device || Dst->GetParentDevice() != Device ||
        Source->GetDesc().Width != Dest->GetDesc().Width || Source->GetDesc().Height != Dest->GetDesc().Height ||
        Source->GetDesc().Format != Dest->GetDesc().Format)
        throw std::invalid_argument("CopyTexture requires matching dimensions/format on this device");
    if (!(Src->GetResource()->GetState() & D3D12_RESOURCE_STATE_COPY_SOURCE) ||
        Dst->GetResource()->GetState() != D3D12_RESOURCE_STATE_COPY_DEST)
        throw std::logic_error("Transition textures to CopySrc/CopyDest before copying");
    CmdList->GetCommandList()->CopyResource(Dst->GetResource()->GetResource(), Src->GetResource()->GetResource());
}
