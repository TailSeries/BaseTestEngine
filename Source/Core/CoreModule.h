#define DLL_IMPORT __declspec(dllimport)
#define DLL_EXPORT __declspec(dllexport)

#ifdef Core_LIBRARY
#define COREMODULE DLL_EXPORT
#else
#define COREMODULE DLL_IMPORT
#endif