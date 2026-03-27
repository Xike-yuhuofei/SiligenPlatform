#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

#define private public
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
#undef private

namespace {

using Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase;
using Siligen::Application::UseCases::Dispensing::DispensingMVPRequest;
using Siligen::Application::UseCases::Dispensing::TaskExecutionContext;
using Siligen::Application::UseCases::Dispensing::TaskState;
using Siligen::Application::Services::Dispensing::DispensePlanningFacade;
using Siligen::Domain::Dispensing::ValueObjects::JobExecutionMode;
using Siligen::Domain::Dispensing::ValueObjects::ProcessOutputPolicy;
using Siligen::Domain::Machine::ValueObjects::MachineMode;
using Siligen::Shared::Types::ErrorCode;

template <typename T>
std::shared_ptr<T> MakeDummyShared() {
    return std::shared_ptr<T>(reinterpret_cast<T*>(0x1), [](T*) {});
}

DispensingExecutionUseCase CreateExecutionUseCase() {
    return DispensingExecutionUseCase(
        MakeDummyShared<DispensePlanningFacade>(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        MakeDummyShared<Siligen::Application::Services::DXF::DxfPbPreparationService>());
}

}  // namespace

TEST(DispensingExecutionUseCaseProgressTest, RunningTaskUsesEstimatedProgressFallback) {
    auto use_case = CreateExecutionUseCase();

    auto context = std::make_shared<TaskExecutionContext>();
    context->task_id = "task-progress";
    context->state.store(TaskState::RUNNING);
    context->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    context->total_segments.store(9);
    context->executed_segments.store(0);
    context->estimated_execution_ms.store(40000);
    use_case.tasks_[context->task_id] = context;
    use_case.active_task_id_ = context->task_id;

    auto running_result = use_case.GetTaskStatus(context->task_id);
    ASSERT_TRUE(running_result.IsSuccess());
    const auto running_status = running_result.Value();
    EXPECT_GT(running_status.progress_percent, 0U);
    EXPECT_LT(running_status.progress_percent, 100U);
    EXPECT_GT(running_status.executed_segments, 0U);

    context->state.store(TaskState::PAUSED);
    auto paused_result = use_case.GetTaskStatus(context->task_id);
    ASSERT_TRUE(paused_result.IsSuccess());
    const auto paused_status = paused_result.Value();
    EXPECT_GE(paused_status.progress_percent, running_status.progress_percent);
    EXPECT_GE(paused_status.executed_segments, running_status.executed_segments);
}

TEST(DispensingExecutionUseCaseProgressTest, RunningTaskNeverReportsFullyCompletedProgress) {
    auto use_case = CreateExecutionUseCase();

    auto context = std::make_shared<TaskExecutionContext>();
    context->task_id = "task-running-clamp";
    context->state.store(TaskState::RUNNING);
    context->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    context->total_segments.store(9);
    context->executed_segments.store(9);
    context->reported_executed_segments.store(9);
    context->reported_progress_percent.store(100);
    context->estimated_execution_ms.store(40000);
    use_case.tasks_[context->task_id] = context;
    use_case.active_task_id_ = context->task_id;

    auto status_result = use_case.GetTaskStatus(context->task_id);
    ASSERT_TRUE(status_result.IsSuccess());
    const auto status = status_result.Value();
    EXPECT_EQ(status.state, "running");
    EXPECT_LT(status.progress_percent, 100U);
    EXPECT_LT(status.executed_segments, status.total_segments);
}

TEST(DispensingExecutionUseCaseProgressTest, RejectsOperationsOnNonActiveTask) {
    auto use_case = CreateExecutionUseCase();

    auto active_context = std::make_shared<TaskExecutionContext>();
    active_context->task_id = "task-active";
    active_context->state.store(TaskState::RUNNING);
    active_context->start_time = std::chrono::steady_clock::now();
    use_case.tasks_[active_context->task_id] = active_context;
    use_case.active_task_id_ = active_context->task_id;

    auto stale_context = std::make_shared<TaskExecutionContext>();
    stale_context->task_id = "task-stale";
    stale_context->state.store(TaskState::COMPLETED);
    stale_context->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    stale_context->end_time = std::chrono::steady_clock::now();
    use_case.tasks_[stale_context->task_id] = stale_context;

    auto status_result = use_case.GetTaskStatus(stale_context->task_id);
    ASSERT_TRUE(status_result.IsError());

    auto cancel_result = use_case.CancelTask(stale_context->task_id);
    ASSERT_TRUE(cancel_result.IsError());
}

TEST(DispensingExecutionUseCaseProgressTest, TerminalCommittedStateOverridesLateRunningWrites) {
    auto use_case = CreateExecutionUseCase();

    auto context = std::make_shared<TaskExecutionContext>();
    context->task_id = "task-terminal-visible";
    context->state.store(TaskState::RUNNING);
    context->committed_terminal_state.store(TaskState::CANCELLED);
    context->terminal_committed.store(true);
    context->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    context->end_time = std::chrono::steady_clock::now();
    context->total_segments.store(10);
    context->executed_segments.store(6);
    use_case.tasks_[context->task_id] = context;
    use_case.active_task_id_ = context->task_id;

    auto status_result = use_case.GetTaskStatus(context->task_id);
    ASSERT_TRUE(status_result.IsSuccess());
    EXPECT_EQ(status_result.Value().state, "cancelled");
}

TEST(DispensingExecutionUseCaseProgressTest, PauseTaskReturnsTerminalFailureInsteadOfTimeout) {
    auto use_case = CreateExecutionUseCase();

    auto context = std::make_shared<TaskExecutionContext>();
    context->task_id = "task-pause-terminal";
    context->state.store(TaskState::RUNNING);
    context->start_time = std::chrono::steady_clock::now();
    use_case.tasks_[context->task_id] = context;
    use_case.active_task_id_ = context->task_id;

    std::thread terminal_writer([context]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        context->terminal_committed.store(true);
        context->committed_terminal_state.store(TaskState::COMPLETED);
        context->state.store(TaskState::COMPLETED);
        context->end_time = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(context->mutex_);
        context->error_message = "completed-by-test";
    });

