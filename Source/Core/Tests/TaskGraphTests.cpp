/*
 * TaskGraph 使用示例与回归测试（链接实际的 Core）。
 * 阅读顺序：RunCycle 的初始化 -> ExampleBasicUsage -> 各项进阶示例 -> RunCycle 的关闭。
 *
 * 常用接口：CreateAndDispatchWhenReady(任务函数, 前置事件, 目标线程)。
 * 前置事件可以是 nullptr、单个 FGraphEventRef 或 FGraphEventArray 的地址；
 * 返回值是本任务的完成事件，可作为后续任务的依赖，也可以交给 WaitUntilTaskCompletes。
 * 提交后任务可能立即执行；提交顺序不保证执行顺序，需要顺序时显式传入依赖。
 *
 * 本文件用 [&] 捕获局部变量，并在离开作用域前等待相关任务完成。
 * 实际异步业务也必须保证捕获对象的生命周期；持有完成事件不会延长被引用对象的生命周期。
 * 以下 Check、看门狗、句柄计数是测试设施，接入引擎业务时不需要照搬。
 */
#include "Task/FunctionGraphTask.h"
#include "Threads/ThreadManager.h"
#include "Threads/RunnableThread.h"
#include "Threads/EventPool.h"

#include <barrier>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <latch>
#include <thread>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

static void Check(bool Condition, const char* Message)
{
    if (!Condition)
    {
        std::cerr << "FAIL: " << Message << std::endl;
        std::_Exit(1);
    }
}

static int RegisteredThreads()
{
    int Count = 0;
    FThreadManager::Get().ForEachThread([&](uint32 Id, FRunnableThread* Thread)
    {
        Check(Id == Thread->GetThreadID(), "registered thread id");
        Check(!Thread->GetThreadName().empty(), "registered thread name");
        ++Count;
    });
    return Count;
}

// Thread 描述调用等待接口的当前线程/队列，不是被等待任务的执行目标。
// 默认值只适用于已经 AttachToThread(GameThread) 的调用者。
static void Wait(const FGraphEventRef& Event, ENamedThreads::Type Thread = ENamedThreads::GameThread)
{
    FTaskGraphInterface::Get().WaitUntilTaskCompletes(Event, Thread);
    Check(Event->IsComplete(), "Wait returned before completion");
}

// 示例 1：工作线程计算 -> GameThread 使用结果。需先按 RunCycle 初始化并绑定主线程。
static void ExampleBasicUsage()
{
    int ComputedValue = 0;
    int AppliedValue = 0;

    // 未指定目标时默认 AnyThread，由 TaskGraph 选择工作线程。
    auto Compute = FFunctionGraphTask::CreateAndDispatchWhenReady([&]
    {
        ComputedValue = 6 * 7;
    });

    // Compute 是前置依赖：计算完成后，才将使用结果的任务交给 GameThread 执行。
    // 此处普通 int 即可：依赖保证写入与读取有先后关系，没有并发访问。
    auto Apply = FFunctionGraphTask::CreateAndDispatchWhenReady([&]
    {
        Check(FTaskGraphInterface::Get().GetCurrentThreadIfKnown() == ENamedThreads::GameThread,
            "basic example GameThread affinity");
        AppliedValue = ComputedValue;
    }, Compute, ENamedThreads::GameThread);

    // GameThread 的等待会处理该命名线程的任务队列，因此 Apply 能在这里执行。
    // 引擎主循环也可以主动处理队列；这里等待是为了让示例自包含，并保护局部变量的生命周期。
    Wait(Apply);
    Check(AppliedValue == 42, "basic compute/apply example");
}

