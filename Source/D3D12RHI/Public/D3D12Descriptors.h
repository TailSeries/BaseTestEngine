#pragma once
/*
 * UE 的描述符系统很庞大(offline/online 管理器、子分配、global heap、bindless),我们第4章只做最小可用的 FD3D12DescriptorHeap 封装,目标是能给 back buffer 建 RTV
 * 三个必须搞懂的点

 * ① DescriptorSize(增量)是硬件相关的,必须查
 * 一个描述符占多少字节,每家 GPU 不同,不能写死。用 GetDescriptorHandleIncrementSize(Type) 查,然后第 N 个槽的地址 = base + N × size。
 * 
 * ② shader-visible 与否
 * - RTV/DSV 堆永远非 shader-visible——它们是给管线输出合并阶段(OM)用的,shader 不采样。建 RTV/DSV 堆时带 SHADER_VISIBLE 标志会创建失败。只有 CPU handle。
 * - CBV_SRV_UAV / SAMPLER 若要 shader 读,必须 shader-visible——有 CPU handle(写描述符用)+ GPU handle(绑给 shader 用)。同类型 shader-visible 堆同一时刻只能绑一个。
 * 但是注意：一个资源可以有多个不同类型的 view。渲染到纹理再采样,用的是两个不同的 descriptor 指向同一个纹理
 * Render-to-Texture 用的是两个 view：同一份资源从RTV→变化到了SRV

             同一个纹理资源（ID3D12Resource）
             ┌───────────────────────────┐
             │   RenderTargetTexture      │
             └───────────────────────────┘
               ▲                      ▲
          RTV(写)                  SRV(读)
      在 RTV 堆                在 CBV_SRV_UAV 堆
      非 shader-visible        shader-visible
      OMSetRenderTargets 用    shader 采样用
 * 
 * ③ CPU handle vs GPU handle
 * - CPU handle:CPU 往这个地址写描述符(CreateRenderTargetView 把 RTV 写进去)。
 * - GPU handle:绑管线时用(SetGraphicsRootDescriptorTable)。RTV/DSV 特殊,绑定时(OMSetRenderTargets)用的是 CPU handle。
 */
#include "D3D12RHIModule.h"
#include "D3D12RHIPrivate.h"
#include "GenericPlatform.h"

using Microsoft::WRL::ComPtr;
class FD3D12Device;

// UE: class FD3D12DescriptorHeap（D3D12Descriptors.h，含 offline/online 管理器等）
// 简化：单个堆封装 + 线性分配（NextFreeSlot），不做子分配/global/bindless
class D3D12RHIMODULE FD3D12DescriptorHeap
{
public:
	FD3D12DescriptorHeap(FD3D12Device* InDevice, D3D12_DESCRIPTOR_HEAP_TYPE InType, uint32 InNumDescriptors, bool bShaderVisible);
	~FD3D12DescriptorHeap();

	ID3D12DescriptorHeap* GetHeap() const { return Heap.Get(); }
	uint32 GetDescriptorSize() const { return DescriptorSize; }
	uint32 GetNumDescriptors() const { return NumDescriptors; }

	// 第 slot个描述符的句柄 = base + slot * DescriptorSize
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32 slot) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32 slot) const; // 仅对shaderisible 有效。

	// 简单线性分配：返回下一个空slot
	uint32 Allocate();

private:
	FD3D12Device* Parent = nullptr;
	ComPtr<ID3D12DescriptorHeap> Heap;
	D3D12_DESCRIPTOR_HEAP_TYPE Type;
	uint32 NumDescriptors = 0;
	uint32 DescriptorSize = 0;
	uint32 NextFreeSlot = 0;
	bool bShaderVisible = false;
	D3D12_CPU_DESCRIPTOR_HANDLE Cpubase{};
	D3D12_GPU_DESCRIPTOR_HANDLE GpuBase{};
};

