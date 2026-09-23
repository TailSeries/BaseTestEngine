#include "D3D12CommandContext.h"
#include "D3D12Device.h"
#include "D3D12Viewport.h"
#include "D3D12Descriptors.h"
#include "D3D12CommandList.h"
#include "D3D12Resources.h"// FD3D12Buffer / FD3D12Texture
#include "D3D12PipelineState.h"
#include "RHIResources.h"// FRHIBufferDesc
#include "RHIDefinitions.h"   // EBufferUsageFlags
#include "D3D12RootSignature.h"

FD3D12CommandContext::FD3D12CommandContext() = default;

FD3D12CommandContext::~FD3D12CommandContext() = default;

void FD3D12CommandContext::Init(FD3D12Device* InDevice, FD3D12Queue* InQueue, FD3D12Viewport* InViewPort, FD3D12DescriptorHeap* InDSVHeap, FD3D12DescriptorHeap* InSRVHeap)
{
	Device = InDevice;
	Queue = InQueue;
	Viewport = InViewPort;
	DSVHeap = InDSVHeap;
	SRVHeap = InSRVHeap;
	for (uint32 i = 0; i < FrameCount; i++)
	{
		// 每帧一个命令分配器
		CmdAllocs[i] = std::make_unique<FD3D12CommandAllocator>(Device, ED3D12QueueType::Direct);

		// CB ring：每帧一个常量缓冲（256 会在 CreateBuffer 里对齐）,同时我们要求创建在upload堆里
		FRHIBufferDesc CBDesc(256, 0, EBufferUsageFlags::ConstantBuffer | EBufferUsageFlags::Dynamic);
		CBs[i] = Device->CreateBuffer(CBDesc, nullptr);
	}
	CmdList = std::make_unique<FD3D12CommandList>(Device, CmdAllocs[0].get(), ED3D12QueueType::Direct);
}

void FD3D12CommandContext::BeginFrame()
{
	Slot = FrameIndex % FrameCount;
	Queue->WaitCPU(FrameFenceValue[Slot]);// 复用前等这个 slot 上一轮 GPU 活干完（稳态秒过）

	// M4-b：释放 GPU 已越过 fence 的延迟删除资源
	DeletionQueue.ReleaseCompleted(Queue->Fence.D3DFence->GetCompletedValue());

	CmdAllocs[Slot]->Reset();
	CmdList->Reset(CmdAllocs[Slot].get());
}

