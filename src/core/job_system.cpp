#include "job_system.hpp"

#include <algorithm>

namespace cube::jobs {

thread_local bool JobSystem::tls_is_worker_ = false;

static std::uint32_t round_down_pow2(std::uint32_t v) {
    if (v < 2) return 0;
    std::uint32_t p = 1;
    while ((p << 1) && (p << 1) <= v) p <<= 1;
    return p;
}

template <class T>
bool JobSystem::MpmcQueue<T>::init(std::uint32_t capacity_pow2) {
    const std::uint32_t cap = round_down_pow2(capacity_pow2);
    if (cap == 0) return false;
    buf_.clear();
    buf_.resize(cap);
    mask_ = static_cast<std::size_t>(cap - 1u);
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
    for (std::size_t i = 0; i < buf_.size(); ++i) buf_[i].seq.store(i, std::memory_order_relaxed);
    return true;
}

template <class T>
void JobSystem::MpmcQueue<T>::reset() {
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
    for (std::size_t i = 0; i < buf_.size(); ++i) buf_[i].seq.store(i, std::memory_order_relaxed);
}

template <class T>
bool JobSystem::MpmcQueue<T>::enqueue(const T& v) {
    Cell* cell = nullptr;
    std::size_t pos = tail_.load(std::memory_order_relaxed);
    for (;;) {
        cell = &buf_[pos & mask_];
        std::size_t seq = cell->seq.load(std::memory_order_acquire);
        const std::intptr_t dif = (std::intptr_t)seq - (std::intptr_t)pos;
        if (dif == 0) {
            if (tail_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) break;
        } else if (dif < 0) {
            return false;
        } else {
            pos = tail_.load(std::memory_order_relaxed);
        }
    }
    cell->data = v;
    cell->seq.store(pos + 1, std::memory_order_release);
    return true;
}

template <class T>
bool JobSystem::MpmcQueue<T>::dequeue(T& out) {
    Cell* cell = nullptr;
    std::size_t pos = head_.load(std::memory_order_relaxed);
    for (;;) {
        cell = &buf_[pos & mask_];
        std::size_t seq = cell->seq.load(std::memory_order_acquire);
        const std::intptr_t dif = (std::intptr_t)seq - (std::intptr_t)(pos + 1);
        if (dif == 0) {
            if (head_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) break;
        } else if (dif < 0) {
            return false;
        } else {
            pos = head_.load(std::memory_order_relaxed);
        }
    }
    out = cell->data;
    cell->seq.store(pos + mask_ + 1, std::memory_order_release);
    return true;
}

template class JobSystem::MpmcQueue<JobSystem::Job>;

void JobSystem::Counter::add(std::int32_t n) {
    if (n <= 0) return;
    remaining.fetch_add(n, std::memory_order_relaxed);
}

void JobSystem::schedule_continuations(Counter& c) {
    Continuation* list = c.conts.exchange(nullptr, std::memory_order_acq_rel);
    while (list) {
        Continuation* n = list->next;
        enqueue_job(list->job, list->prio);
        delete list;
        list = n;
    }
}

void JobSystem::Counter::done() {
    const std::int32_t prev = remaining.fetch_sub(1, std::memory_order_acq_rel);
    if (prev != 1) return;
    if (js) js->schedule_continuations(*this);
    {
        std::scoped_lock lk(wait_m);
    }
    wait_cv.notify_all();
}

bool JobSystem::init(const Config& cfg) {
    if (running_.load(std::memory_order_acquire)) return true;
    cfg_ = cfg;
    if (cfg_.queue_capacity < 64) cfg_.queue_capacity = 64;
    const std::uint32_t cap = round_down_pow2(cfg_.queue_capacity);
    if (!q_high_.init(cap) || !q_norm_.init(cap) || !q_low_.init(cap)) return false;

    const std::uint32_t hc = std::max(1u, std::thread::hardware_concurrency());
    std::uint32_t tc = cfg_.thread_count ? cfg_.thread_count : (hc > 2 ? (hc - 2) : 1u);
    tc = std::max(1u, std::min(tc, 64u));

    stop_.store(false, std::memory_order_release);
    pending_high_.store(0, std::memory_order_relaxed);
    pending_norm_.store(0, std::memory_order_relaxed);
    pending_low_.store(0, std::memory_order_relaxed);
    stall_warnings_.store(0, std::memory_order_relaxed);

    worker_counters_.clear();
    worker_counters_.resize(tc);
    workers_.clear();
    workers_.reserve(tc);
    for (std::uint32_t i = 0; i < tc; ++i) workers_.emplace_back([this, i] { worker_main(i); });

    running_.store(true, std::memory_order_release);
    return true;
}

void JobSystem::shutdown() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) return;
    stop_.store(true, std::memory_order_release);
    wake_cv_.notify_all();
    for (auto& t : workers_) if (t.joinable()) t.join();
    workers_.clear();
    worker_counters_.clear();
    q_high_.reset();
    q_norm_.reset();
    q_low_.reset();
}