// 示例 2：批量并行任务、优先级选择，以及未绑定的外部线程提交/等待任务。
static void TestAnyThreadAndPriorities(bool High, bool Background)
{
    auto& Graph = FTaskGraphInterface::Get();
    // 多个无依赖任务会并发修改 Count，所以这里必须使用原子操作。
    std::atomic<int> Count{0};
    FGraphEventArray Events;
    for (int Index = 0; Index < 1000; ++Index)
        Events.push_back(FFunctionGraphTask::CreateAndDispatchWhenReady([&] { ++Count; }));
    Graph.WaitUntilTasksComplete(Events, ENamedThreads::GameThread);
    Check(Count == 1000, "AnyThread tasks exactly once");

    // Thread 优先级选择工作线程组，Task 优先级选择组内任务队列。
    // 对应线程组未启用时会回退到普通线程组；优先级不能代替任务依赖。
    const ENamedThreads::Type Targets[]{ENamedThreads::AnyNormalThreadNormalTask,
        ENamedThreads::AnyHiPriThreadHiPriTask, ENamedThreads::AnyBackgroundThreadNormalTask};
    for (int Index = 0; Index < 3; ++Index)
    {
        const int ExpectedPriority = Index == 1 ? (High ? 1 : 0) : Index == 2 ? (Background ? 2 : 0) : 0;
        auto Event = FFunctionGraphTask::CreateAndDispatchWhenReady([&, ExpectedPriority]
        {
            const auto Current = Graph.GetCurrentThreadIfKnown();
            Check(ENamedThreads::GetThreadIndex(Current) != ENamedThreads::AnyThread, "worker TLS");
            Check(ENamedThreads::GetThreadPriorityIndex(Current) == ExpectedPriority, "priority group/fallback mapping");
        }, nullptr, Targets[Index]);
        Wait(Event);
    }

    // 外部 std::thread 无需绑定为命名线程也能提交任务；等待时传 AnyThread，走事件等待路径。
    std::thread External([&]
    {
        Check(Graph.GetCurrentThreadIfKnown() == ENamedThreads::AnyThread, "external TLS should be unset");
        auto Event = FFunctionGraphTask::CreateAndDispatchWhenReady([&] { ++Count; });
        Wait(Event, ENamedThreads::AnyThread);
    });
    External.join();
    Check(Count == 1001, "external thread wait");
}

using FHeldTask = TGraphTask<TFunctionGraphTaskImpl<void(), ESubsequentsMode::TrackSubsequents>>;

// 示例 3：Root -> 32 个并行分支 -> Join；还展示手动放行与已完成的前置依赖。
static void TestDependencies()
{
    std::atomic<int> Count{0};
    // ConstructAndHold 构造但暂不放行，适合先连接依赖图、再统一启动的场景。
    auto* Root = FHeldTask::CreateTask().ConstructAndHold(std::function<void()>([&] { ++Count; }), ENamedThreads::AnyThread);
    auto RootEvent = Root->GetCompletionEvent();
    FGraphEventArray Branches;
    for (int Index = 0; Index < 32; ++Index)
        Branches.push_back(FFunctionGraphTask::CreateAndDispatchWhenReady([&]
        {
            Check(RootEvent->IsComplete(), "child ran before prerequisite");
            ++Count;
        }, RootEvent));
    // 传入事件数组表示等待所有分支，而不是任意一个分支。
    auto Join = FFunctionGraphTask::CreateAndDispatchWhenReady([&]
    {
        Check(Count == 33, "fan-in ran before all branches");
        ++Count;
    }, &Branches);
    Check(Count == 0 && !Join->IsComplete(), "held task started before Unlock");
    // Unlock 后任务可能执行并自动销毁，之后只保留/使用 RootEvent，不再访问 Root 指针。
    Root->Unlock();
    Wait(Join);
    Check(Count == 34, "fan-out/fan-in count");
    // 已完成的事件也可以安全作为依赖，不需要调用方额外判断 IsComplete。
    auto Late = FFunctionGraphTask::CreateAndDispatchWhenReady([&] { ++Count; }, Join);
    Wait(Late);
    Check(Count == 35, "already-complete prerequisite");
}

