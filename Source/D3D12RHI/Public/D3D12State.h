#pragma once
#include "D3D12RHIModule.h"
#include "D3D12RHIPrivate.h"
#include "RHIResources.h"

class D3D12RHIMODULE FD3D12RasterizerState:public FRHIRasterizerState
{
public:
	D3D12_RASTERIZER_DESC Desc{};
	virtual bool GetInitializer(FRasterizerStateInitializerRHI& Init) override;
};


class D3D12RHIMODULE FD3D12DepthStencilState:public FRHIDepthStencilState
{
public:
	D3D12_DEPTH_STENCIL_DESC Desc{};
	virtual bool GetInitializer(FDepthStencilStateInitializerRHI& Init) override;
};

class D3D12RHIMODULE FD3D12BlendState:public  FRHIBlendState
{
public:
	D3D12_BLEND_DESC Desc{};
	virtual bool GetInitializer(FBlendStateInitializerRHI& Init) override;
};