void JobSystem::init_counter(Counter& c, std::int32_t initial) {
    c.js = this;
    c.conts.store(nullptr, std::memory_order_relaxed);
    c.remaining.store(initial, std::memory_order_relaxed);
}

bool JobSystem::enqueue_job(const Job& j, Priority p) {
    for (;;) {
        bool ok = false;
        if (p == Priority::High) ok = q_high_.enqueue(j);
        else if (p == Priority::Normal) ok = q_norm_.enqueue(j);
        else ok = q_low_.enqueue(j);
        if (ok) {
            if (p == Priority::High) pending_high_.fetch_add(1, std::memory_order_relaxed);
            else if (p == Priority::Normal) pending_norm_.fetch_add(1, std::memory_order_relaxed);
            else pending_low_.fetch_add(1, std::memory_order_relaxed);
            wake_one();
            return true;
        }
        std::this_thread::yield();
    }
}

void JobSystem::submit(JobFn fn, void* data, Priority prio, Counter* counter, Counter* dependency, const char* name) {
    if (!fn) return;
    if (counter) counter->add(1);
    Job j{fn, data, counter, dependency, name};
    if (dependency && !dependency->is_done()) {
        auto* n = new Continuation{j, prio, nullptr};
        Continuation* head = dependency->conts.load(std::memory_order_relaxed);
        do { n->next = head; } while (!dependency->conts.compare_exchange_weak(head, n, std::memory_order_release, std::memory_order_relaxed));
        return;
    }
    enqueue_job(j, prio);
}

void JobSystem::submit_batch(const Job* jobs, std::size_t n, Priority prio, Counter* counter, Counter* dependency) {
    if (!jobs || n == 0) return;
    if (counter) counter->add((std::int32_t)n);
    for (std::size_t i = 0; i < n; ++i) {
        const Job& in = jobs[i];
        if (!in.fn) {
            if (counter) counter->done();
            continue;
        }
        Job j{in.fn, in.data, counter, dependency, in.name};
        if (dependency && !dependency->is_done()) {
            auto* nn = new Continuation{j, prio, nullptr};
            Continuation* head = dependency->conts.load(std::memory_order_relaxed);
            do { nn->next = head; } while (!dependency->conts.compare_exchange_weak(head, nn, std::memory_order_release, std::memory_order_relaxed));
            continue;
        }
        enqueue_job(j, prio);
    }
}

bool JobSystem::try_dequeue(Job& out) {
    if (q_high_.dequeue(out)) { pending_high_.fetch_sub(1, std::memory_order_relaxed); return true; }
    if (q_norm_.dequeue(out)) { pending_norm_.fetch_sub(1, std::memory_order_relaxed); return true; }
    if (q_low_.dequeue(out)) { pending_low_.fetch_sub(1, std::memory_order_relaxed); return true; }
    return false;
}

