#pragma once

#include "D3D12RHIModule.h"
#include "D3D12RHIPrivate.h"
#include "Base/GenericPlatform.h"
#include <vector>
using Microsoft::WRL::ComPtr;
class FD3D12Adapter;
// UE: class FD3D12Viewport : FRHIViewport, FD3D12AdapterChild
// 精简：去 FRHIViewport 基类 + 多线程 Present；BackBuffer 先存裸资源

class D3D12RHIMODULE FD3D12Viewport
{
public:
	FD3D12Viewport(FD3D12Adapter* InAdapter, HWND InWindowHandle, uint32 InSizeX, uint32 InSizeY, DXGI_FORMAT InFormat, uint32 InNumBackBuffers);
	~FD3D12Viewport();

	FD3D12Viewport(const FD3D12Viewport&) = delete;
	FD3D12Viewport& operator=(const FD3D12Viewport&) = delete;

	void Init();  // UE 同名：建 swap chain + 取后备缓冲（构造后单独调）
	void Resize(uint32 NewSizeX, uint32 NewSizeY);
	void PresentInternal(int32 SyncInterval); // UE 同名：真正调 SwapChain->Present

	ID3D12Resource* GetBackBuffer() const { return BackBuffers[GetCurrentBackBufferIndex()].Get(); };
	uint32           GetCurrentBackBufferIndex() const { return SwapChain->GetCurrentBackBufferIndex(); }
	IDXGISwapChain3* GetSwapChain()              const { return SwapChain.Get(); }
	uint32           GetNumBackBuffers()         const { return NumBackBuffers; }
private:
	void ResizeInternal(); // UE 同名：从 swap chain 重新取回后备缓冲（Init / Resize 复用）
	FD3D12Adapter* Adapter = nullptr;
	HWND WindowHandle = nullptr;
	uint32 SizeX = 0;
	uint32 SizeY = 0;
	DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	uint32 NumBackBuffers = 2;
	ComPtr<IDXGISwapChain3> SwapChain;
	std::vector<ComPtr<ID3D12Resource>> BackBuffers;
};