#pragma once
#include "FrameReource.h"
#include "Common/D3DApp.h"

class  ShapesApp:public D3DApp
{
public:
	constexpr int gNumFrameResources = 3;
	/*
	 * FrameResource[0]  FrameResource[1]  FrameResource[2]   <- CPU 和 GPU 之间的环形缓冲
	  ├─ PassCB            ├─ PassCB            ├─ PassCB      （每帧一份）
	  └─ ObjectCB          └─ ObjectCB          └─ ObjectCB    （每物体一个槽）
	 */
	struct RenderItem
	{
		DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

		//因为对象常量缓冲在每个 FrameResource 里各有一份（后面讲），所以改一次物体数据，要让它对 3 个 FrameResource 都同步更新——初始值就是 gNumFrameResources（3），这样下一次更新会连着覆盖 3
		int NumFramesDirty = gNumFrameResources;

		UINT ObjCBIndex = -1;
		MeshGeometry* Geo = nullptr;
		D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		UINT IndexCount = 0;
		UINT StartIndexLocation = 0;
		int BaseVertexLocation = 0;
	};
public:

	ShapesApp(HINSTANCE hInstance);
	ShapesApp(const ShapesApp& rhs) = delete;
	ShapesApp& operator=(const ShapesApp& rhs) = delete;
	~ShapesApp();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);


	void UpdateCamera(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void BuildDescriptorHeaps();
	void BuildConstantBufferView();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& rItems);

private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrentFrameResourceIndex = 0;
	
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	std::vector<RenderItem*> mOpaqueRitms;
	PassConstants mMainPassCB;
	UINT mPassCbvOffset = 0;

	
	bool mIsWireFrame = false;

	DirectX::XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * DirectX::XM_PI;
	float mPhi = 0.2f * DirectX::XM_PI;
	float mRadius = 15.0f;

	POINT mLastMousePos;


};
