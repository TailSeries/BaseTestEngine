#pragma once

#include "D3D12RHIModule.h"
#include "D3D12RHIPrivate.h"
#include "D3D12Descriptors.h"
#include "GenericPlatform.h"
#include <vector>
#include "D3D12Resources.h"
using Microsoft::WRL::ComPtr;
class FD3D12Adapter;
// UE: class FD3D12Viewport : FRHIViewport, FD3D12AdapterChild
// 已接入 FRHIViewport；Adapter 回指暂以成员保存。
// BackBuffers 的纹理持有 RTV；描述符堆归 Device 所有。
class D3D12RHIMODULE FD3D12Viewport : public FRHIViewport
{
public:
	FD3D12Viewport(FD3D12Adapter* InAdapter, HWND InWindowHandle, uint32 InSizeX, uint32 InSizeY, DXGI_FORMAT InFormat, uint32 InNumBackBuffers);
	~FD3D12Viewport() override;

	FD3D12Viewport(const FD3D12Viewport&) = delete;
	FD3D12Viewport& operator=(const FD3D12Viewport&) = delete;

	void Init();  // UE 同名：建 swap chain + 取后备缓冲（构造后单独调）
	void Resize(uint32 NewSizeX, uint32 NewSizeY);
	void PresentInternal(int32 SyncInterval); // UE 同名：真正调 SwapChain->Present

	FD3D12Texture* GetBackBuffer() const { return BackBuffers[GetCurrentBackBufferIndex()].get(); };
	uint32           GetCurrentBackBufferIndex() const { return SwapChain->GetCurrentBackBufferIndex(); }
	IDXGISwapChain3* GetSwapChain()              const { return SwapChain.Get(); }
	uint32           GetNumBackBuffers()         const { return NumBackBuffers; }


	// 当前 TRefCountPtr 是共享引用别名，必须复制已有控制块。
	// 不可从 GetBackBuffer() 的裸指针重新构造共享指针。
	TRefCountPtr<FD3D12Texture> GetBackBufferRef() const
	{
		return BackBuffers[GetCurrentBackBufferIndex()];
	}

private:
	void ResizeInternal(); // UE 同名：从 swap chain 重新取回后备缓冲（Init / Resize 复用）
	FD3D12Adapter* Adapter = nullptr;
	HWND WindowHandle = nullptr;
	uint32 SizeX = 0;
	uint32 SizeY = 0;
	DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	uint32 NumBackBuffers = 2;
	ComPtr<IDXGISwapChain3> SwapChain;
	std::vector<TRefCountPtr<FD3D12Texture>> BackBuffers;
};
