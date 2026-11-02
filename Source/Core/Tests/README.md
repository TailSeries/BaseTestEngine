# Core 日志、线程与任务图验证

从仓库根目录运行，Windows 下会构建并链接真实的 Core DLL，无需启动渲染器：

```powershell
cmake -S Source/Core/Tests -B out/build/core-lockfree-tests
cmake --build out/build/core-lockfree-tests --config Debug -j 8
ctest --test-dir out/build/core-lockfree-tests -C Debug --output-on-failure
cmake --build out/build/core-lockfree-tests --config Release -j 8
ctest --test-dir out/build/core-lockfree-tests -C Release --output-on-failure
```

- `CoreLockfreeTests`：Boost 容器基本行为，4 生产者/4 消费者，100000 个指针恰好消费一次。
- `CoreLogTests`：格式化与中文、分类/全局级别过滤、过滤后不求值参数、跨翻译单元与
  Core DLL 分类、旧 printf 日志兼容、4 线程共 1000 条日志、文件滚动和保留数量、
  初始化失败保留原配置、关闭后重新初始化，以及子进程中 Fatal 刷新落盘并终止。
  输出保存在测试工作目录的 `log-tests-<进程号>-<时间戳>/` 中；公共接口和示例见 `../Logging/Log.h`。
- `CoreStallingQueueTests`：调度队列的优先级/FIFO、节点池扩容与复用、原子认领唤醒目标、
  5000 轮判空/休眠竞争，以及 10 批共 100000 个任务的实际 Event 唤醒。
- `CoreTaskGraphTests`：真实 TaskGraph 的 AnyThread 提交和等待、工作线程 TLS 与管理器登记、
  前置依赖和 fan-out/fan-in、Hold/Unlock、已经完成的前置事件、2000 轮依赖注册与事件完成竞争、
  GameThread 主/本地队列、RHI/Render 具名线程、DontCompleteUtil 延迟完成、外部线程等待，
  以及 5 轮启停和句柄/线程登记清理。涵盖高优先级组与背景组的四种启用组合。

TaskGraph 测试带有 45 秒 watchdog，CTest 另设 60 秒超时；死锁不会被无限等待掩盖。
这些是基本功能和针对性并发回归测试，不覆盖内存耗尽、操作系统线程创建失败及完整引擎负载。

## 最小使用方式

调用方目标链接 `Core`，然后：

```cpp
#include "Task/FunctionGraphTask.h"

// 每个启用的 worker 优先级组分配 2 个线程。
// NumThreads > 0 时最多为 26；<= 0 使用平台默认值，不表示关闭多线程。
FTaskGraphInterface::Startup(2);
auto& Graph = FTaskGraphInterface::Get();
Graph.AttachToThread(ENamedThreads::GameThread);

{
    int Value = 0;
    auto Work = FFunctionGraphTask::CreateAndDispatchWhenReady([&] {
        Value = 42;
    });
    auto FollowUp = FFunctionGraphTask::CreateAndDispatchWhenReady([&] {
        // 依赖 Work，并在 GameThread 执行。
        assert(Value == 42);
    }, Work, ENamedThreads::GameThread);

    Graph.WaitUntilTaskCompletes(FollowUp, ENamedThreads::GameThread);
}

// 先停止所有外部提交，等待任务完成，并让外部具名线程退出和 join。
FTaskGraphInterface::Shutdown();
```

`GameThread` / `RHIThread` / `ActualRenderingThread` 是执行目标标识；TaskGraph 不会替调用方
创建这些外部线程。每个实际线程需要先 `AttachToThread`，再运行对应的任务处理循环。
优先级组开关应在 Startup 之前设置；Shutdown 完成后可重新 Startup 和 Attach。
