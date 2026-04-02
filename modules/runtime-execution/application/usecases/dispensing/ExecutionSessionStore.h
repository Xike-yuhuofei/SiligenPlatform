#pragma once

#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Siligen::Application::UseCases::Dispensing {

using TaskID = std::string;

enum class TaskState {
    PENDING,
    RUNNING,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELLED
};

enum class JobState {
    PENDING,
    RUNNING,
    STOPPING,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELLED
};

struct TaskStatusResponse {
    TaskID task_id;
    std::string state;
    uint32 progress_percent = 0;
    uint32 executed_segments = 0;
    uint32 total_segments = 0;
    float32 elapsed_seconds = 0.0f;
    std::string error_message;
};

struct TaskExecutionContext {
    TaskID task_id;
    std::atomic<TaskState> state{TaskState::PENDING};
    std::atomic<TaskState> committed_terminal_state{TaskState::PENDING};
    DispensingExecutionRequest request;
    DispensingExecutionResult result;

    std::atomic<uint32> total_segments{0};
    std::atomic<uint32> executed_segments{0};
    std::atomic<uint32> reported_progress_percent{0};
    std::atomic<uint32> reported_executed_segments{0};
    std::atomic<uint32> estimated_execution_ms{0};
    std::atomic<bool> cancel_requested{false};
    std::atomic<bool> pause_requested{false};
    std::atomic<bool> pause_applied{false};
    std::atomic<bool> terminal_committed{false};
    std::atomic<bool> execution_started{false};
    std::atomic<bool> inflight_registered{false};
    std::atomic<bool> inflight_released{false};

    std::chrono::steady_clock::time_point start_time{};
    std::chrono::steady_clock::time_point end_time{};
    mutable std::mutex mutex_;
    std::string scheduler_task_id;
    std::string error_message;
};

struct JobExecutionContext {
    JobID job_id;
    std::string plan_id;
    std::string plan_fingerprint;
    DispensingExecutionRequest execution_request;
    std::atomic<JobState> state{JobState::PENDING};
    std::atomic<uint32> target_count{0};
    std::atomic<uint32> completed_count{0};
    std::atomic<uint32> current_cycle{0};
    std::atomic<uint32> current_segment{0};
    std::atomic<uint32> total_segments{0};
    std::atomic<uint32> cycle_progress_percent{0};
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> pause_requested{false};
    std::atomic<bool> final_state_committed{false};
    bool dry_run = false;
    std::chrono::steady_clock::time_point start_time{};
    std::chrono::steady_clock::time_point end_time{};
    mutable std::mutex mutex_;
    std::string active_task_id;
    std::string error_message;
};

struct ExecutionSessionStore {
    static bool IsTerminalState(TaskState state) {
        return state == TaskState::COMPLETED || state == TaskState::FAILED || state == TaskState::CANCELLED;
    }

    static bool IsTerminalJobState(JobState state) {
        return state == JobState::COMPLETED || state == JobState::FAILED || state == JobState::CANCELLED;
    }

    static TaskState ResolveVisibleState(const std::shared_ptr<TaskExecutionContext>& context) {
        if (!context) {
            return TaskState::FAILED;
        }

        const auto state = context->state.load();
        if (!context->terminal_committed.load()) {
            return state;
        }

        const auto committed = context->committed_terminal_state.load();
        if (IsTerminalState(committed)) {
            return committed;
        }
        return state;
    }

    static bool TryCommitTerminalState(
        const std::shared_ptr<TaskExecutionContext>& context,
        TaskState terminal_state,
        const std::string& error_message) {
        if (!context || !IsTerminalState(terminal_state)) {
            return false;
        }

        bool expected = false;
        if (!context->terminal_committed.compare_exchange_strong(expected, true)) {
            return false;
        }

        context->committed_terminal_state.store(terminal_state);
        context->state.store(terminal_state);
        context->end_time = std::chrono::steady_clock::now();
        if (!error_message.empty()) {
            std::lock_guard<std::mutex> lock(context->mutex_);
            context->error_message = error_message;
        }
        return true;
    }

    static bool TryCommitJobTerminalState(
        const std::shared_ptr<JobExecutionContext>& context,
        JobState terminal_state,
        const std::string& error_message) {
        if (!context || !IsTerminalJobState(terminal_state)) {
            return false;
        }

        bool expected = false;
        if (!context->final_state_committed.compare_exchange_strong(expected, true)) {
            return false;
        }

        context->state.store(terminal_state);
        context->end_time = std::chrono::steady_clock::now();
        if (!error_message.empty()) {
            std::lock_guard<std::mutex> lock(context->mutex_);
            context->error_message = error_message;
        }
        return true;
    }

    std::shared_ptr<TaskExecutionContext> ResolveActiveContextLocked() const {
        if (active_task_id_.empty()) {
            return nullptr;
        }
        auto it = tasks_.find(active_task_id_);
        if (it == tasks_.end()) {
            return nullptr;
        }
        return it->second;
    }

    JobID GetActiveJobId() const {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        return active_job_id_;
    }

    TaskID GenerateTaskID() {
        const auto seq = task_sequence_.fetch_add(1, std::memory_order_relaxed) + 1;
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        return "task-" + std::to_string(millis) + "-" + std::to_string(seq);
    }

    JobID GenerateJobID() {
        const auto seq = job_sequence_.fetch_add(1, std::memory_order_relaxed) + 1;
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        return "job-" + std::to_string(millis) + "-" + std::to_string(seq);
    }

    mutable std::mutex tasks_mutex_;
    std::unordered_map<TaskID, std::shared_ptr<TaskExecutionContext>> tasks_;
    TaskID active_task_id_;

    mutable std::mutex jobs_mutex_;
    std::unordered_map<JobID, std::shared_ptr<JobExecutionContext>> jobs_;
    JobID active_job_id_;

    std::atomic<std::uint64_t> task_sequence_{0};
    std::atomic<std::uint64_t> job_sequence_{0};
};

}  // namespace Siligen::Application::UseCases::Dispensing
