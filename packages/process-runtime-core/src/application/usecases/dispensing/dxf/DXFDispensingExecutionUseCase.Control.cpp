#define MODULE_NAME "DXFDispensingExecutionUseCase"

#include "DXFDispensingExecutionUseCase.h"

#include "domain/dispensing/domain-services/DispensingProcessService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/interfaces/ILoggingService.h"

#include <chrono>
#include <thread>

namespace Siligen::Application::UseCases::Dispensing::DXF {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

namespace {

constexpr auto kPauseTransitionTimeout = std::chrono::seconds(2);
constexpr auto kPauseTransitionPoll = std::chrono::milliseconds(50);

}  // namespace

void DXFDispensingExecutionUseCase::StopExecution() {
    stop_requested_.store(true);
    if (!process_service_) {
        return;
    }
    process_service_->StopExecution(&stop_requested_);
}

Result<void> DXFDispensingExecutionUseCase::PauseTask(const TaskID& task_id) {
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
    if (state == TaskState::PAUSED) {
        return Result<void>::Success();
    }
    if (state != TaskState::RUNNING) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "Task is not running", "DXFDispensingExecutionUseCase"));
    }

    context->pause_requested.store(true);
    const auto deadline = std::chrono::steady_clock::now() + kPauseTransitionTimeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (context->state.load() == TaskState::PAUSED && context->pause_applied.load()) {
            return Result<void>::Success();
        }
        if (context->state.load() == TaskState::FAILED ||
            context->state.load() == TaskState::CANCELLED ||
            context->state.load() == TaskState::COMPLETED) {
            break;
        }
        std::this_thread::sleep_for(kPauseTransitionPoll);
    }

    return Result<void>::Failure(
        Error(ErrorCode::MOTION_TIMEOUT, "Pause transition timed out", "DXFDispensingExecutionUseCase"));
}

Result<void> DXFDispensingExecutionUseCase::ResumeTask(const TaskID& task_id) {
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
    if (state == TaskState::RUNNING && !context->pause_applied.load()) {
        return Result<void>::Success();
    }
    if (state != TaskState::PAUSED) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "Task is not paused", "DXFDispensingExecutionUseCase"));
    }

    context->pause_requested.store(false);
    const auto deadline = std::chrono::steady_clock::now() + kPauseTransitionTimeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (context->state.load() == TaskState::RUNNING && !context->pause_applied.load()) {
            return Result<void>::Success();
        }
        if (context->state.load() == TaskState::FAILED ||
            context->state.load() == TaskState::CANCELLED ||
            context->state.load() == TaskState::COMPLETED) {
            break;
        }
        std::this_thread::sleep_for(kPauseTransitionPoll);
    }

    return Result<void>::Failure(
        Error(ErrorCode::MOTION_TIMEOUT, "Resume transition timed out", "DXFDispensingExecutionUseCase"));
}

}  // namespace Siligen::Application::UseCases::Dispensing::DXF
