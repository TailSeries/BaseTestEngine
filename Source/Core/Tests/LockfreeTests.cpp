#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/lockfree/spsc_value.hpp>
#include <boost/lockfree/stack.hpp>
#include <boost/version.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

static void Check(bool Condition, const char* Message)
{
    if (!Condition)
    {
        std::cerr << "FAIL: " << Message << '\n';
        std::abort();
    }
}

static void TestContainers()
{
    boost::lockfree::queue<int, boost::lockfree::capacity<32>> Queue;
    Check(Queue.is_lock_free(), "bounded queue atomics are not lock-free");
    for (int Index = 0; Index < 32; ++Index)
        Check(Queue.bounded_push(Index), "bounded push");
    Check(!Queue.bounded_push(32), "full queue must reject push");
    int Value = -1;
    for (int Index = 0; Index < 32; ++Index)
        Check(Queue.pop(Value) && Value == Index, "queue FIFO order");
    Check(!Queue.pop(Value), "empty queue must reject pop");
    Check(Queue.bounded_push(42) && Queue.pop(Value) && Value == 42, "queue reuse");

    boost::lockfree::stack<int, boost::lockfree::capacity<8>> Stack;
    Check(Stack.push(1) && Stack.push(2), "stack push");
    Check(Stack.pop(Value) && Value == 2, "stack LIFO order");
    Check(Stack.pop(Value) && Value == 1 && !Stack.pop(Value), "stack drain");

    boost::lockfree::spsc_queue<int, boost::lockfree::capacity<8>> Spsc;
    for (int Round = 0; Round < 100; ++Round)
    {
        for (int Index = 0; Index < 8; ++Index)
            Check(Spsc.push(Index), "SPSC push/wraparound");
        Check(!Spsc.push(8), "SPSC full");
        for (int Index = 0; Index < 8; ++Index)
            Check(Spsc.pop(Value) && Value == Index, "SPSC FIFO/wraparound");
    }

    boost::lockfree::spsc_value<int> Latest;
    Check(!Latest.read(Value), "SPSC value starts empty");
    Latest.write(1);
    Latest.write(2);
    Check(Latest.read(Value) && Value == 2, "SPSC value retains latest write");
    Check(!Latest.read(Value), "SPSC value consumed once");
}

static void TestConcurrentPointers()
{
    constexpr int NumProducers = 4;
    constexpr int NumConsumers = 4;
    constexpr int PerProducer = 25000;
    constexpr int Total = NumProducers * PerProducer;
    struct Task { int Id; };
    std::vector<Task> Tasks(Total);
    std::vector<std::atomic<int>> Seen(Total);
    for (int Index = 0; Index < Total; ++Index)
    {
        Tasks[Index].Id = Index;
        Seen[Index].store(0);
    }

    // The initial pool is deliberately smaller than the workload: exercise
    // dynamic growth and node reuse as well as concurrent pointer transport.
    boost::lockfree::queue<Task*> Queue(128);
    Check(Queue.is_lock_free(), "dynamic pointer queue atomics are not lock-free");
    std::atomic<bool> Start{false};
    std::atomic<int> Consumed{0};
    const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    std::vector<std::thread> Workers;
    for (int Consumer = 0; Consumer < NumConsumers; ++Consumer)
    {
        Workers.emplace_back([&]
        {
            while (!Start.load(std::memory_order_acquire))
                std::this_thread::yield();
            while (Consumed.load(std::memory_order_relaxed) < Total)
            {
                Task* Item = nullptr;
                if (Queue.pop(Item))
                {
                    Check(Item != nullptr && Item->Id >= 0 && Item->Id < Total, "invalid task pointer");
                    Check(Seen[Item->Id].fetch_add(1) == 0, "duplicate task");
                    Consumed.fetch_add(1, std::memory_order_relaxed);
                }
                else
                {
                    Check(std::chrono::steady_clock::now() < Deadline, "MPMC timeout/lost task");
                    std::this_thread::yield();
                }
            }
        });
    }
    for (int Producer = 0; Producer < NumProducers; ++Producer)
    {
        Workers.emplace_back([&, Producer]
        {
            while (!Start.load(std::memory_order_acquire))
                std::this_thread::yield();
            for (int Index = 0; Index < PerProducer; ++Index)
                Check(Queue.push(&Tasks[Producer * PerProducer + Index]), "MPMC enqueue failed");
        });
    }
    Start.store(true, std::memory_order_release);
    for (auto& Worker : Workers)
        Worker.join();
    for (const auto& Count : Seen)
        Check(Count.load() == 1, "task not consumed exactly once");
    Task* Remaining = nullptr;
    Check(!Queue.pop(Remaining), "queue not empty after drain");
}

int main()
{
    static_assert(BOOST_VERSION == 108900, "Unexpected vendored Boost version");
    TestContainers();
    TestConcurrentPointers();
    std::cout << "PASS: Boost " << BOOST_LIB_VERSION
              << "; queue/stack/SPSC; 4 producers + 4 consumers, 100000 pointers exactly once\n";
}
