#pragma once
#include "Common/D3DApp.h"
#include <DirectXCollision.h>
#include "ModulePublic/DXModule.h"
class DXMODULE InitDirect3DApp:public D3DApp
{
public:
	InitDirect3DApp(HINSTANCE hInstance);
	~InitDirect3DApp();
	virtual bool Initialize() override;
protected:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;
};