// 内部并发回归：刻意让依赖注册与事件完成同时发生，检查是否丢失后续任务。
// Gate 是测试手动创建的事件，才由测试调用 DispatchSubsequents。
// 普通业务提交任务后由调度器完成事件，不应手动完成任务返回的事件。
static void TestRegistrationRace()
{
    constexpr int Rounds = 2000;
    std::barrier Rendezvous(2);
    FGraphEventRef Gate;
    FGraphEventArray Results;
    std::atomic<int> Count{0};
    std::thread Completer([&]
    {
        for (int Round = 0; Round < Rounds; ++Round)
        {
            Rendezvous.arrive_and_wait();
            Gate->DispatchSubsequents();
            Rendezvous.arrive_and_wait();
        }
    });
    for (int Round = 0; Round < Rounds; ++Round)
    {
        Gate = FGraphEvent::CreateGraphEvent();
        Rendezvous.arrive_and_wait();
        Results.push_back(FFunctionGraphTask::CreateAndDispatchWhenReady([&] { ++Count; }, Gate));
        Rendezvous.arrive_and_wait();
    }
    Completer.join();
    FTaskGraphInterface::Get().WaitUntilTasksComplete(Results, ENamedThreads::GameThread);
    Check(Count == Rounds, "registration racing completion lost a subsequent");
}

// 示例 4：命名线程亲和性、同一线程的独立队列，以及外部线程的绑定与退出。
static void TestNamedThreads()
{
    auto& Graph = FTaskGraphInterface::Get();
    const auto GameOsId = std::this_thread::get_id();
    auto GameTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]
    {
        Check(std::this_thread::get_id() == GameOsId, "GameThread affinity");
    }, nullptr, ENamedThreads::GameThread);
    Wait(GameTask);

    // GameThread_Local 与 GameThread 属于同一个 OS 线程，但使用不同队列。
    // 等待 Local 队列不会顺便执行主队列里的 MainTask。
    bool MainRan = false;
    auto MainTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&] { MainRan = true; }, nullptr, ENamedThreads::GameThread);
    auto LocalTask = FFunctionGraphTask::CreateAndDispatchWhenReady([] {}, nullptr, ENamedThreads::GameThread_Local);
    Wait(LocalTask, ENamedThreads::GameThread_Local);
    Check(!MainRan, "local queue must not pump main queue");
    Wait(MainTask);
    Check(MainRan, "main queue task");

    for (auto Named : {ENamedThreads::RHIThread, ENamedThreads::ActualRenderingThread})
    {
        std::latch Ready(1);
        // AttachToThread 只绑定当前 OS 线程，不会创建线程。
        // 命名线程必须自行处理队列；只有提交任务而没有处理循环，任务不会执行。
        std::thread Owner([&]
        {
            Graph.AttachToThread(Named);
            Ready.count_down();
            Graph.ProcessThreadUntilRequestReturn(Named);
        });
        Ready.wait();
        Graph.WakeNamedThread(Named); // 测试显式唤醒（包含合法索引 0）；正常提交任务已会唤醒。
        auto NamedTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]
        {
            Check(Graph.GetCurrentThreadIfKnown() == Named, "named thread TLS/affinity");
            Check(std::this_thread::get_id() != GameOsId, "named task ran on GameThread");
        }, nullptr, Named);
        Wait(NamedTask);
        // 在目标线程上请求处理循环返回。RequestReturn 不负责销毁 OS 线程或关闭 TaskGraph。
        // 等待返回任务完成后仍需 join，确保 Owner 真正退出后再 Shutdown。
        auto Return = FFunctionGraphTask::CreateAndDispatchWhenReady([&] { Graph.RequestReturn(Named); }, nullptr, Named);
        Wait(Return);
        Owner.join();
    }
}

// 示例 5：父任务函数已执行，但其完成事件要等子任务完成后才能完成。
// 前置依赖控制“何时开始”，DontCompleteUtil 控制“何时完成”（沿用当前接口拼写）。
static void TestDeferredCompletion()
{
    auto* Child = FHeldTask::CreateTask().ConstructAndHold(std::function<void()>([] {}), ENamedThreads::AnyThread);
    auto ChildEvent = Child->GetCompletionEvent();
    std::atomic<bool> BodyFinished{false};
    auto Parent = FFunctionGraphTask::CreateAndDispatchWhenReady(
        std::function<void(ENamedThreads::Type, const FGraphEventRef&)>([&](auto, const FGraphEventRef& Completion)
        {
            // 必须在父任务函数返回之前登记，避免阻塞工作线程等待 Child。
            Completion->DontCompleteUtil(ChildEvent);
            BodyFinished.store(true, std::memory_order_release);
        }));
    // 这里只为测试观察“父任务已登记依赖、子任务尚未放行”的状态而轮询。
    // 正常业务通过依赖或完成事件衔接，不需要照搬这个轮询。
    while (!BodyFinished.load(std::memory_order_acquire))
        std::this_thread::yield();
    Check(!Parent->IsComplete(), "DontCompleteUtil completed before held child");
    Child->Unlock();
    Wait(Parent);
    Check(ChildEvent->IsComplete(), "deferred child completion");
}

