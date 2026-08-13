#pragma once

#ifdef D3D12RHI_LIBRARY
#define D3D12RHIMODULE __declspec(dllexport)
#else
#define D3D12RHIMODULE __declspec(dllimport)
#endif