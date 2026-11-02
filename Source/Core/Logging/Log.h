#pragma once

#include "CoreModule.h"
#include <atomic>
#include <cstddef>
#include <exception>
#include <format>
#include <string_view>
#include <utility>

/*
 * Core 日志公共接口（C++20），后端为 spdlog，业务代码只需包含本文件。
 *
 * 1. 最小使用：内置 LogCore / LogTemp，无须声明分类。
 *    #include "Logging/Log.h"
 *    CORE_LOG(LogTemp, Log, "Hello Engine");
 *    CORE_LOG(LogCore, Warning, "资源 {} 未找到，重试 {} 次", "hero.mesh", 3);
 *    CORE_LOG(LogCore, Display, "帧耗时 {:.2f} ms", 16.666);
 *    CORE_LOG(LogCore, Error, "错误码 0x{:08X}", 42u);
 *
 * 2. 在 .cpp 文件作用域定义私有分类：
 *    CORE_DEFINE_LOG_CATEGORY_STATIC(LogRender, Log);
 *    CORE_LOG(LogRender, Log, "绘制 {} 个物体", 120);
 *
 *    多个文件共享分类：头文件声明，恰好一个 .cpp 定义。
 *    CORE_DECLARE_LOG_CATEGORY_EXTERN(LogGame);             // GameLog.h
 *    CORE_DEFINE_LOG_CATEGORY(LogGame, Log);                 // GameLog.cpp
 *    若跨 DLL 使用，声明时加所属模块的导入/导出宏：
 *    CORE_DECLARE_LOG_CATEGORY_EXTERN(LogRender, RHIMODULE); // 定义它的 .cpp 也包含该头文件
 *
 * 3. 程序启动时配置输出（建议在启动工作线程前）：
 *    CoreLog::FLogOptions Options;
 *    Options.FilePath = "Logs/Demo.log"; // 相对于工作目录；空字符串表示不写文件
 *    Options.Console = true;
 *    Options.MaxFileBytes = 5 * 1024 * 1024;
 *    Options.MaxFiles = 3;                // 保留的滚动历史文件数
 *    if (!CoreLog::Initialize(Options)) { // 失败会报告到 stderr，保留原配置
 *        // 按业务需要处理日志文件不可写等错误。
 *    }
 *    // 未 Initialize 时自动使用控制台；重复 Initialize 可更换输出，文件采用追加模式。
 *    // 退出：先结束各业务/工作线程，再 Flush / Shutdown。
 *    CoreLog::Flush();
 *    CoreLog::Shutdown(); // 刷新并关闭；之后普通日志忽略，重新 Initialize 可恢复
 *    // 直接调用 CoreLog::Initialize() 使用默认路径：当前工作目录/Logs/Engine.log。
 *    // 工作目录不一定是源码目录或 exe 所在目录；也可设置绝对路径，例如：
 *    // Options.FilePath = "F:/workspace/BaseTestEngine/Logs/Demo.log";
 *    // 默认单文件上限为 5 MiB，保留 3 个历史文件：Engine.1.log、Engine.2.log、Engine.3.log。
 *    // 未初始化时只输出控制台，不创建默认日志文件。
 *    // 旧接口 InitLogFile("Engine.log") 保存到当前工作目录/Engine.log。
 *
 * 4. 级别从重要到详细：Fatal, Error, Warning, Display, Log, Verbose, VeryVerbose。
 *    默认分类上限为 Log；全局上限默认 VeryVerbose，不额外限制分类。
 *    LogRender.SetVerbosity(CoreLog::ELogVerbosity::VeryVerbose);
 *    CORE_LOG(LogRender, Verbose, "调试物体 {}", 7);
 *    CoreLog::SetVerbosity(CoreLog::ELogVerbosity::Warning); // 全局只允许 Warning 及更严重
 *    LogRender.SetVerbosity(CoreLog::ELogVerbosity::NoLogging); // 关闭该分类
 *    // 必须同时通过全局和分类过滤；过滤掉的 CORE_LOG 不会求值格式化参数。
 *    // Fatal 始终尝试输出并刷新，然后 abort 终止进程，不能当普通 Error 使用。
 *    // CORE_LOG(LogCore, Fatal, "无法恢复的错误：{}", "设备初始化失败");
 *
 * 5. 格式串采用 std::format 的 {} 语法，不使用 printf 的 %d，也不需要 TEXT()。
 *    使用 UTF-8 窄字符串；运行时文本（包括 '%'、'{}'）作为参数传入：
 *    std::string Message = "用户输入：100% {原文}";
 *    CORE_LOG(LogTemp, Log, "{}", Message);
 *
 *    {:.2f} 中，':' 是通用的格式说明分隔符，'.2' 表示保留两位小数，
 *    'f' 表示定点形式；整体类似 printf 的 %.2f。冒号并非浮点数专用：
 *    "{:04d}" 将 42 输出为 "0042"，"{:X}" 将 255 输出为 "FF"。
 *    旧接口仍支持传统 printf 格式，需包含 "Common/LogAssert.h"：
 *    LogStringMsg("名称=%s，耗时=%.2f ms，数量=%d", "Demo", 16.67, 10);
 *    CORE_LOG 不解析 %s / %f / %d，不能混用两种格式语法。
 *
 * 6. 写文件与阻塞行为：当前使用同步、多线程安全的 sinks，没有后台日志线程。
 *    调用日志的线程负责格式化、获取输出锁以及写控制台/文件；锁竞争和 I/O 都可能阻塞。
 *    普通写入可能先进入缓冲区或系统缓存，但这不意味着调用无阻塞；线程安全也不等于无锁。
 *    Flush 会刷新文件缓冲区，Shutdown 会刷新并关闭；它们不保证数据已持久化到物理磁盘。
 *    不应在调度器自旋临界区或每次任务入队时大量打印日志。
 *
 *    后续如需异步输出，可改为 spdlog 异步 logger：调用线程格式化并入队，
 *    专用日志线程执行输出。但队列满时必须选择策略：阻塞等待（保留日志）或
 *    丢弃日志（避免因队列满而等待）。spdlog 的异步队列本身也使用锁，
 *    并非严格无锁、无阻塞；这些是后续可选方案，当前封装尚未启用异步模式。
 *
 * Initialize / Shutdown 是生命周期操作，应在无并发日志调用时执行。
 * 输出包含时间、级别、线程 ID、分类和源码位置；Display / Log 均映射为 spdlog info。
 * 本接口借鉴 UE_LOG 的调用形态，不实现 UE 的反射、控制台命令或编译期级别裁剪。
 */