void FD3D12CommandContext::BeginRenderPass(const float ClearColor[4])
{
	ID3D12GraphicsCommandList* CL = CmdList->GetCommandList();
	ID3D12Resource* BackBuffer = Viewport->GetBackBuffer(); // 拿的是swapchain当前的backbufer
	D3D12_CPU_DESCRIPTOR_HANDLE RTV = Viewport->GetCurrentBackBufferRTV();// 拿上面那个backbuffer对应的RTV
	D3D12_CPU_DESCRIPTOR_HANDLE DSV = DSVHeap->GetCPUHandle(0); // dsv只需要一份，我们直接跨帧复用就行

	//barrier PRESENT >> RENDER_TARGET
	D3D12_RESOURCE_BARRIER Barrier = {};
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	Barrier.Transition.pResource = BackBuffer;
	Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	CL->ResourceBarrier(1, &Barrier);

	CL->OMSetRenderTargets(1, &RTV, false, &DSV);
	CL->ClearRenderTargetView(RTV, ClearColor, 0, nullptr);
	CL->ClearDepthStencilView(DSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// 视口/裁剪：从 backbuffer 尺寸推，免得再传参
	D3D12_RESOURCE_DESC BBDesc = BackBuffer->GetDesc();
	D3D12_VIEWPORT VP = { 0.0f, 0.0f, (float)BBDesc.Width, (float)BBDesc.Height, 0.0f, 1.0f };
	D3D12_RECT Scissor = {0,0,(LONG)BBDesc.Width, (LONG)BBDesc.Height};
	CL->RSSetViewports(1, &VP);
	CL->RSSetScissorRects(1, &Scissor);
}

void FD3D12CommandContext::SetGraphicsPipelineState(FRHIGraphicsPipelineState* PSO)
{
	ID3D12GraphicsCommandList* CL = CmdList->GetCommandList();
	FD3D12PipelineState* D3DPSO = static_cast<FD3D12PipelineState*>(PSO);
	// UE：PSO 打包根签名，一起设
	CL->SetGraphicsRootSignature(D3DPSO->GetRootSignature()->GetRootSignature());

	// 描述符堆：每帧 Reset 后设一次，必须在 SetGraphicsRootDescriptorTable 之前
	ID3D12DescriptorHeap* Heaps[] = {SRVHeap->GetHeap()};
	CL->SetDescriptorHeaps(1, Heaps);
	CL->SetPipelineState(D3DPSO->GetPipelineState());
}

void FD3D12CommandContext::SetShaderConstants(uint32 RootParam, const void* Data, uint32 Size)
{
	// 简化：root CBV + CB ring（UE 用 FRHIUniformBuffer，留将来一章）
	FD3D12Buffer* CB = CBs[Slot].get();
	memcpy(CB->GetMappedData(), Data, Size);
	ID3D12GraphicsCommandList* CL = CmdList->GetCommandList();
	CL->SetGraphicsRootConstantBufferView(RootParam, CB->GetResource()->GetGPUVirtualAddress());
}

void FD3D12CommandContext::SetTexture(uint32 RootParam, FRHITexture* Texture)
{
	FD3D12Texture* D3DTex = static_cast<FD3D12Texture*>(Texture);
	ID3D12GraphicsCommandList* CL = CmdList->GetCommandList();
	CL->SetGraphicsRootDescriptorTable(RootParam, SRVHeap->GetGPUHandle(D3DTex->GetSRVSlot()));
}

void FD3D12CommandContext::SetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBuffer)
{
	FD3D12Buffer* VB = static_cast<FD3D12Buffer*>(VertexBuffer);
	D3D12_VERTEX_BUFFER_VIEW VBV = {};
	VBV.BufferLocation = VB->GetResource()->GetGPUVirtualAddress();
	VBV.SizeInBytes = VB->GetSize();
	VBV.StrideInBytes = VB->GetStride();
	ID3D12GraphicsCommandList* CL = CmdList->GetCommandList();
	CL->IASetVertexBuffers(StreamIndex, 1, &VBV);
}

void FD3D12CommandContext::DrawIndexedPrimitive(FRHIBuffer* IndexBuffer, uint32 IndexCount)
{
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
	ID3D12GraphicsCommandList* CL = CmdList->GetCommandList();
	//barrier Render_target >> present
	D3D12_RESOURCE_BARRIER Barrier = {};
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	Barrier.Transition.pResource = Viewport->GetBackBuffer();
	Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	CL->ResourceBarrier(1, &Barrier);
}


void FD3D12CommandContext::EndFrame()
{
	CmdList->Close();
	ID3D12CommandList* Lists[] = {CmdList->GetCommandList()};
	Queue->GetD3DQueue()->ExecuteCommandLists(1, Lists);
	Viewport->PresentInternal(1);
	FrameFenceValue[Slot] = Queue->Signal(Queue->Fence); // 只记一下对应slot帧需要等的fencevalue，不用等
	FrameIndex++;
}

void FD3D12CommandContext::WaitForGPU()
{
	Queue->WaitCPU(Queue->Signal(Queue->Fence));
}

void FD3D12CommandContext::DeferredDelete(TRefCountPtr<FRHIResource> Resource)
{
	// 入队时用"当前帧将要 signal 的 fence 值"——GPU 越过它，才算引用它的这帧结束, 
	// 这里实际上是延迟一帧才删除（GPU真正的Fence值是NextCompletionValue+1操作之前的值），更加安全。严防上一帧是不是还有地方在用这个资源。
	DeletionQueue.Enqueue(std::move(Resource), Queue->Fence.NextCompletionValue);
}








