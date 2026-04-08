#pragma once
#define CHAPTER_6 1
#include <vector>
#ifdef  CHAPTER_6
#include "Common/D3DApp.h"
#include "Common/MathHelper.h"



class BoxApp:public D3DApp
{
public:
	BoxApp(HINSTANCE hInstance);
	BoxApp(const BoxApp& rhs) = delete;
	BoxApp& operator=(const BoxApp& rhs) = delete;
	~BoxApp();

	virtual bool Initialize() override;


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



};


#endif


