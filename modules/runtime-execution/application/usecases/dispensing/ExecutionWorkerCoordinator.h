#pragma once

#include "ExecutionSessionStore.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace Siligen::Application::UseCases::Dispensing {

struct ExecutionWorkerCoordinator {
    void JoinWorkerThread() {
        std::thread worker_to_join;
        {
            std::lock_guard<std::mutex> lock(worker_mutex_);
            if (!worker_thread_.joinable()) {
                return;
            }
            worker_to_join = std::move(worker_thread_);
        }
        if (worker_to_join.joinable()) {
            worker_to_join.join();
        }
    }

    void JoinJobWorkerThread() {
        std::thread worker_to_join;
        {
            std::lock_guard<std::mutex> lock(job_worker_mutex_);
            if (!job_worker_thread_.joinable()) {
                return;
            }
            worker_to_join = std::move(job_worker_thread_);
        }
        if (worker_to_join.joinable()) {
            worker_to_join.join();
        }
    }

    void RegisterTaskInflight(const std::shared_ptr<TaskExecutionContext>& context) {
        if (!context) {
            return;
        }
        context->inflight_released.store(false);
        context->inflight_registered.store(true);
        inflight_tasks_.fetch_add(1, std::memory_order_relaxed);
    }

    void ReleaseTaskInflight(const std::shared_ptr<TaskExecutionContext>& context) {
        if (!context || !context->inflight_registered.load()) {
            return;
        }

        bool expected = false;
        if (!context->inflight_released.compare_exchange_strong(expected, true)) {
            return;
        }

        const auto remaining = inflight_tasks_.fetch_sub(1, std::memory_order_relaxed);
        if (remaining <= 1U) {
            std::lock_guard<std::mutex> lock(inflight_mutex_);
            inflight_cv_.notify_all();
        }
    }

    template <typename TReconcileFn, typename TBuildFn>
    bool WaitForAllInflightTasks(
        std::chrono::milliseconds timeout,
        std::chrono::milliseconds poll_interval,
        TReconcileFn&& reconcile_fn,
        TBuildFn&& build_diagnostics_fn,
        std::string* diagnostics_out) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::unique_lock<std::mutex> lock(inflight_mutex_);
                if (inflight_cv_.wait_for(
                        lock,
                        poll_interval,
                        [this]() { return inflight_tasks_.load(std::memory_order_relaxed) == 0U; })) {
                    return true;
                }
            }
            reconcile_fn();
        }

        reconcile_fn();
        {
            std::unique_lock<std::mutex> lock(inflight_mutex_);
            if (inflight_cv_.wait_for(
                    lock,
                    poll_interval,
                    [this]() { return inflight_tasks_.load(std::memory_order_relaxed) == 0U; })) {
                return true;
            }
        }
        if (diagnostics_out != nullptr) {
            *diagnostics_out = build_diagnostics_fn();
        }
        return inflight_tasks_.load(std::memory_order_relaxed) == 0U;
    }

    mutable std::mutex worker_mutex_;
    std::thread worker_thread_;

    mutable std::mutex job_worker_mutex_;
    std::thread job_worker_thread_;

    std::atomic<std::uint32_t> inflight_tasks_{0};
    mutable std::mutex inflight_mutex_;
    std::condition_variable inflight_cv_;
};

}  // namespace Siligen::Application::UseCases::Dispensing
