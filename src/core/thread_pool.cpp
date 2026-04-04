/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  thread_pool.cpp — Thread Pool Implementation                             ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * WORKER LOOP (per thread):
 *   while (true) {
 *       1. Wait on condition variable until (stop || !tasks_.empty())
 *       2. If stop_ == true && queue empty → exit
 *       3. Pop front task from queue (FIFO)
 *       4. Increment active_tasks_ counter
 *       5. Execute task
 *       6. Decrement active_tasks_ counter
 *       7. Notify done_cv_ (unblocks wait_all() callers)
 *   }
 *
 * SHUTDOWN SEQUENCE:
 *   1. Set stop_ = true
 *   2. Notify all workers
 *   3. Join all threads (waits for in-progress tasks to finish)
 */

#include "core/thread_pool.h"

namespace vd {

// ═══════════════════════════════════════════════════════════════════════════════
// Constructor — Spawn worker threads
// ═══════════════════════════════════════════════════════════════════════════════

ThreadPool::ThreadPool(size_t num_threads) {
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> task;

                // ── Critical section: pop one task ────────────────────────
                {
                    std::unique_lock lock(mutex_);
                    cv_.wait(lock, [this] {
                        return stop_ || !tasks_.empty();
                    });

                    // Drain remaining tasks before stopping
                    if (stop_ && tasks_.empty()) return;

                    task = std::move(tasks_.front());
                    tasks_.pop();
                    ++active_tasks_;
                }

                // ── Execute task (outside lock) ──────────────────────────
                // RAII scope guard guarantees active_tasks_ is decremented
                // and done_cv_ is notified even if task() throws.
                // Without this, an exception would leave active_tasks_
                // permanently inflated, causing wait_all() to deadlock.
                struct ScopeGuard {
                    ThreadPool& pool;
                    ~ScopeGuard() {
                        {
                            std::lock_guard lk(pool.done_mutex_);
                            --pool.active_tasks_;
                        }
                        pool.done_cv_.notify_all();
                    }
                } guard{*this};

                task();
            }
        });
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Destructor
// ═══════════════════════════════════════════════════════════════════════════════

ThreadPool::~ThreadPool() {
    shutdown();
}

// ═══════════════════════════════════════════════════════════════════════════════
// shutdown — Graceful termination
// ═══════════════════════════════════════════════════════════════════════════════

void ThreadPool::shutdown() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();                          // Wake all sleeping workers

    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();   // Wait for each to finish
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// wait_all — Block until all tasks complete
// ═══════════════════════════════════════════════════════════════════════════════
// Condition: queue is empty AND no workers actively executing tasks.

void ThreadPool::wait_all() {
    std::unique_lock lock(done_mutex_);
    done_cv_.wait(lock, [this] {
        std::lock_guard q_lock(mutex_);
        return tasks_.empty() && active_tasks_ == 0;
    });
}

// ═══════════════════════════════════════════════════════════════════════════════
// pending_count — Tasks queued + in-flight
// ═══════════════════════════════════════════════════════════════════════════════

size_t ThreadPool::pending_count() const {
    std::lock_guard lock(mutex_);
    return tasks_.size() + active_tasks_;
}

} // namespace vd
