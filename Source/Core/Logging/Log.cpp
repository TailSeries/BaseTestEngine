#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <vector>

CORE_DEFINE_LOG_CATEGORY(LogCore, Log);
CORE_DEFINE_LOG_CATEGORY(LogTemp, Log);

namespace CoreLog
{
namespace
{
std::shared_ptr<spdlog::logger> MakeLogger(const FLogOptions& Options)
{
    std::vector<spdlog::sink_ptr> Sinks;
    if (Options.Console)
        Sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    if (Options.FilePath && *Options.FilePath)
    {
        if (Options.MaxFileBytes == 0 || Options.MaxFiles == 0)
            throw std::invalid_argument("Log rotation limits must be greater than zero");
        Sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            Options.FilePath, Options.MaxFileBytes, Options.MaxFiles));
    }
    auto Logger = std::make_shared<spdlog::logger>("Core", Sinks.begin(), Sinks.end());
    Logger->set_level(spdlog::level::trace);
    Logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v (%s:%#)");
    Logger->flush_on(spdlog::level::err);
    return Logger;
}

struct FLogState
{
    // 单一后端位于 Core DLL；不使用 spdlog 的全局 registry。
    std::atomic<std::shared_ptr<spdlog::logger>> Logger;
    std::atomic<ELogVerbosity> Verbosity{ELogVerbosity::VeryVerbose};
    FLogState() : Logger(MakeLogger(FLogOptions{.FilePath = ""})) {}
};

FLogState& State()
{
    static FLogState Instance;
    return Instance;
}

spdlog::level::level_enum ToSpdlogLevel(ELogVerbosity Level)
{
    switch (Level)
    {
    case ELogVerbosity::Fatal: return spdlog::level::critical;
    case ELogVerbosity::Error: return spdlog::level::err;
    case ELogVerbosity::Warning: return spdlog::level::warn;
    case ELogVerbosity::Display:
    case ELogVerbosity::Log: return spdlog::level::info;
    case ELogVerbosity::Verbose: return spdlog::level::debug;
    case ELogVerbosity::VeryVerbose: return spdlog::level::trace;
    default: return spdlog::level::off;
    }
}
}

bool Initialize(const FLogOptions& Options) noexcept
{
    try
    {
        auto Next = MakeLogger(Options);
        auto Previous = State().Logger.exchange(std::move(Next));
        if (Previous) Previous->flush();
        return true;
    }
    catch (const std::exception& Error)
    {
        std::fprintf(stderr, "CoreLog initialization failed: %s\n", Error.what());
        return false;
    }
}

void Flush() noexcept
{
    try { if (auto Logger = State().Logger.load()) Logger->flush(); }
    catch (...) { std::fputs("CoreLog flush failed\n", stderr); }
}

void Shutdown() noexcept
{
    try { if (auto Logger = State().Logger.exchange(nullptr)) Logger->flush(); }
    catch (...) { std::fputs("CoreLog shutdown failed\n", stderr); }
}

void SetVerbosity(ELogVerbosity Verbosity) noexcept
{
    State().Verbosity.store(Verbosity, std::memory_order_relaxed);
}

bool IsEnabled(const FLogCategory& Category, ELogVerbosity Verbosity) noexcept
{
    if (Verbosity == ELogVerbosity::Fatal) return true;
    return Verbosity != ELogVerbosity::NoLogging && Verbosity <= Category.GetVerbosity()
        && Verbosity <= State().Verbosity.load(std::memory_order_relaxed)
        && State().Logger.load() != nullptr;
}

void WriteText(const FLogCategory& Category, ELogVerbosity Verbosity,
               FLogSource Source, std::string_view Message) noexcept
{
    try
    {
        if (auto Logger = State().Logger.load())
        {
            Logger->log(spdlog::source_loc{Source.File, Source.Line, Source.Function},
                        ToSpdlogLevel(Verbosity), "[{}] {}", Category.GetName(), Message);
            if (Verbosity == ELogVerbosity::Fatal) Logger->flush();
        }
        else if (Verbosity == ELogVerbosity::Fatal)
            std::fprintf(stderr, "[%s] Fatal: %.*s\n", Category.GetName(), int(Message.size()), Message.data());
    }
    catch (...)
    {
        std::fputs("CoreLog write failed\n", stderr);
    }
    if (Verbosity == ELogVerbosity::Fatal)
    {
        std::fflush(stderr);
        std::abort();
    }
}
}
