#pragma once
#include "Logging/Log.h"

// 旧接口兼容层：已有 %d / %s 调用继续可用，统一输出到 Core 的 spdlog 后端。
// 新代码请使用 Logging/Log.h 中的 CORE_LOG（{} 格式）。
COREMODULE void InitializeLogAssert();
COREMODULE void InitLogFile(const char* Path);

namespace CoreLog
{
COREMODULE void WritePrintf(ELogVerbosity Level, FLogSource Source, const char* Format, ...) noexcept;

// 单个文本参数按原文输出，避免驱动错误消息内的 '%' 被解释成 printf 占位符。
inline void WriteLegacy(ELogVerbosity Level, FLogSource Source, const char* Text) noexcept
{
    WriteText(LogCore, Level, Source, Text ? Text : "(null)");
}

template <typename... Args> requires (sizeof...(Args) > 0)
void WriteLegacy(ELogVerbosity Level, FLogSource Source, const char* Format, Args... Arguments) noexcept
{
    WritePrintf(Level, Source, Format, Arguments...);
}
}

#define CORE_LEGACY_LOG(Level, ...) \
    do { \
        if (CoreLog::IsEnabled(LogCore, CoreLog::ELogVerbosity::Level)) \
            CoreLog::WriteLegacy(CoreLog::ELogVerbosity::Level, {__FILE__, __LINE__, __func__}, __VA_ARGS__); \
    } while (false)

#define LogStringMsg(...) CORE_LEGACY_LOG(Log, __VA_ARGS__)
#define DebugStringMsg(...) CORE_LEGACY_LOG(Display, __VA_ARGS__)
#define WarningStringMsg(...) CORE_LEGACY_LOG(Warning, __VA_ARGS__)
#define ErrorStringMsg(...) CORE_LEGACY_LOG(Error, __VA_ARGS__)
#define LogTraceMsg(...) LogStringMsg(__VA_ARGS__)
#define DebugTraceMsg(...) DebugStringMsg(__VA_ARGS__)
#define WarningTraceMsg(...) WarningStringMsg(__VA_ARGS__)
#define ErrorTraceMsg(...) ErrorStringMsg(__VA_ARGS__)
