/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  thread_pool.h — Lock-Free–Inspired Thread Pool with Future Support       ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * FILE PURPOSE:
 *   Provides a fixed-size thread pool for parallelizing pipeline stages.
 *   Used by PipelineManager to run translation and TTS calls concurrently.
 *
 * ARCHITECTURE:
 *
 *   ┌─────────────── ThreadPool ──────────────────┐
 *   │                                              │
 *   │    submit(task) ────► ┌──────────────┐       │
 *   │                       │  Task Queue  │       │    (DSA: std::queue<>)
 *   │                       │  (FIFO)      │       │    (Protected by mutex_)
 *   │                       └──────┬───────┘       │
 *   │                              │ notify_one    │
 *   │                              ▼               │
 *   │   ┌─────────┐ ┌─────────┐ ┌─────────┐      │
 *   │   │ Worker  │ │ Worker  │ │ Worker  │ ...   │    N worker threads
 *   │   │ Thread  │ │ Thread  │ │ Thread  │       │    (cv_.wait → pop → run)
 *   │   └─────────┘ └─────────┘ └─────────┘      │
 *   │                                              │
 *   │   wait_all() blocks until queue + active = 0 │
 *   └──────────────────────────────────────────────┘
 *
 * DSA & COMPLEXITY:
 *   - Task queue:     std::queue<> (FIFO), O(1) push/pop
 *   - Synchronization: std::mutex + std::condition_variable
 *   - Submit:         O(1) — enqueue + notify
 *   - Wait all:       Blocks on done_cv_ until all tasks complete
 *
 * DESIGN PATTERN:
 *   Producer–Consumer pattern.  submit() is producer, workers are consumers.
 *
 * C++20 FEATURES:
 *   - std::invocable concept for type-safe task submission
 *   - std::invoke_result_t for automatic return type deduction
 *
 * THREAD SAFETY:
 *   All public methods are thread-safe.  Multiple threads may call submit()
 *   concurrently without external locking.
 *
 * DEPENDENCIES:
 *   Standard library only (<thread>, <mutex>, <queue>, <future>, <concepts>)
 */

#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>
#include <concepts>

namespace vd {

class ThreadPool {
public:
    // ─── Construction & Destruction ───────────────────────────────────────

    /**
     * Create thread pool with fixed worker count.
     * @param num_threads  Number of OS threads to spawn (typically CPU core count)
     */
    explicit ThreadPool(size_t num_threads);

    /// Destructor — signals all workers to stop and joins them.
    ~ThreadPool();

    // Non-copyable, non-movable (workers hold `this` pointers)
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // ─── Task Submission ──────────────────────────────────────────────────

    /**
     * Submit a callable task and receive a std::future for the result.
     *
     * @tparam F     Callable type (lambda, function, functor)
     * @tparam Args  Argument types
     * @param  f     The callable to execute
     * @param  args  Arguments forwarded to the callable
     * @return std::future<ReturnType> — blocks on .get() until complete
     *
     * Complexity: O(1) — wraps in packaged_task, enqueues, notifies one worker.
     * Exceptions propagate through the returned future.
     *
     * Example:
     *   auto future = pool.submit([](int x) { return x * 2; }, 21);
     *   int result = future.get();  // 42
     */
    template<std::invocable F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using RetType = std::invoke_result_t<F, Args...>;

        // Wrap the callable in a shared packaged_task for type erasure
        auto task = std::make_shared<std::packaged_task<RetType()>>(
            [f = std::forward<F>(f),
             ...args = std::forward<Args>(args)]() mutable {
                return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
            }
        );

        auto future = task->get_future();

        {
            std::lock_guard lock(mutex_);
            if (stop_) throw std::runtime_error("submit() called on stopped ThreadPool");
            tasks_.emplace([task]() { (*task)(); });
        }

        cv_.notify_one();  // Wake one waiting worker
        return future;
    }

    // ─── Synchronization ──────────────────────────────────────────────────

    /// Block until all enqueued tasks have completed.
    void wait_all();

    /// Number of tasks currently queued + actively running.
    [[nodiscard]] size_t pending_count() const;

    /// Number of worker threads.
    [[nodiscard]] size_t thread_count() const { return workers_.size(); }

    /// Gracefully stop all workers (blocks until joined).
    void shutdown();

private:
    // ─── Data Members ─────────────────────────────────────────────────────

    std::vector<std::thread>          workers_;         ///< Worker OS threads
    std::queue<std::function<void()>> tasks_;           ///< FIFO task queue
    mutable std::mutex                mutex_;           ///< Protects tasks_ queue
    std::condition_variable           cv_;              ///< Signals enqueue events
    std::atomic<bool>                 stop_{false};     ///< Shutdown flag
    std::atomic<size_t>               active_tasks_{0}; ///< Currently executing tasks
    std::condition_variable           done_cv_;         ///< Signals task completion
    std::mutex                        done_mutex_;      ///< Protects done_cv_ waits
};

} // namespace vd
