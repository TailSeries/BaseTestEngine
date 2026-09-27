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
	FD3D12Texture* BackBufferTexture = static_cast<FD3D12Texture*>(Color.RenderTarget);
	if (!Color.RenderTarget ||
		!BackBufferTexture->IsBackBuffer() ||
		Color.ResolveTarget ||
		Color.MipIndex != 0 ||
		Color.ArraySlice != -1)
	{
		throw std::invalid_argument(
			"Current pass requires a swap-chain backbuffer, mip 0");
	}

    FD3D12RenderTargetView* ColorView = BackBufferTexture->GetRenderTargetView();
    if (BackBufferTexture->GetParentDevice() != Device || !ColorView || !ColorView->IsInitialized())
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
    // 当前深度纹理创建为 DEPTH_WRITE，尚不支持运行中转换为其他用途。
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

	ID3D12Resource* BackBuffer = 	BackBufferTexture->GetResource()->GetResource();

	D3D12_RESOURCE_BARRIER Barrier{};
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	Barrier.Transition.pResource = BackBuffer;
	Barrier.Transition.Subresource =
		D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	CL->ResourceBarrier(1, &Barrier);

	const D3D12_CPU_DESCRIPTOR_HANDLE RTV =
		ColorView->GetCPUHandle();

	const D3D12_CPU_DESCRIPTOR_HANDLE DSV =
		DepthView->GetCPUHandle();

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

	CurrentColorTarget = BackBufferTexture;
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

	ID3D12GraphicsCommandList* CL = CmdList->GetCommandList();
	// 当前 backbuffer 专用流程：Pass 结束转回 PRESENT；不是任意附件的通用状态策略。
	D3D12_RESOURCE_BARRIER Barrier = {};
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	Barrier.Transition.pResource = CurrentColorTarget->GetResource()->GetResource();
	Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	CL->ResourceBarrier(1, &Barrier);

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
	CmdList->Close();
	ID3D12CommandList* Lists[] = {CmdList->GetCommandList()};
	Queue->GetD3DQueue()->ExecuteCommandLists(1, Lists);
	FrameFenceValue[Slot] = Queue->Signal(Queue->Fence); // 只记一下对应slot帧需要等的fencevalue，不用等

	for (auto& Resource : PendingDeletes)
	{
		DeletionQueue.Enqueue(std::move(Resource), FrameFenceValue[Slot]);
	}
	PendingDeletes.clear();

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
