#include "InitDirect3D.h"

#include <iostream>

InitDirect3DApp::InitDirect3DApp(HINSTANCE hInstance)
	:D3DApp(hInstance)
{

}

InitDirect3DApp::~InitDirect3DApp()
{
}

bool InitDirect3DApp::Initialize()
{
	return D3DApp::Initialize();
}

void InitDirect3DApp::OnResize()
{
	D3DApp::OnResize();
}

void InitDirect3DApp::Update(const GameTimer& gt)
{
}

void InitDirect3DApp::Draw(const GameTimer& gt)
{
	//当前这一帧的绘制命令
	//命令真正的俄内存是CommandAllocator来分配的，所以我们都是当一帧的命令开始之前（也就是上一帧的命令结束之后）来执行reset
	ThrowIfFailed(MCommandAllocator->Reset());
	ThrowIfFailed(MCommandList->Reset(MCommandAllocator.Get(), nullptr));

	/*
	 * 来到新的一帧了，那上一帧的backbuffer要从内存状态presetn到rendertaget
	 */
	CD3DX12_RESOURCE_BARRIER BackBufferResourceBarrierPresent2RT  = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	MCommandList->ResourceBarrier(1, &BackBufferResourceBarrierPresent2RT);

	//重新设置ViewPort和scissorrect
	MCommandList->RSSetViewports(1, &MScreenViewPort);
	MCommandList->RSSetScissorRects(1, &MScissorRect);

	/*
	 * 清空backbuffer和depthbuffer
	 */
	MCommandList->ClearRenderTargetView(CurrentBackBufferView(), DirectX::Colors::LightSteelBlue, 0, nullptr);
	MCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0 , nullptr);

	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferViewHandle = CurrentBackBufferView();
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentDepthStencilViewHandle = DepthStencilView();
	//指示当前命令列表的渲染目标和深度缓冲区
	MCommandList->OMSetRenderTargets(1, &CurrentBackBufferViewHandle, true, &CurrentDepthStencilViewHandle);

	//将currentbackbuffer的state从rendertagret转到present
	CD3DX12_RESOURCE_BARRIER BackBufferResourceBarrierRT2Present = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	MCommandList->ResourceBarrier(1, &BackBufferResourceBarrierRT2Present);

	//命令录制结束 一定要close
	ThrowIfFailed(MCommandList->Close());

	//执行命令
	ID3D12CommandList* CmdLists[] = {MCommandList.Get()};
	MCommandQueue->ExecuteCommandLists(_countof(CmdLists), CmdLists);
	ThrowIfFailed(MSwapChain->Present(0, 0));

	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;
	FlushCommandQueue();

	//swap
	/*
	 * 第一个参数涉及垂直同步，0 立即提交，1等待下一次屏幕刷新时
	 * 第二个参数是提交的行为限制 0：默认行为；DXGI_PRESENT_DO_NOT_WAIT 如果前台还没准备好，不阻塞，返回 DXGI_ERROR_WAS_STILL_DRAWING。DXGI_PRESENT_ALLOW_TEARING开启flipmodel之后支持画面撕裂的方式直接提交
	 */
}


