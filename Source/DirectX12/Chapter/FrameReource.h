#pragma once
#include "Common/D3DUtil.h"
#include "Common/MathHelper.h"
#include "Common/UploadBuffer.h"
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
};


struct PassConstants
{
    // 总大小：384 + 16 + 16 + 16 = 432 字节，经 CalcConstantBufferByteSize 对齐后实际分配 512 字节（256的倍数）。

    DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();        // 64 bytes
    DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();     // 64 bytes
    DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();        // 64 bytes
    DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();     // 64 bytes
    DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();    // 64 bytes
    DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4(); // 64 bytes  → 合计 384 bytes

    DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f }; // 12 bytes
    float cbPerObjectPad1 = 0.0f; //  4 bytes   → 填充到16字节边界

    DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };   // 8 bytes
    DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };  // 8 bytes   → 合计 16 bytes
    float NearZ = 0.0f;      // 4 bytes
    float FarZ = 0.0f;       // 4 bytes
    float TotalTime = 0.0f;  // 4 bytes
    float DeltaTime = 0.0f;  // 4 bytes  → 合计 16 bytes
};


struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;
};




struct FrameResource
{

    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT waveVertCount);
    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource& rhs) = delete;
    ~FrameResource();

	// GPU执行命令期间不能Reset Allocator，所以每个帧资源需要独立的Allocator，CPU可以在新一帧Reset自己的Allocator
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;
    // GPU可能还在读取上一帧的PassCB数据，CPU不能覆盖它。三帧各自独立，CPU写当前帧的CB，GPU读上一帧的CB
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
    // 水波顶点数据每帧变化，GPU可能在读上一帧的顶点数据。三帧各自有独立VB，避免覆盖冲突 
    std::unique_ptr<UploadBuffer<Vertex>> WavesVB = nullptr;
    // CPU轮询到此帧资源时，检查 mFence->GetCompletedValue() < Fence，如果GPU还没完成就阻塞等待
    uint64 Fence;

};