namespace CoreLog
{
enum class ELogVerbosity : unsigned char
{
    NoLogging, Fatal, Error, Warning, Display, Log, Verbose, VeryVerbose
};

class FLogCategory
{
public:
    // Name 必须具有静态生命周期；下方宏自动使用字符串字面量。
    constexpr FLogCategory(const char* Name, ELogVerbosity DefaultVerbosity)
        : Name(Name), Verbosity(DefaultVerbosity) {}
    const char* GetName() const { return Name; }
    ELogVerbosity GetVerbosity() const { return Verbosity.load(std::memory_order_relaxed); }
    void SetVerbosity(ELogVerbosity Value) { Verbosity.store(Value, std::memory_order_relaxed); }
private:
    const char* Name;
    std::atomic<ELogVerbosity> Verbosity;
};

struct FLogSource { const char* File; int Line; const char* Function; };
struct FLogOptions
{
    const char* FilePath = "Logs/Engine.log";
    bool Console = true;
    std::size_t MaxFileBytes = 5 * 1024 * 1024;
    std::size_t MaxFiles = 3;
};

COREMODULE bool Initialize(const FLogOptions& Options = {}) noexcept;
COREMODULE void Flush() noexcept;
COREMODULE void Shutdown() noexcept;
COREMODULE void SetVerbosity(ELogVerbosity Verbosity) noexcept;
COREMODULE bool IsEnabled(const FLogCategory& Category, ELogVerbosity Verbosity) noexcept;
COREMODULE void WriteText(const FLogCategory& Category, ELogVerbosity Verbosity,
                         FLogSource Source, std::string_view Message) noexcept;

template <typename... Args>
void Write(const FLogCategory& Category, ELogVerbosity Verbosity, FLogSource Source,
           std::format_string<Args...> Format, Args&&... Arguments) noexcept
{
    try
    {
        WriteText(Category, Verbosity, Source, std::format(Format, std::forward<Args>(Arguments)...));
    }
    catch (const std::exception& Error)
    {
        // 例如动态宽度非法。Fatal 格式化失败也必须保留终止语义。
        WriteText(Category, Verbosity, Source, Error.what());
    }
}
}

#define CORE_DECLARE_LOG_CATEGORY_EXTERN(Name, ...) extern __VA_ARGS__ CoreLog::FLogCategory Name
#define CORE_DEFINE_LOG_CATEGORY(Name, DefaultVerbosity) \
    constinit CoreLog::FLogCategory Name{#Name, CoreLog::ELogVerbosity::DefaultVerbosity}
#define CORE_DEFINE_LOG_CATEGORY_STATIC(Name, DefaultVerbosity) \
    static CORE_DEFINE_LOG_CATEGORY(Name, DefaultVerbosity)

CORE_DECLARE_LOG_CATEGORY_EXTERN(LogCore, COREMODULE);
CORE_DECLARE_LOG_CATEGORY_EXTERN(LogTemp, COREMODULE);

#define CORE_LOG(Category, Verbosity, ...) \
    do { \
        const auto& CoreLogCategory = (Category); \
        constexpr auto CoreLogLevel = CoreLog::ELogVerbosity::Verbosity; \
        if (CoreLog::IsEnabled(CoreLogCategory, CoreLogLevel)) \
            CoreLog::Write(CoreLogCategory, CoreLogLevel, {__FILE__, __LINE__, __func__}, __VA_ARGS__); \
    } while (false)
