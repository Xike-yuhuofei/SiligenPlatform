#define MODULE_NAME "DXFDispensingExecutionUseCase"

#include "DXFDispensingExecutionUseCase.h"

#include "shared/logging/PrintfLogFormatter.h"
#include "shared/interfaces/ILoggingService.h"

#include <chrono>
#include <thread>

using namespace Siligen::Shared::Types;

namespace Siligen::Application::UseCases::Dispensing::DXF {

Result<TaskID> DXFDispensingExecutionUseCase::ExecuteAsync(const DXFDispensingMVPRequest& request) {
    auto validation = request.Validate();
    if (!validation.IsSuccess()) {
        return Result<TaskID>::Failure(validation.GetError());
    }
    auto conn_check = ValidateHardwareConnection();
    if (!conn_check.IsSuccess()) {
        return Result<TaskID>::Failure(conn_check.GetError());
    }

    TaskID task_id = GenerateTaskID();
    auto context = std::make_shared<TaskExecutionContext>();
    context->task_id = task_id;
    context->request = request;
    context->state.store(TaskState::PENDING);
    context->start_time = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        tasks_[task_id] = context;
    }

    auto runner = [this, context]() {
        if (context->cancel_requested.load()) {
            context->state.store(TaskState::CANCELLED);
            {
                std::lock_guard<std::mutex> lock(context->mutex_);
                context->error_message = "执行已取消";
            }
            context->end_time = std::chrono::steady_clock::now();
            return;
        }

        context->state.store(TaskState::RUNNING);
        auto exec_result = this->ExecuteInternal(context->request, context);

        if (context->cancel_requested.load() || stop_requested_.load()) {
            context->state.store(TaskState::CANCELLED);
            std::lock_guard<std::mutex> lock(context->mutex_);
            context->error_message = "执行已取消";
        } else if (exec_result.IsSuccess()) {
            context->state.store(TaskState::COMPLETED);
            context->result = exec_result.Value();
            context->executed_segments.store(context->result.executed_segments);
            context->total_segments.store(context->result.total_segments);
        } else {
            context->state.store(TaskState::FAILED);
            std::lock_guard<std::mutex> lock(context->mutex_);
            context->error_message = exec_result.GetError().GetMessage();
        }

        context->end_time = std::chrono::steady_clock::now();
    };

    if (task_scheduler_port_) {
        auto submit_result = task_scheduler_port_->SubmitTask(runner);
        if (!submit_result.IsSuccess()) {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            tasks_.erase(task_id);
            return Result<TaskID>::Failure(submit_result.GetError());
        }
    } else {
        std::thread(std::move(runner)).detach();
    }

    return Result<TaskID>::Success(task_id);
}

Result<TaskStatusResponse> DXFDispensingExecutionUseCase::GetTaskStatus(const TaskID& task_id) const {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        return Result<TaskStatusResponse>::Failure(
            Error(ErrorCode::INVALID_STATE, "Task not found", "DXFDispensingExecutionUseCase"));
    }

    const auto& context = it->second;
    TaskStatusResponse response;
    response.task_id = task_id;
    response.state = TaskStateToString(context->state.load());
    response.executed_segments = context->executed_segments.load();
    response.total_segments = context->total_segments.load();
    if (response.total_segments > 0) {
        response.progress_percent = (response.executed_segments * 100) / response.total_segments;
    }
    auto now = std::chrono::steady_clock::now();
    response.elapsed_seconds = std::chrono::duration<float>(now - context->start_time).count();
    {
        std::lock_guard<std::mutex> context_lock(context->mutex_);
        response.error_message = context->error_message;
    }

    return Result<TaskStatusResponse>::Success(response);
}

Result<void> DXFDispensingExecutionUseCase::CancelTask(const TaskID& task_id) {
    std::shared_ptr<TaskExecutionContext> context;
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        auto it = tasks_.find(task_id);
        if (it == tasks_.end()) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "Task not found", "DXFDispensingExecutionUseCase"));
        }
        context = it->second;
    }

    const auto state = context->state.load();
    if (state == TaskState::RUNNING || state == TaskState::PENDING || state == TaskState::PAUSED) {
        context->cancel_requested.store(true);
        context->pause_requested.store(false);
        StopExecution();
        return Result<void>::Success();
    }

    return Result<void>::Failure(
        Error(ErrorCode::INVALID_STATE, "Task cannot be cancelled in current state", "DXFDispensingExecutionUseCase"));
}

void DXFDispensingExecutionUseCase::CleanupExpiredTasks() {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    auto now = std::chrono::steady_clock::now();
    auto it = tasks_.begin();
    while (it != tasks_.end()) {
        auto context = it->second;
        bool should_remove = false;

        const auto state = context->state.load();
        if (state == TaskState::COMPLETED ||
            state == TaskState::FAILED ||
            state == TaskState::CANCELLED) {
            auto elapsed = std::chrono::duration_cast<std::chrono::hours>(
                now - context->end_time).count();
            if (elapsed >= 1) {
                should_remove = true;
            }
        }

        if (state == TaskState::RUNNING || state == TaskState::PAUSED) {
            auto elapsed = std::chrono::duration_cast<std::chrono::hours>(
                now - context->start_time).count();
            if (elapsed >= 24) {
                should_remove = true;
            }
        }

        if (should_remove) {
            it = tasks_.erase(it);
        } else {
            ++it;
        }
    }
}

TaskID DXFDispensingExecutionUseCase::GenerateTaskID() const {
    auto now = std::chrono::steady_clock::now();
    auto timestamp = now.time_since_epoch().count();
    return "task-" + std::to_string(timestamp);
}

std::string DXFDispensingExecutionUseCase::TaskStateToString(TaskState state) const {
    switch (state) {
        case TaskState::PENDING:
            return "pending";
        case TaskState::RUNNING:
            return "running";
        case TaskState::PAUSED:
            return "paused";
        case TaskState::COMPLETED:
            return "completed";
        case TaskState::FAILED:
            return "failed";
        case TaskState::CANCELLED:
            return "cancelled";
        default:
            return "unknown";
    }
}

}  // namespace Siligen::Application::UseCases::Dispensing::DXF

