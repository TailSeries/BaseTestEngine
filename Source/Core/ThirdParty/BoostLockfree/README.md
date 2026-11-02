# Boost.Lockfree for Core

本目录集成 Boost **1.89.0** 的 Lockfree 头文件模块及 `bcp` 提取出的传递依赖。
不需要安装 Boost，不需要链接 Boost 的 `.lib` / `.dll`，CMake 构建时不会联网。
当前采用上游默认的 `std::atomic` 后端；不要定义 `BOOST_LOCKFREE_FORCE_BOOST_ATOMIC`，
切换到 Boost.Atomic 后端可能需要另外集成其二进制支持。

## 使用

Core 通过 `PUBLIC` 依赖传播头文件搜索路径，链接 Core 的目标可以直接使用：

```cpp
#include <boost/lockfree/queue.hpp>

// 构造时预分配 1024 个内部节点；任务本身的生命周期由调用方管理。
boost::lockfree::queue<FBaseGraphTask*> Queue(1024);

// 必须检查入队结果；push 在预分配池耗尽时可能申请内存。
bool Accepted = Queue.push(Task);
FBaseGraphTask* Next = nullptr;
if (Queue.pop(Next))
{
    // 使用 Next。
}
```

其他独立目标也可以只链接 CMake 接口目标 `CoreThirdParty::BoostLockfree`。
可用容器包括 `queue`、`stack`、`spsc_queue` 和 `spsc_value`；两个 SPSC 类型仅支持
一个生产者和一个消费者。

`bounded_push` 不扩容，内部节点不足时返回 false。不要忽略失败或把固定容量队列的
满队列处理改成所有 worker 原地自旋。`is_lock_free()` 检查队列原子操作的实现，
不保证动态内存分配也是无锁的。队列构造和析构必须在并发访问之外完成。

Core 的 `FStallingTaskQueue` 已使用 `boost::lockfree::queue<T*>`，每个任务优先级
默认预分配 1024 个节点。正常入队先调用 `bounded_push`，节点不足时使用 `push`
允许扩容；`GetAllocationFallbackCount()` 记录进入允许分配路径的次数（不等于实际
内存分配次数，因为竞争期间其他消费者可能刚好归还节点）。第三个模板参数可以调整
每个优先级的初始容量。队列仅保存指针，不销毁任务。

任务优先级越小越先被检查；同优先级维持 FIFO 出队语义，并行执行的完成顺序不保证。
调度层继续使用“代数 + 空闲位图”的 CAS 协议，Push 发布任务后认领一个空闲线程，
调用方触发该线程的 Event。Pop 登记空闲前会验证期间没有状态变化；直接 WakeUp
的线程再次 Pop 时会撤销残留空闲位。每个消费者必须使用本队列内唯一的线程编号。

## 来源和裁剪

- 官方发布页：https://www.boost.org/releases/1.89.0/
- 官方源码包：https://archives.boost.io/release/1.89.0/source/boost_1_89_0.tar.bz2
- SHA-256：`85a33fa22621b4f314f8e85e1a5e2a9363d22e4f4992925d4bb3bc631b5a0c7a`
- 许可证：本目录的 `LICENSE_1_0.txt`；上游文件中的版权声明保留。
- 所有 vendored 头文件未经修改。`MANIFEST.sha256` 记录逐文件摘要。

使用该版本源码自带的 `bcp`，在源码根目录执行：

```text
bcp --boost=. boost/lockfree boost/version.hpp <subset-directory>
```

将输出中的 `boost/` 复制到本目录的 `include/boost/`，并复制源码根目录的许可证。
`bcp` 保守地保留不同编译器、平台和条件编译分支的依赖，因此会带入 Config、
Parameter、MPL、Preprocessor 等公共头文件；这些是依赖闭包，不是完整 Boost。
头文件共 2045 个，约 19.5 MiB。上游文档、测试、工具及二进制库不随项目集成。

升级时重新裁剪、更新版本和摘要，并执行下面的验证；不要手工修改上游头文件。

## 验证

在仓库根目录执行（使用当前机器的默认 CMake 编译器）：

```powershell
cmake -S Source/Core/Tests -B out/build/core-lockfree-tests
cmake --build out/build/core-lockfree-tests --config Debug
ctest --test-dir out/build/core-lockfree-tests -C Debug --output-on-failure
cmake --build out/build/core-lockfree-tests --config Release
ctest --test-dir out/build/core-lockfree-tests -C Release --output-on-failure
```

验证覆盖容量边界、FIFO/LIFO、SPSC 环绕，以及 4 个生产者、4 个消费者传递
100000 个任务指针时恰好消费一次。Windows 下另有 `CoreStallingQueueTests`，直接使用
调度队列和 Win32 auto-reset Event，覆盖优先级/FIFO、扩容、节点复用、并发认领唤醒
目标、直接唤醒、先发信号后等待、5000 轮判空/休眠竞争，以及 10 批共 100000 个任务
在 worker 全部休眠后重新提交。等待超时会失败，不通过周期性唤醒掩盖通知丢失。
上述队列测试不依赖引擎启动，也不编入 Core DLL。Windows 下另有 `CoreTaskGraphTests`，
链接真实 Core DLL，覆盖任务图基本使用、依赖注册竞争、具名线程、优先级组和反复启停；
详见 `Source/Core/Tests/README.md`。
