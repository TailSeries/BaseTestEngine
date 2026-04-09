#include "LogAssert.h"
#include "CoreModule.h"
#if PLATFORM_WIN
HANDLE __LoghConsole;
#else
void* __LoghConsole;
#endif
COREMODULE void InitializeLogAssert()
{
	__LoghConsole = GetStdHandle(STD_OUTPUT_HANDLE);
}


FILE* GLogFile = nullptr;

COREMODULE void InitLogFile(const char* path)
{
	GLogFile = fopen(path, "w");
	if (GLogFile)
		setvbuf(GLogFile, nullptr, _IONBF, 0);
}