#pragma once

#include "core/profile.hpp"
#include "core/log.hpp"

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

namespace cube::jobs {

enum class Priority : std::uint8_t { High, Normal, Low };

class JobSystem {
public:
    using JobFn = void(*)(void*);

    struct Counter;

    struct Job {
        JobFn fn{};
        void* data{};
        Counter* counter{};
        Counter* dependency{};
        const char* name{};
    };

    struct Stats {
        std::uint32_t worker_count{};
        std::uint32_t pending_high{};
        std::uint32_t pending_normal{};
        std::uint32_t pending_low{};
        std::uint32_t stall_warnings{};
        std::array<float, 64> worker_utilization{};
    };

    struct Config {
        std::uint32_t thread_count{};
        std::uint32_t queue_capacity{4096};
        std::uint32_t stall_warn_ms{100};
    };

    JobSystem() = default;
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    bool init(const Config& cfg = {});
    void shutdown();

    void init_counter(Counter& c, std::int32_t initial = 0);
    void submit(JobFn fn, void* data, Priority prio = Priority::Normal, Counter* counter = nullptr, Counter* dependency = nullptr, const char* name = nullptr);
    void submit_batch(const Job* jobs, std::size_t n, Priority prio = Priority::Normal, Counter* counter = nullptr, Counter* dependency = nullptr);
    void wait(Counter& c);

    Stats snapshot_stats();

private:
    struct alignas(64) WorkerCounters {
        std::atomic<std::uint64_t> busy_ns{0};
        std::atomic<std::uint64_t> total_ns{0};

        WorkerCounters() = default;
        WorkerCounters(const WorkerCounters& o) {
            busy_ns.store(o.busy_ns.load(std::memory_order_relaxed), std::memory_order_relaxed);
            total_ns.store(o.total_ns.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        WorkerCounters& operator=(const WorkerCounters& o) {
            busy_ns.store(o.busy_ns.load(std::memory_order_relaxed), std::memory_order_relaxed);
            total_ns.store(o.total_ns.load(std::memory_order_relaxed), std::memory_order_relaxed);
            return *this;
        }
        WorkerCounters(WorkerCounters&& o) noexcept : WorkerCounters(o) {}
        WorkerCounters& operator=(WorkerCounters&& o) noexcept { return (*this = o); }
    };

    template <class T>
    class MpmcQueue {
    public:
        bool init(std::uint32_t capacity_pow2);
        bool enqueue(const T& v);
        bool dequeue(T& out);
        void reset();
        std::uint32_t capacity() const { return mask_ ? (mask_ + 1u) : 0u; }

    private:
        struct alignas(64) Cell {
            std::atomic<std::size_t> seq{};
            T data{};

            Cell() = default;
            Cell(const Cell& o) {
                seq.store(o.seq.load(std::memory_order_relaxed), std::memory_order_relaxed);
                data = o.data;
            }
            Cell& operator=(const Cell& o) {
                seq.store(o.seq.load(std::memory_order_relaxed), std::memory_order_relaxed);
                data = o.data;
                return *this;
            }
            Cell(Cell&& o) noexcept : Cell(o) {}
            Cell& operator=(Cell&& o) noexcept { return (*this = o); }
        };

        std::vector<Cell> buf_;
        std::size_t mask_{};
        alignas(64) std::atomic<std::size_t> head_{0};
        alignas(64) std::atomic<std::size_t> tail_{0};
    };

    struct Continuation {
        Job job{};
        Priority prio{};
        Continuation* next{};
    };

public:
    struct Counter {
        JobSystem* js{};
        std::atomic<std::int32_t> remaining{0};
        std::atomic<Continuation*> conts{nullptr};
        std::mutex wait_m;
        std::condition_variable wait_cv;

        void add(std::int32_t n);
        void done();
        bool is_done() const { return remaining.load(std::memory_order_acquire) <= 0; }
    };

private:
    bool enqueue_job(const Job& j, Priority p);
    bool try_dequeue(Job& out);
    bool try_run_one();
    void wake_one();
    void worker_main(std::uint32_t worker_index);
    void schedule_continuations(Counter& c);

    Config cfg_{};
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};

    MpmcQueue<Job> q_high_;
    MpmcQueue<Job> q_norm_;
    MpmcQueue<Job> q_low_;

    std::atomic<std::uint32_t> pending_high_{0};
    std::atomic<std::uint32_t> pending_norm_{0};
    std::atomic<std::uint32_t> pending_low_{0};

    std::atomic<std::uint32_t> stall_warnings_{0};

    std::mutex wake_m_;
    std::condition_variable wake_cv_;

    std::vector<std::thread> workers_;
    std::vector<WorkerCounters> worker_counters_;

    static thread_local bool tls_is_worker_;
};

}

