#pragma once
#define CHAPTER_6 1
#include <vector>
#ifdef  CHAPTER_6
#include "Common/D3DApp.h"
#include "Common/MathHelper.h"
#include "Common/UploadBuffer.h"


class BoxApp:public D3DApp
{
public:
	struct Vertex
	{
		DirectX::XMFLOAT3 Pos; // 顶点位置
		DirectX::XMFLOAT4 Color; // 每个顶点的颜色信息
	};

	//把变换矩阵信息放在constantbuffer里,这里我们直接用单位矩阵
	struct ObjectConstants
	{
		DirectX::XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
	};


public:
	BoxApp(HINSTANCE hInstance);
	BoxApp(const BoxApp& rhs) = delete;
	BoxApp& operator=(const BoxApp& rhs) = delete;
	~BoxApp();

	virtual bool Initialize() override;

	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();




private:
	//填充顶点布局
	void FillInputLayout();

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;

private:
	//1. 顶点输入元素布局
	std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayout;

	// cbv 堆
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> MCbvHeap = nullptr;

	// 根签名：必须绑定cbv
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	// consttandbuffer 具体的一个资源
	std::unique_ptr<UploadBuffer<ObjectConstants>> MObjectCB = nullptr;


	// 编译的二进制shader代码
	Microsoft::WRL::ComPtr<ID3DBlob> MvsByteCode = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> MpsByteCode = nullptr;

};


#endif


