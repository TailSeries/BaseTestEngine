#define DLL_IMPORT __declspec(dllimport)
#define DLL_EXPORT __declspec(dllexport)

#ifdef ALGORITHM_LIBRARY
#define ALGOMODULE DLL_EXPORT
#else
#define ALGOMODULE DLL_IMPORT
#endif