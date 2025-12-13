#include "core/job_system.hpp"

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>
#include <mutex>

static int jfail(int code, const char* what) {
    std::fprintf(stderr, "cube_tests: FAIL(%d): %s\n", code, what);
    return code;
}

namespace {

struct IncCtx { std::atomic<int>* v; };
static void inc_job(void* p) { static_cast<IncCtx*>(p)->v->fetch_add(1, std::memory_order_relaxed); }

struct OrderCtx { std::mutex* m; std::vector<int>* out; int id; };
static void push_order(void* p) {
    auto* c = static_cast<OrderCtx*>(p);
    std::scoped_lock lk(*c->m);
    c->out->push_back(c->id);
}

struct WaitCtx { cube::jobs::JobSystem* js; cube::jobs::JobSystem::Counter* dep; std::atomic<int>* out; };
static void wait_job(void* p) {
    auto* c = static_cast<WaitCtx*>(p);
    c->js->wait(*c->dep);
    c->out->store(1, std::memory_order_relaxed);
}

static void sleep_job(void* p) {
    (void)p;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

}

int run_job_tests() {
    using cube::jobs::JobSystem;
    using cube::jobs::Priority;

    {
        JobSystem js;
        if (!js.init(JobSystem::Config{.thread_count = 2, .queue_capacity = 1024, .stall_warn_ms = 100})) return jfail(301, "JobSystem init");
        JobSystem::Counter c;
        js.init_counter(c);
        std::atomic<int> v{0};
        IncCtx ctx{&v};
        js.submit(&inc_job, &ctx, Priority::Normal, &c, nullptr, "inc");
        js.wait(c);
        js.shutdown();
        if (v.load(std::memory_order_relaxed) != 1) return jfail(302, "submit single job");
    }

    {
        JobSystem js;
        if (!js.init(JobSystem::Config{.thread_count = 4, .queue_capacity = 4096, .stall_warn_ms = 100})) return jfail(303, "JobSystem init (batch)");
        JobSystem::Counter c;
        js.init_counter(c);
        std::atomic<int> v{0};
        IncCtx ctx{&v};
        std::vector<JobSystem::Job> jobs;
        jobs.resize(1000);
        for (auto& j : jobs) j = JobSystem::Job{&inc_job, &ctx, nullptr, nullptr, "inc"};
        js.submit_batch(jobs.data(), jobs.size(), Priority::Normal, &c, nullptr);
        js.wait(c);
        js.shutdown();
        if (v.load(std::memory_order_relaxed) != (int)jobs.size()) return jfail(304, "submit batch jobs");
    }

    {
        JobSystem js;
        if (!js.init(JobSystem::Config{.thread_count = 1, .queue_capacity = 256, .stall_warn_ms = 100})) return jfail(305, "JobSystem init (priority)");
        JobSystem::Counter gate;
        js.init_counter(gate, 1);
        JobSystem::Counter done;
        js.init_counter(done);
        std::mutex m;
        std::vector<int> order;
        OrderCtx low{&m, &order, 0};
        OrderCtx high{&m, &order, 1};
        js.submit(&push_order, &low, Priority::Low, &done, &gate, "low");
        js.submit(&push_order, &high, Priority::High, &done, &gate, "high");
        gate.done();
        js.wait(done);
        js.shutdown();
        if (order.size() != 2) return jfail(306, "priority order size");
        if (order[0] != 1) return jfail(307, "high priority runs first");
    }

    {
        JobSystem js;
        if (!js.init(JobSystem::Config{.thread_count = 2, .queue_capacity = 2048, .stall_warn_ms = 100})) return jfail(308, "JobSystem init (deps chain)");
        JobSystem::Counter a;
        js.init_counter(a);
        JobSystem::Counter b;
        js.init_counter(b);
        std::atomic<int> v{0};

        auto job1 = +[](void* p) { static_cast<std::atomic<int>*>(p)->store(1, std::memory_order_relaxed); };
        auto job2 = +[](void* p) { static_cast<std::atomic<int>*>(p)->fetch_add(1, std::memory_order_relaxed); };

        js.submit(job1, &v, Priority::Normal, &a, nullptr, "a");
        js.submit(job2, &v, Priority::Normal, &b, &a, "b_dep_a");
        js.wait(b);
        js.shutdown();
        if (v.load(std::memory_order_relaxed) != 2) return jfail(309, "dependency chain");
    }

    {
        JobSystem js;
        if (!js.init(JobSystem::Config{.thread_count = 4, .queue_capacity = 8192, .stall_warn_ms = 100})) return jfail(310, "JobSystem init (fan)");
        JobSystem::Counter fan;
        js.init_counter(fan);
        std::atomic<int> v{0};
        IncCtx ctx{&v};
        constexpr int N = 5000;
        for (int i = 0; i < N; ++i) js.submit(&inc_job, &ctx, Priority::Normal, &fan, nullptr, "fan");
        js.wait(fan);

        JobSystem::Counter final;
        js.init_counter(final);
        std::atomic<int> fin{0};
        WaitCtx w{&js, &fan, &fin};
        js.submit(+[](void* p) { auto* c = static_cast<WaitCtx*>(p); c->out->store(2, std::memory_order_relaxed); }, &w, Priority::Normal, &final, &fan, "final_dep_fan");
        js.wait(final);
        js.shutdown();
        if (v.load(std::memory_order_relaxed) != N) return jfail(311, "fan-out/fan-in count");
        if (fin.load(std::memory_order_relaxed) != 2) return jfail(312, "fan-in continuation ran");
    }

    {
        JobSystem js;
        if (!js.init(JobSystem::Config{.thread_count = 1, .queue_capacity = 1024, .stall_warn_ms = 100})) return jfail(313, "JobSystem init (wait help)");
        JobSystem::Counter b;
        js.init_counter(b);
        std::atomic<int> waited{0};
        WaitCtx w{&js, &b, &waited};

        JobSystem::Counter done;
        js.init_counter(done);

        js.submit(&wait_job, &w, Priority::Normal, &done, nullptr, "wait");
        js.submit(+[](void* p) { static_cast<std::atomic<int>*>(p)->store(7, std::memory_order_relaxed); }, &waited, Priority::Normal, &b, nullptr, "signal");
        js.wait(done);
        js.shutdown();
        if (waited.load(std::memory_order_relaxed) == 0) return jfail(314, "worker wait helped progress");
    }

    {
        JobSystem js;
        if (!js.init(JobSystem::Config{.thread_count = 4, .queue_capacity = 16384, .stall_warn_ms = 100})) return jfail(315, "JobSystem init (stress)");
        JobSystem::Counter c;
        js.init_counter(c);
        std::atomic<int> v{0};
        IncCtx ctx{&v};
        constexpr int producers = 4;
        constexpr int per = 5000;
        std::vector<std::thread> ts;
        ts.reserve(producers);
        for (int p = 0; p < producers; ++p) {
            ts.emplace_back([&] {
                for (int i = 0; i < per; ++i) js.submit(&inc_job, &ctx, Priority::Normal, &c, nullptr, "inc");
            });
        }
        for (auto& t : ts) t.join();
        js.wait(c);
        js.shutdown();
        if (v.load(std::memory_order_relaxed) != producers * per) return jfail(316, "multi-producer stress");
    }

    {
        JobSystem js;
        if (!js.init(JobSystem::Config{.thread_count = 2, .queue_capacity = 256, .stall_warn_ms = 1})) return jfail(317, "JobSystem init (stall)");
        JobSystem::Counter c;
        js.init_counter(c);
        js.submit(&sleep_job, nullptr, Priority::Normal, &c, nullptr, "sleep");
        js.wait(c);
        auto st = js.snapshot_stats();
        js.shutdown();
        if (st.stall_warnings == 0) return jfail(318, "stall detection");
    }

    return 0;
}

