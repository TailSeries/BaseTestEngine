#pragma once
#include "Common/D3DUtil.h"
#include "Common/MathHelper.h"
#include "Common/UploadBuffer.h"

/*
 * 帧资源
 */

/*
 * 位置信息，矩阵 
 */
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
};

// 当前这个渲染流里需要放在常量缓冲区的部分
struct PassConstants
{
    DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4(); //视图矩阵：把世界坐标变换到摄像机空间（Camera Space）。摄像机在原点、看向 -Z。
    DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();  //View 的逆：摄像机空间 → 世界空间。主要用来做从相机空间重建世界坐标（比如算 view ray 方向、做后处理/SSAO 时需要）。注意它不是把模型放回原位用的，那是 gWorld 的逆
    DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4(); //  投影矩阵：摄像机空间 → 齐次裁剪空间（clip space）。包含了 FOV、宽高比、Near/Far。
    DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4(); // ViewProj 的逆：裁剪空间 → 世界空间。最常用场景是从深度重建世界坐标（InvViewProj 对屏幕坐标和 depth  进行反投影）
    DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f }; // 摄像机世界坐标位置。光照、雾、视线方向都要用到，所以每个 pass 都传下来，省得每个像素再求逆。
    float cbPerObjectPad1 = 0.0f; //  对齐填充（padding）。XMFLOAT3 是 12 字节，HLSL 里 float3 对齐到 16 字节，所以补 4 字节让后面  RenderTargetSize 对齐到 16 字节边界。这只是 C++ 侧对齐 HLSL 的产物，shader  里没有任何逻辑用它。
    DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };//当前渲染目标（swap chain back buffer / depth buffer）的像素尺寸，如 {1280, 720}。
    DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f }; // 尺寸的倒数 {1/1280, 1/720}。一个像素对应的 UV 增量，几乎等于 "texel  size"，是全屏后处理（全屏 quad 遍历相邻像素、模糊、Sobel 边缘检测）的标准工具
    float NearZ = 0.0f;
    float FarZ = 0.0f;
    float TotalTime = 0.0f;
    float DeltaTime = 0.0f;
};



struct FrameResource
{
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();


	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	UINT64 Fence = 0;
};







