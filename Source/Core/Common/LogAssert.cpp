#include "LogAssert.h"
#include <cstdarg>
#include <cstdio>
#include <vector>

void InitializeLogAssert()
{
    CoreLog::FLogOptions Options;
    Options.FilePath = "";
    CoreLog::Initialize(Options);
}

void InitLogFile(const char* Path)
{
    CoreLog::FLogOptions Options;
    Options.FilePath = Path;
    CoreLog::Initialize(Options);
}

void CoreLog::WritePrintf(ELogVerbosity Level, FLogSource Source, const char* Format, ...) noexcept
{
    va_list Arguments;
    va_start(Arguments, Format);
    va_list Copy;
    va_copy(Copy, Arguments);
    const int Size = Format ? std::vsnprintf(nullptr, 0, Format, Copy) : -1;
    va_end(Copy);
    try
    {
        if (Size >= 0)
        {
            std::vector<char> Buffer(static_cast<size_t>(Size) + 1);
            std::vsnprintf(Buffer.data(), Buffer.size(), Format, Arguments);
            WriteText(LogCore, Level, Source, std::string_view(Buffer.data(), Size));
        }
        else
            WriteText(LogCore, ELogVerbosity::Error, Source, "Invalid legacy log format");
    }
    catch (...)
    {
        std::fputs("CoreLog legacy formatting failed\n", stderr);
    }
    va_end(Arguments);
}
