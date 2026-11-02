#include "Logging/Log.h"
#include "Common/LogAssert.h"
#include "LogTestCategory.h"
#include <Windows.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <thread>
#include <vector>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

CORE_DEFINE_LOG_CATEGORY_STATIC(LogExample, Log);

static void Check(bool Value, const char* Message)
{
    if (!Value) { std::cerr << "FAIL: " << Message << '\n'; std::exit(1); }
}

static std::string Read(const std::filesystem::path& Path)
{
    std::ifstream File(Path, std::ios::binary);
    return {std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>()};
}

int main(int Argc, char** Argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (Argc == 3 && std::string_view(Argv[1]) == "--fatal")
    {
#ifdef _MSC_VER
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
        CoreLog::FLogOptions Options{.FilePath = Argv[2], .Console = false};
        if (!CoreLog::Initialize(Options)) return 2;
        CoreLog::SetVerbosity(CoreLog::ELogVerbosity::NoLogging);
        LogExample.SetVerbosity(CoreLog::ELogVerbosity::NoLogging);
        CORE_LOG(LogExample, Fatal, "fatal-flushed {}", 42);
        return 0;
    }

    // 独立进程目录，保留文件便于查看实际输出。
    const auto Directory = std::filesystem::current_path() /
        ("log-tests-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
    std::filesystem::create_directories(Directory);
    const auto FilePath = (Directory / "basic.log").string();
    CORE_LOG(LogTemp, Log, "console logging before Initialize");
    CoreLog::FLogOptions Options{.FilePath = FilePath.c_str(), .Console = false};
    Check(CoreLog::Initialize(Options), "initialize file logger");
    CORE_LOG(LogExample, Log, "hello {} {:.2f} {:04X} 中文", 42, 1.25, 255);
    CORE_LOG(LogCore, Warning, "exported Core DLL category");
    LogFromOtherTranslationUnit();

    int Evaluations = 0;
    CORE_LOG(LogExample, Verbose, "filtered {}", ++Evaluations);
    Check(Evaluations == 0, "filtered arguments must not be evaluated");
    LogExample.SetVerbosity(CoreLog::ELogVerbosity::VeryVerbose);
    CORE_LOG(LogExample, Verbose, "verbose-visible");
    CORE_LOG(LogExample, VeryVerbose, "trace-visible");
    CoreLog::SetVerbosity(CoreLog::ELogVerbosity::Warning);
    CORE_LOG(LogExample, Log, "filtered-global {}", ++Evaluations);
    Check(Evaluations == 0, "global filter");
    CORE_LOG(LogExample, Error, "error-visible");
    CoreLog::SetVerbosity(CoreLog::ELogVerbosity::VeryVerbose);
    LogSharedTest.SetVerbosity(CoreLog::ELogVerbosity::NoLogging);
    LogFromOtherTranslationUnit();

    LogStringMsg("legacy number %d text %s", 7, "ok");
    ErrorStringMsg("literal 100% {braces}");
    WarningTraceMsg("legacy trace %d", 9);
    CoreLog::Flush();
    auto Text = Read(FilePath);
    Check(Text.find("[LogExample] hello 42 1.25 00FF 中文") != std::string::npos, "format/category/UTF-8");
    Check(Text.find("LogTests.cpp:") != std::string::npos, "source location");
    Check(Text.find("exported Core DLL category") != std::string::npos, "DLL category");
    const auto Shared = Text.find("other translation unit");
    Check(Shared != std::string::npos && Text.find("other translation unit", Shared + 1) == std::string::npos, "shared category filter");
    Check(Text.find("filtered") == std::string::npos, "filtered output absent");
    Check(Text.find("verbose-visible") != std::string::npos && Text.find("trace-visible") != std::string::npos, "verbose levels");
    Check(Text.find("legacy number 7 text ok") != std::string::npos && Text.find("literal 100% {braces}") != std::string::npos, "legacy bridge");

    // 初始化失败应保留已有 logger。
    auto Invalid = Options;
    Invalid.MaxFileBytes = 0;
    Check(!CoreLog::Initialize(Invalid), "reject invalid rotation limits");
    CORE_LOG(LogExample, Log, "after-failed-init");

    std::vector<std::thread> Writers;
    for (int Thread = 0; Thread < 4; ++Thread)
        Writers.emplace_back([Thread]
        {
            for (int Index = 0; Index < 250; ++Index)
                CORE_LOG(LogExample, Log, "concurrent-item {}:{}", Thread, Index);
        });
    for (auto& Writer : Writers) Writer.join();
    CoreLog::Shutdown();
    Text = Read(FilePath);
    Check(Text.find("after-failed-init") != std::string::npos, "preserve logger after failure");
    std::set<std::string> Items;
    size_t Position = 0;
    while ((Position = Text.find("concurrent-item ", Position)) != std::string::npos)
    {
        Position += std::string_view("concurrent-item ").size();
        const auto End = Text.find(' ', Position);
        Check(Items.insert(Text.substr(Position, End - Position)).second, "duplicate concurrent line");
    }
    Check(Items.size() == 1000, "concurrent logging lost or interleaved lines");
    CORE_LOG(LogExample, Log, "after-shutdown {}", ++Evaluations);
    Check(Evaluations == 0, "shutdown suppresses calls");
    CoreLog::Shutdown();
    Check(CoreLog::Initialize(Options), "restart logger");
    CORE_LOG(LogExample, Log, "after-restart");
    CoreLog::Shutdown();
    Check(Read(FilePath).find("after-restart") != std::string::npos && Read(FilePath).find("hello 42") != std::string::npos, "restart appends");

    const auto RotatingPath = (Directory / "rotate.log").string();
    Options.FilePath = RotatingPath.c_str();
    Options.MaxFileBytes = 512;
    Options.MaxFiles = 2;
    Check(CoreLog::Initialize(Options), "rotating logger");
    for (int Index = 0; Index < 100; ++Index)
        CORE_LOG(LogExample, Log, "rotation-item {} padding padding padding padding", Index);
    CoreLog::Shutdown();
    Check(std::filesystem::exists(Directory / "rotate.1.log") && std::filesystem::exists(Directory / "rotate.2.log"), "rotation archives");
    Check(!std::filesystem::exists(Directory / "rotate.3.log"), "rotation retention bound");
    Check(Read(RotatingPath).find("rotation-item 99") != std::string::npos, "latest rotated message");

    const auto LegacyPath = (Directory / "legacy.log").string();
    InitLogFile(LegacyPath.c_str());
    LogStringMsg("legacy-init %d", 123);
    CoreLog::Shutdown();
    Check(Read(LegacyPath).find("legacy-init 123") != std::string::npos, "legacy file initialization");

    // 子进程验证 Fatal：过滤关闭也必须落盘并终止。
    const auto FatalPath = (Directory / "fatal.log").string();
    std::string Command = "\"" + std::string(Argv[0]) + "\" --fatal \"" + FatalPath + "\"";
    STARTUPINFOA Startup{};
    Startup.cb = sizeof(Startup);
    PROCESS_INFORMATION Process{};
    Check(CreateProcessA(nullptr, Command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                         nullptr, nullptr, &Startup, &Process), "spawn fatal child");
    Check(WaitForSingleObject(Process.hProcess, 10000) == WAIT_OBJECT_0, "fatal child exited");
    DWORD ExitCode = 0;
    Check(GetExitCodeProcess(Process.hProcess, &ExitCode), "fatal child exit code");
    CloseHandle(Process.hThread);
    CloseHandle(Process.hProcess);
    Check(ExitCode != 0 && Read(FatalPath).find("fatal-flushed 42") != std::string::npos, "Fatal terminates and flushes");
    std::cout << "PASS: logging formatting, filtering, DLL categories, legacy bridge, concurrency, rotation, restart, Fatal\n";
}