    auto pause_result = use_case.PauseTask(context->task_id);
    terminal_writer.join();

    ASSERT_TRUE(pause_result.IsError());
    EXPECT_EQ(pause_result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(pause_result.GetError().GetMessage().find("failure_stage=pause_confirm"), std::string::npos);
    EXPECT_NE(pause_result.GetError().GetMessage().find("failure_code=COMPLETED"), std::string::npos);
}

TEST(DispensingExecutionUseCaseProgressTest, ResumeTaskReturnsTerminalFailureInsteadOfTimeout) {
    auto use_case = CreateExecutionUseCase();

    auto context = std::make_shared<TaskExecutionContext>();
    context->task_id = "task-resume-terminal";
    context->state.store(TaskState::PAUSED);
    context->pause_applied.store(true);
    context->start_time = std::chrono::steady_clock::now();
    use_case.tasks_[context->task_id] = context;
    use_case.active_task_id_ = context->task_id;

    std::thread terminal_writer([context]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        context->terminal_committed.store(true);
        context->committed_terminal_state.store(TaskState::CANCELLED);
        context->state.store(TaskState::CANCELLED);
        context->end_time = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(context->mutex_);
        context->error_message = "cancelled-by-test";
    });

    auto resume_result = use_case.ResumeTask(context->task_id);
    terminal_writer.join();

    ASSERT_TRUE(resume_result.IsError());
    EXPECT_EQ(resume_result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(resume_result.GetError().GetMessage().find("failure_stage=resume_confirm"), std::string::npos);
    EXPECT_NE(resume_result.GetError().GetMessage().find("failure_code=CANCELLED"), std::string::npos);
}

TEST(DispensingExecutionUseCaseProgressTest, CleanupExpiredTasksKeepsActiveRunningTask) {
    auto use_case = CreateExecutionUseCase();

    auto running_context = std::make_shared<TaskExecutionContext>();
    running_context->task_id = "task-running-old";
    running_context->state.store(TaskState::RUNNING);
    running_context->start_time = std::chrono::steady_clock::now() - std::chrono::hours(48);
    use_case.tasks_[running_context->task_id] = running_context;
    use_case.active_task_id_ = running_context->task_id;

    auto completed_context = std::make_shared<TaskExecutionContext>();
    completed_context->task_id = "task-completed-old";
    completed_context->state.store(TaskState::COMPLETED);
    completed_context->committed_terminal_state.store(TaskState::COMPLETED);
    completed_context->terminal_committed.store(true);
    completed_context->start_time = std::chrono::steady_clock::now() - std::chrono::hours(3);
    completed_context->end_time = std::chrono::steady_clock::now() - std::chrono::hours(2);
    use_case.tasks_[completed_context->task_id] = completed_context;

    use_case.CleanupExpiredTasks();

    EXPECT_NE(use_case.tasks_.find(running_context->task_id), use_case.tasks_.end());
    EXPECT_EQ(use_case.active_task_id_, running_context->task_id);
    EXPECT_EQ(use_case.tasks_.find(completed_context->task_id), use_case.tasks_.end());
}

TEST(DispensingExecutionUseCaseProgressTest, DryRunRequestResolvesToExplicitSafeProfile) {
    DispensingMVPRequest request;
    request.dxf_filepath = "dummy.dxf";
    request.dry_run = true;
    request.dispensing_speed_mm_s = 25.0f;
    request.dry_run_speed_mm_s = 80.0f;
    const auto validation = request.Validate();

    EXPECT_EQ(request.ResolveMachineMode(), MachineMode::Test);
    EXPECT_EQ(request.ResolveExecutionMode(), JobExecutionMode::ValidationDryCycle);
    EXPECT_EQ(request.ResolveOutputPolicy(), ProcessOutputPolicy::Inhibited);
    EXPECT_TRUE(validation.IsSuccess()) << validation.GetError().ToString();
}

TEST(DispensingExecutionUseCaseProgressTest, RejectsConflictingLegacyAndExplicitExecutionSemantics) {
    DispensingMVPRequest request;
    request.dxf_filepath = "dummy.dxf";
    request.dry_run = true;
    request.output_policy = ProcessOutputPolicy::Enabled;
    request.dispensing_speed_mm_s = 25.0f;
    request.dry_run_speed_mm_s = 80.0f;

    auto validation = request.Validate();
    ASSERT_TRUE(validation.IsError());
    EXPECT_EQ(validation.GetError().GetCode(), ErrorCode::INVALID_PARAMETER);
}
