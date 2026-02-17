#define DLL_IMPORT __declspec(dllimport)
#define DLL_EXPORT __declspec(dllexport)

#ifdef TRANSFER_LIBRARY
#define TRANSFERMODULE DLL_EXPORT
#else
#define TRANSFERMODULE DLL_IMPORT
#endif