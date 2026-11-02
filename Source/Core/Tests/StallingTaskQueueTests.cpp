#include "Task/StallingTaskQueue.h"
#include <Windows.h>

#include <array>
#include <barrier>
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

// Same auto-reset, signal-before-wait behavior as FEventWin. No timer wakes
// the worker to repair a missed notification; a timeout fails the test.
class FTestEvent
{
public:
    FTestEvent() : Handle(CreateEventW(nullptr, FALSE, FALSE, nullptr))
    {
        Check(Handle != nullptr, "CreateEvent");
    }
    ~FTestEvent() { CloseHandle(Handle); }
    FTestEvent(const FTestEvent&) = delete;
    FTestEvent& operator=(const FTestEvent&) = delete;
    void Trigger() { Check(SetEvent(Handle) != FALSE, "SetEvent"); }
    void Wait() { Check(WaitForSingleObject(Handle, 10000) == WAIT_OBJECT_0, "missed wakeup"); }
private:
    HANDLE Handle;
};

static void TestPriorityAndGrowth()
{
    FStallingTaskQueue<int, 3, 2> Queue;
    Check(Queue.IsLockFree(), "scheduler queue atomics are not lock-free");
    std::array<int, 9> Items{};
    for (int Priority = 2; Priority >= 0; --Priority)
        for (int Index = 0; Index < 3; ++Index)
            Check(Queue.Push(&Items[Priority * 3 + Index], Priority) == -1, "no worker should be idle");
    Check(Queue.GetAllocationFallbackCount() > 0, "small node pool must expand");
    for (int Index = 0; Index < 9; ++Index)
        Check(Queue.Pop() == &Items[Index], "priority/FIFO ordering or growth lost a task");
    Check(Queue.Pop() == nullptr, "queue must be empty");

    FStallingTaskQueue<int, 2, 32> Preallocated;
    for (int Round = 0; Round < 64; ++Round)
    {
        for (int Index = 0; Index < 9; ++Index)
            Preallocated.Push(&Items[Index], 1);
        for (int Index = 0; Index < 9; ++Index)
            Check(Preallocated.Pop() == &Items[Index], "preallocated pool reuse");
    }
    Check(Preallocated.GetAllocationFallbackCount() == 0, "preallocated path must not request allocation");
}

static void TestWakeClaims()
{
    FStallingTaskQueue<int, 2> Queue;
    constexpr std::array<int, 8> Ids{0, 1, 2, 3, 4, 5, 6, 31};
    std::array<int, 8> Items{};
    std::array<int, 8> Wakes{};
    std::barrier Start(8);
    std::vector<std::thread> Producers;
    for (int Id : Ids)
        Check(Queue.Pop(Id, true) == nullptr, "register idle worker");
    for (int Index = 0; Index < 8; ++Index)
    {
        Producers.emplace_back([&, Index]
        {
            Start.arrive_and_wait();
            Wakes[Index] = Queue.Push(&Items[Index], Index % 2);
        });
    }
    for (auto& Producer : Producers)
        Producer.join();
    uint64 Claimed = 0;
    for (int Id : Wakes)
    {
        Check(Id >= 0 && Id < 32, "producer must claim an idle worker");
        const uint64 Bit = uint64(1) << Id;
        Check((Claimed & Bit) == 0, "two producers claimed the same idle worker");
        Claimed |= Bit;
    }
    Check(Claimed == 0x8000007full, "wrong idle workers claimed");
    for (int Index = 0; Index < 8; ++Index)
        Check(Queue.Pop() != nullptr, "wake claim lost a task");

    // A direct WakeUp call does not pass through Push.
    Check(Queue.Pop(31, true) == nullptr, "idle before explicit wake");
    Check(Queue.Pop(31, false) == nullptr, "poll after explicit wake");
    Check(Queue.Push(&Items[0], 0) == -1, "explicitly woken worker retained a stale idle bit");
    Check(Queue.Pop(31, false) == &Items[0], "task after explicit wake");

    FTestEvent Event;
    Check(Queue.Pop(0, true) == nullptr, "idle before signal-before-wait");
    Check(Queue.Push(&Items[1], 0) == 0, "select worker before it enters Wait");
    Event.Trigger();
    Event.Wait();
    Check(Queue.Pop(0, false) == &Items[1], "signal-before-wait lost task");
}

