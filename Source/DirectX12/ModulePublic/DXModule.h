#define DLL_IMPORT __declspec(dllimport)
#define DLL_EXPORT __declspec(dllexport)

#ifdef DirectX12_LIBRARY
#define DXMODULE DLL_EXPORT
#else
#define DXMODULE DLL_IMPORT
#endif