bool JobSystem::try_run_one() {
    Job j{};
    if (!try_dequeue(j)) return false;
    const auto t0 = std::chrono::steady_clock::now();
    const char* nm = j.name ? j.name : "job";
    CUBE_PROFILE_SCOPE_N("job");
    j.fn(j.data);
    const auto t1 = std::chrono::steady_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    if ((std::uint64_t)ms > cfg_.stall_warn_ms) {
        stall_warnings_.fetch_add(1, std::memory_order_relaxed);
        LOG_WARN("Jobs", "Job '%s' stall: %lldms", nm, (long long)ms);
    }
    if (j.counter) j.counter->done();
    return true;
}

void JobSystem::wait(Counter& c) {
    using namespace std::chrono_literals;
    const auto start = std::chrono::steady_clock::now();
    bool warned = false;
    while (!c.is_done()) {
        if (try_run_one()) continue;
        if (!warned) {
            const auto now = std::chrono::steady_clock::now();
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            const std::uint32_t pending = pending_high_.load(std::memory_order_relaxed) + pending_norm_.load(std::memory_order_relaxed) + pending_low_.load(std::memory_order_relaxed);
            if (pending == 0 && ms > 250) {
                warned = true;
                LOG_WARN("Jobs", "Possible deadlock waiting on counter");
            }
        }
        std::unique_lock lk(c.wait_m);
        if (c.is_done()) break;
        c.wait_cv.wait_for(lk, 1ms);
    }
}

void JobSystem::wake_one() {
    wake_cv_.notify_one();
}

void JobSystem::worker_main(std::uint32_t worker_index) {
    tls_is_worker_ = true;
    using clock = std::chrono::steady_clock;
    auto last = clock::now();
    for (;;) {
        if (stop_.load(std::memory_order_acquire)) break;
        Job j{};
        if (!try_dequeue(j)) {
            auto now = clock::now();
            const auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last).count();
            worker_counters_[worker_index].total_ns.fetch_add((std::uint64_t)dt, std::memory_order_relaxed);
            last = now;
            std::unique_lock lk(wake_m_);
            wake_cv_.wait_for(lk, std::chrono::milliseconds(2), [this] {
                return stop_.load(std::memory_order_relaxed) ||
                    (pending_high_.load(std::memory_order_relaxed) + pending_norm_.load(std::memory_order_relaxed) + pending_low_.load(std::memory_order_relaxed)) > 0;
            });
            continue;
        }

        auto now = clock::now();
        auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last).count();
        worker_counters_[worker_index].total_ns.fetch_add((std::uint64_t)dt, std::memory_order_relaxed);
        last = now;

        const auto t0 = clock::now();
        const char* nm = j.name ? j.name : "job";
        CUBE_PROFILE_SCOPE_N("job");
        j.fn(j.data);
        const auto t1 = clock::now();
        const auto busy = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        worker_counters_[worker_index].busy_ns.fetch_add((std::uint64_t)busy, std::memory_order_relaxed);

        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        if ((std::uint64_t)ms > cfg_.stall_warn_ms) {
            stall_warnings_.fetch_add(1, std::memory_order_relaxed);
            LOG_WARN("Jobs", "Job '%s' stall: %lldms", nm, (long long)ms);
        }
        if (j.counter) j.counter->done();
    }
    tls_is_worker_ = false;
}

JobSystem::Stats JobSystem::snapshot_stats() {
    Stats s{};
    s.worker_count = (std::uint32_t)worker_counters_.size();
    s.pending_high = pending_high_.load(std::memory_order_relaxed);
    s.pending_normal = pending_norm_.load(std::memory_order_relaxed);
    s.pending_low = pending_low_.load(std::memory_order_relaxed);
    s.stall_warnings = stall_warnings_.load(std::memory_order_relaxed);
    const std::size_t n = std::min<std::size_t>(worker_counters_.size(), s.worker_utilization.size());
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint64_t busy = worker_counters_[i].busy_ns.exchange(0, std::memory_order_relaxed);
        const std::uint64_t total = worker_counters_[i].total_ns.exchange(0, std::memory_order_relaxed);
        s.worker_utilization[i] = total ? (float)((double)busy * 100.0 / (double)total) : 0.0f;
    }
    return s;
}

}