static void TestEmptyToSleepRace()
{
    constexpr int Rounds = 5000;
    FStallingTaskQueue<int, 2> Queue;
    FTestEvent Event;
    std::barrier Rendezvous(2);
    std::vector<int> Items(Rounds);
    std::thread Consumer([&]
    {
        for (int Round = 0; Round < Rounds; ++Round)
        {
            Rendezvous.arrive_and_wait();
            int* Task;
            while ((Task = Queue.Pop(0, true)) == nullptr)
            {
                if (Round % 2 == 0)
                    std::this_thread::yield();
                Event.Wait();
            }
            Check(Task == &Items[Round], "race returned wrong task");
            Rendezvous.arrive_and_wait();
        }
    });
    for (int Round = 0; Round < Rounds; ++Round)
    {
        Rendezvous.arrive_and_wait();
        if (Round % 2 != 0)
            std::this_thread::yield();
        const int Id = Queue.Push(&Items[Round], Round % 2);
        Check(Id == -1 || Id == 0, "wrong wake target in single-consumer queue");
        if (Id == 0)
            Event.Trigger();
        // No next submission can mask a lost wakeup in this round.
        Rendezvous.arrive_and_wait();
    }
    Consumer.join();
}

static void TestSleepingWorkers()
{
    constexpr int NumWorkers = 4;
    constexpr int NumProducers = 4;
    constexpr int Bursts = 10;
    constexpr int PerProducerPerBurst = 2500;
    constexpr int BurstSize = NumProducers * PerProducerPerBurst;
    constexpr int Total = Bursts * BurstSize;
    struct FTask { int Id; };
    FStallingTaskQueue<FTask, 2, 32> Queue;
    std::vector<FTask> Items(Total);
    std::vector<std::atomic<int>> Seen(Total);
    for (int Index = 0; Index < Total; ++Index)
    {
        Items[Index].Id = Index;
        Seen[Index].store(0);
    }
    std::array<FTestEvent, NumWorkers> Events;
    std::atomic<bool> Stop{false};
    std::atomic<int> Consumed{0};
    std::atomic<int> Sleeping{0};
    std::vector<std::thread> Workers;
    for (int Id = 0; Id < NumWorkers; ++Id)
    {
        Workers.emplace_back([&, Id]
        {
            while (!Stop.load(std::memory_order_acquire))
            {
                if (FTask* Task = Queue.Pop(Id, true))
                {
                    Check(Task->Id >= 0 && Task->Id < Total, "invalid task");
                    Check(Seen[Task->Id].fetch_add(1) == 0, "duplicate task execution");
                    Consumed.fetch_add(1, std::memory_order_release);
                }
                else
                {
                    Sleeping.fetch_add(1);
                    Events[Id].Wait();
                    Sleeping.fetch_sub(1);
                }
            }
        });
    }

    auto WaitFor = [&](int ExpectedCount)
    {
        const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (Consumed.load(std::memory_order_acquire) != ExpectedCount || Sleeping.load() != NumWorkers)
        {
            Check(std::chrono::steady_clock::now() < Deadline, "workers failed to drain tasks and sleep");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };
    WaitFor(0);
    for (int Burst = 0; Burst < Bursts; ++Burst)
    {
        std::vector<std::thread> Producers;
        for (int Producer = 0; Producer < NumProducers; ++Producer)
        {
            Producers.emplace_back([&, Producer, Burst]
            {
                for (int Index = 0; Index < PerProducerPerBurst; ++Index)
                {
                    const int ItemIndex = Burst * BurstSize + Producer * PerProducerPerBurst + Index;
                    const int Id = Queue.Push(&Items[ItemIndex], Index % 2);
                    Check(Id >= -1 && Id < NumWorkers, "invalid worker selected");
                    if (Id >= 0)
                        Events[Id].Trigger();
                }
            });
        }
        for (auto& Producer : Producers)
            Producer.join();
        WaitFor((Burst + 1) * BurstSize);
    }
    Stop.store(true, std::memory_order_release);
    for (auto& Event : Events)
        Event.Trigger();
    for (auto& Worker : Workers)
        Worker.join();
    for (const auto& Count : Seen)
        Check(Count.load() == 1, "task missing after bursts");
    Check(Queue.Pop(0, false) == nullptr, "queue not drained");
}

int main()
{
    TestPriorityAndGrowth();
    TestWakeClaims();
    TestEmptyToSleepRace();
    TestSleepingWorkers();
    std::cout << "PASS: priority/FIFO, pool reuse/growth, wake ownership, explicit wake, "
                 "5000 empty/sleep races, 100000 tasks across 10 sleeping-worker bursts\n";
}