// 生命周期示例：配置线程组 -> Startup -> 绑定 GameThread -> 提交/处理/等待 -> Shutdown。
static void RunCycle(bool High, bool Background, bool FullTests)
{
    Check(!FTaskGraphInterface::IsRunning(), "graph still running before startup");
    // 在线程系统启动前配置，运行期间不要修改线程组开关。
    ENamedThreads::bHasHighPriorityThreads = High;
    ENamedThreads::bHasBackgroundThreads = Background;
    // 2 是每个启用的工作线程组的线程数；全部启用时共有 6 个工作线程。
    // <= 0 使用平台默认数量；Game/RHI/Rendering 等命名线程不包含在这个数量里。
    FTaskGraphInterface::Startup(2);
    auto& Graph = FTaskGraphInterface::Get();
    Graph.AttachToThread(ENamedThreads::GameThread);
    Check(Graph.GetNumWorkerThreads() == 2, "Startup worker count ignored");
    Check(RegisteredThreads() == 2 * (1 + int(High) + int(Background)), "worker registration count");
    Check(Graph.GetCurrentThreadIfKnown() == ENamedThreads::GameThread, "attached main TLS");
    int Callbacks = 0;
    std::function<void()> OnShutdown = [&] { ++Callbacks; };
    Graph.AddShutdownCallback(OnShutdown);

    ExampleBasicUsage();
    TestAnyThreadAndPriorities(High, Background);
    TestDependencies();
    TestNamedThreads();
    TestDeferredCompletion();
    if (FullTests)
        TestRegistrationRace();

    // 关闭前先停止外部提交，等相关任务完成，并 join 自己创建的命名线程。
    // Shutdown 不是任意在途任务的取消接口；调用后上面的 Graph 引用不再有效。
    FTaskGraphInterface::Shutdown();
    Check(!FTaskGraphInterface::IsRunning(), "shutdown singleton not cleared");
    Check(RegisteredThreads() == 0, "worker registrations leaked after shutdown");
    Check(Callbacks == 1, "shutdown callback count");
}

int main()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#ifdef _DEBUG
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    std::jthread Watchdog([](std::stop_token Stop)
    {
        const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
        while (!Stop.stop_requested())
        {
            Check(std::chrono::steady_clock::now() < Deadline, "TaskGraph test timed out (deadlock or lost task)");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // 事件池回归：空池清理、归还后清理、重复清理均应安全。
    {
        FEventPool<EEventPoolTypes::AutoReset> Pool;
        Pool.EmptyPool();
        auto* Event = Pool.GetEventFromPool();
        Event->Trigger();
        Pool.ReturnToPool(Event);
        Pool.EmptyPool();
        Pool.EmptyPool();
    }
    // 首轮预热事件池，再检查重复启动/关闭没有持续泄漏线程或事件句柄。
    RunCycle(true, true, true);
    DWORD WarmHandles = 0;
    Check(GetProcessHandleCount(GetCurrentProcess(), &WarmHandles), "query handles");
    RunCycle(true, true, false);
    RunCycle(false, true, false);
    RunCycle(true, false, false);
    RunCycle(false, false, false);
    DWORD FinalHandles = 0;
    Check(GetProcessHandleCount(GetCurrentProcess(), &FinalHandles), "query final handles");
    Check(FinalHandles <= WarmHandles + 1, "thread/event handles leaked across shutdown/restart");
    Watchdog.request_stop();
    std::cout << "PASS: real Core TaskGraph, AnyThread/priorities, dependency fan-in/out, "
                 "2000 completion-registration races, Game/local/RHI/render queues, "
                 "deferred completion, external waits, 5 startup/shutdown cycles, thread/handle cleanup\n";
}
