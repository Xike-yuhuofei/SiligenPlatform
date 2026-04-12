#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

#include "usecases/dispensing/DispensingExecutionUseCase.Internal.h"

namespace {

using Siligen::Application::UseCases::Dispensing::DispensingExecutionRequest;
using Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase;
using Siligen::Application::UseCases::Dispensing::JobExecutionContext;
using Siligen::Application::UseCases::Dispensing::JobState;
using Siligen::Application::UseCases::Dispensing::RuntimeJobStatusResponse;
using Siligen::Application::UseCases::Dispensing::TaskExecutionContext;
using Siligen::Application::UseCases::Dispensing::TaskState;
using Siligen::Domain::Dispensing::Ports::TaskExecutor;
using Siligen::Domain::Dispensing::Ports::TaskStatus;
using Siligen::Domain::Dispensing::Ports::TaskStatusInfo;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionOptions;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionPlan;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionReport;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeOverrides;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeParams;
using Siligen::Domain::Dispensing::ValueObjects::JobExecutionMode;
using Siligen::Domain::Dispensing::ValueObjects::ProcessOutputPolicy;
using Siligen::Domain::Machine::ValueObjects::MachineMode;
using Siligen::Domain::Motion::Ports::InterpolationData;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint;
using RuntimeDispensingProcessPort = Siligen::RuntimeExecution::Contracts::Dispensing::IDispensingProcessPort;
using RuntimeTaskSchedulerPort = Siligen::Domain::Dispensing::Ports::ITaskSchedulerPort;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::ErrorCode;

class StubDispensingProcessPort final : public RuntimeDispensingProcessPort {
   public:
    Result<void> ValidateHardwareConnection() noexcept override {
        return Result<void>::Success();
    }

    Result<DispensingRuntimeParams> BuildRuntimeParams(const DispensingRuntimeOverrides&) noexcept override {
        DispensingRuntimeParams params;
        params.dispensing_velocity = 10.0f;
        return Result<DispensingRuntimeParams>::Success(params);
    }

    Result<DispensingExecutionReport> ExecuteProcess(const DispensingExecutionPlan& plan,
                                                     const DispensingRuntimeParams&,
                                                     const DispensingExecutionOptions&,
                                                     std::atomic<bool>*,
                                                     std::atomic<bool>*,
                                                     std::atomic<bool>*,
                                                     Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver*)
        noexcept override {
        DispensingExecutionReport report;
        report.executed_segments = static_cast<std::uint32_t>(plan.interpolation_segments.size());
        report.total_distance = plan.total_length_mm;
        return Result<DispensingExecutionReport>::Success(report);
    }

    void StopExecution(std::atomic<bool>*, std::atomic<bool>* = nullptr) noexcept override {}
};

class CapturingTaskSchedulerPort final : public RuntimeTaskSchedulerPort {
   public:
    Result<std::string> SubmitTask(TaskExecutor executor) override {
        ++submit_calls;
        last_executor = std::move(executor);
        if (submit_error.has_value()) {
            return Result<std::string>::Failure(submit_error.value());
        }
        return Result<std::string>::Success("scheduler-task-1");
    }

    Result<TaskStatusInfo> GetTaskStatus(const std::string& task_id) const override {
        return Result<TaskStatusInfo>::Success(TaskStatusInfo(task_id, TaskStatus::PENDING, 0));
    }

    Result<void> CancelTask(const std::string&) override {
        cancelled = true;
        return Result<void>::Success();
    }

    void CleanupExpiredTasks() override { ++cleanup_calls; }

    TaskExecutor last_executor;
    bool cancelled = false;
    int cleanup_calls = 0;
    int submit_calls = 0;
    std::optional<Siligen::Shared::Types::Error> submit_error;
};

DispensingExecutionRequest BuildExecutionRequest(bool include_interpolation_points) {
    DispensingExecutionRequest request;
    request.dispensing_speed_mm_s = 10.0f;

    auto& package = request.execution_package;
    package.total_length_mm = 12.5f;
    package.estimated_time_s = 1.25f;

    auto& plan = package.execution_plan;
    plan.total_length_mm = package.total_length_mm;

    if (include_interpolation_points) {
        plan.interpolation_points.emplace_back(0.0f, 0.0f, 10.0f);
        plan.interpolation_points.emplace_back(12.5f, 0.0f, 10.0f);
    }

    InterpolationData segment;
    segment.positions = {12.5f, 0.0f};
    segment.velocity = 10.0f;
    plan.interpolation_segments.push_back(segment);

    MotionTrajectoryPoint first_point;
    first_point.t = 0.0f;
    first_point.position = {0.0f, 0.0f, 0.0f};
    first_point.velocity = {10.0f, 0.0f, 0.0f};
    MotionTrajectoryPoint second_point;
    second_point.t = 1.25f;
    second_point.position = {12.5f, 0.0f, 0.0f};
    second_point.velocity = {10.0f, 0.0f, 0.0f};
    plan.motion_trajectory.points.push_back(first_point);
    plan.motion_trajectory.points.push_back(second_point);
    plan.motion_trajectory.total_length = 12.5f;
    plan.motion_trajectory.total_time = 1.25f;

    return request;
}

std::unique_ptr<DispensingExecutionUseCase::Impl> CreateExecutionUseCase(
    std::shared_ptr<RuntimeDispensingProcessPort> process_port = std::make_shared<StubDispensingProcessPort>(),
    std::shared_ptr<RuntimeTaskSchedulerPort> task_scheduler_port = nullptr) {
    return std::make_unique<DispensingExecutionUseCase::Impl>(
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        std::move(process_port),
        nullptr,
        std::move(task_scheduler_port),
        nullptr,
        nullptr);
}

}  // namespace

TEST(DispensingExecutionUseCaseInternalTest, RunningTaskUsesEstimatedProgressFallback) {
    auto use_case = CreateExecutionUseCase();

    auto context = std::make_shared<TaskExecutionContext>();
    context->task_id = "task-progress";
    context->state.store(TaskState::RUNNING);
    context->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    context->total_segments.store(9);
    context->executed_segments.store(0);
    context->estimated_execution_ms.store(40000);
    use_case->tasks_[context->task_id] = context;
    use_case->active_task_id_ = context->task_id;

    auto running_result = use_case->GetTaskStatus(context->task_id);
    ASSERT_TRUE(running_result.IsSuccess());
    const auto running_status = running_result.Value();
    EXPECT_GT(running_status.progress_percent, 0U);
    EXPECT_LT(running_status.progress_percent, 100U);
    EXPECT_GT(running_status.executed_segments, 0U);

    context->state.store(TaskState::PAUSED);
    auto paused_result = use_case->GetTaskStatus(context->task_id);
    ASSERT_TRUE(paused_result.IsSuccess());
    const auto paused_status = paused_result.Value();
    EXPECT_GE(paused_status.progress_percent, running_status.progress_percent);
    EXPECT_GE(paused_status.executed_segments, running_status.executed_segments);
}

TEST(DispensingExecutionUseCaseInternalTest, RunningTaskNeverReportsFullyCompletedProgress) {
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
    use_case->tasks_[context->task_id] = context;
    use_case->active_task_id_ = context->task_id;

    auto status_result = use_case->GetTaskStatus(context->task_id);
    ASSERT_TRUE(status_result.IsSuccess());
    const auto status = status_result.Value();
    EXPECT_EQ(status.state, "running");
    EXPECT_LT(status.progress_percent, 100U);
    EXPECT_LT(status.executed_segments, status.total_segments);
}

TEST(DispensingExecutionUseCaseInternalTest, RejectsOperationsOnNonActiveTask) {
    auto use_case = CreateExecutionUseCase();

    auto active_context = std::make_shared<TaskExecutionContext>();
    active_context->task_id = "task-active";
    active_context->state.store(TaskState::RUNNING);
    active_context->start_time = std::chrono::steady_clock::now();
    use_case->tasks_[active_context->task_id] = active_context;
    use_case->active_task_id_ = active_context->task_id;

    auto stale_context = std::make_shared<TaskExecutionContext>();
    stale_context->task_id = "task-stale";
    stale_context->state.store(TaskState::COMPLETED);
    stale_context->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    stale_context->end_time = std::chrono::steady_clock::now();
    use_case->tasks_[stale_context->task_id] = stale_context;

    auto status_result = use_case->GetTaskStatus(stale_context->task_id);
    ASSERT_TRUE(status_result.IsError());

    auto cancel_result = use_case->CancelTask(stale_context->task_id);
    ASSERT_TRUE(cancel_result.IsError());
}

TEST(DispensingExecutionUseCaseInternalTest, TerminalCommittedStateOverridesLateRunningWrites) {
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
    use_case->tasks_[context->task_id] = context;
    use_case->active_task_id_ = context->task_id;

    auto status_result = use_case->GetTaskStatus(context->task_id);
    ASSERT_TRUE(status_result.IsSuccess());
    EXPECT_EQ(status_result.Value().state, "cancelled");
}

TEST(DispensingExecutionUseCaseInternalTest, PauseTaskReturnsTerminalFailureInsteadOfTimeout) {
    auto use_case = CreateExecutionUseCase();

    auto context = std::make_shared<TaskExecutionContext>();
    context->task_id = "task-pause-terminal";
    context->state.store(TaskState::RUNNING);
    context->start_time = std::chrono::steady_clock::now();
    use_case->tasks_[context->task_id] = context;
    use_case->active_task_id_ = context->task_id;

    std::thread terminal_writer([context]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        context->terminal_committed.store(true);
        context->committed_terminal_state.store(TaskState::COMPLETED);
        context->state.store(TaskState::COMPLETED);
        context->end_time = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(context->mutex_);
        context->error_message = "completed-by-test";
    });

    auto pause_result = use_case->PauseTask(context->task_id);
    terminal_writer.join();

    ASSERT_TRUE(pause_result.IsError());
    EXPECT_EQ(pause_result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(pause_result.GetError().GetMessage().find("failure_stage=pause_confirm"), std::string::npos);
    EXPECT_NE(pause_result.GetError().GetMessage().find("failure_code=COMPLETED"), std::string::npos);
}

TEST(DispensingExecutionUseCaseInternalTest, ResumeTaskReturnsTerminalFailureInsteadOfTimeout) {
    auto use_case = CreateExecutionUseCase();

    auto context = std::make_shared<TaskExecutionContext>();
    context->task_id = "task-resume-terminal";
    context->state.store(TaskState::PAUSED);
    context->pause_applied.store(true);
    context->start_time = std::chrono::steady_clock::now();
    use_case->tasks_[context->task_id] = context;
    use_case->active_task_id_ = context->task_id;

    std::thread terminal_writer([context]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        context->terminal_committed.store(true);
        context->committed_terminal_state.store(TaskState::CANCELLED);
        context->state.store(TaskState::CANCELLED);
        context->end_time = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(context->mutex_);
        context->error_message = "cancelled-by-test";
    });

    auto resume_result = use_case->ResumeTask(context->task_id);
    terminal_writer.join();

    ASSERT_TRUE(resume_result.IsError());
    EXPECT_EQ(resume_result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(resume_result.GetError().GetMessage().find("failure_stage=resume_confirm"), std::string::npos);
    EXPECT_NE(resume_result.GetError().GetMessage().find("failure_code=CANCELLED"), std::string::npos);
}

TEST(DispensingExecutionUseCaseInternalTest, CleanupExpiredTasksKeepsActiveRunningTask) {
    auto use_case = CreateExecutionUseCase();

    auto running_context = std::make_shared<TaskExecutionContext>();
    running_context->task_id = "task-running-old";
    running_context->state.store(TaskState::RUNNING);
    running_context->start_time = std::chrono::steady_clock::now() - std::chrono::hours(48);
    use_case->tasks_[running_context->task_id] = running_context;
    use_case->active_task_id_ = running_context->task_id;

    auto completed_context = std::make_shared<TaskExecutionContext>();
    completed_context->task_id = "task-completed-old";
    completed_context->state.store(TaskState::COMPLETED);
    completed_context->committed_terminal_state.store(TaskState::COMPLETED);
    completed_context->terminal_committed.store(true);
    completed_context->start_time = std::chrono::steady_clock::now() - std::chrono::hours(3);
    completed_context->end_time = std::chrono::steady_clock::now() - std::chrono::hours(2);
    use_case->tasks_[completed_context->task_id] = completed_context;

    use_case->CleanupExpiredTasks();

    EXPECT_NE(
        use_case->tasks_.find(running_context->task_id),
        use_case->tasks_.end());
    EXPECT_EQ(use_case->active_task_id_, running_context->task_id);
    EXPECT_EQ(
        use_case->tasks_.find(completed_context->task_id),
        use_case->tasks_.end());
}

TEST(DispensingExecutionUseCaseInternalTest, ExecuteAsyncTrimsRedundantMotionTrajectoryPointsFromRetainedRequest) {
    auto scheduler = std::make_shared<CapturingTaskSchedulerPort>();
    auto use_case = CreateExecutionUseCase(std::make_shared<StubDispensingProcessPort>(), scheduler);
    auto request = BuildExecutionRequest(true);

    auto task_result = use_case->ExecuteAsync(request);

    ASSERT_TRUE(task_result.IsSuccess());
    const auto task_id = task_result.Value();
    const auto task_it = use_case->tasks_.find(task_id);
    ASSERT_NE(task_it, use_case->tasks_.end());
    ASSERT_TRUE(static_cast<bool>(task_it->second->request));
    const auto& retained_plan = task_it->second->request->execution_package.execution_plan;
    EXPECT_EQ(retained_plan.interpolation_points.size(), 2U);
    EXPECT_TRUE(retained_plan.motion_trajectory.points.empty());
    EXPECT_FLOAT_EQ(retained_plan.motion_trajectory.total_length, 12.5f);
    EXPECT_FLOAT_EQ(retained_plan.motion_trajectory.total_time, 1.25f);
}

TEST(DispensingExecutionUseCaseInternalTest, ExecuteAsyncKeepsMotionTrajectoryPointsWhenInterpolationPointsUnavailable) {
    auto scheduler = std::make_shared<CapturingTaskSchedulerPort>();
    auto use_case = CreateExecutionUseCase(std::make_shared<StubDispensingProcessPort>(), scheduler);
    auto request = BuildExecutionRequest(false);

    auto task_result = use_case->ExecuteAsync(request);

    ASSERT_TRUE(task_result.IsSuccess());
    const auto task_id = task_result.Value();
    const auto task_it = use_case->tasks_.find(task_id);
    ASSERT_NE(task_it, use_case->tasks_.end());
    ASSERT_TRUE(static_cast<bool>(task_it->second->request));
    const auto& retained_plan = task_it->second->request->execution_package.execution_plan;
    EXPECT_TRUE(retained_plan.interpolation_points.empty());
    EXPECT_EQ(retained_plan.motion_trajectory.points.size(), 2U);
}

TEST(DispensingExecutionUseCaseInternalTest, ExecuteAsyncDropsOlderTerminalTasksBeforeRegisteringNewTask) {
    auto scheduler = std::make_shared<CapturingTaskSchedulerPort>();
    auto use_case = CreateExecutionUseCase(std::make_shared<StubDispensingProcessPort>(), scheduler);

    auto terminal_context = std::make_shared<TaskExecutionContext>();
    terminal_context->task_id = "task-terminal-old";
    terminal_context->state.store(TaskState::COMPLETED);
    terminal_context->committed_terminal_state.store(TaskState::COMPLETED);
    terminal_context->terminal_committed.store(true);
    terminal_context->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(2);
    terminal_context->end_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    use_case->tasks_[terminal_context->task_id] = terminal_context;
    use_case->active_task_id_ = terminal_context->task_id;

    auto task_result = use_case->ExecuteAsync(BuildExecutionRequest(true));

    ASSERT_TRUE(task_result.IsSuccess());
    EXPECT_EQ(use_case->tasks_.find(terminal_context->task_id), use_case->tasks_.end());
    EXPECT_EQ(use_case->active_task_id_, task_result.Value());
}

TEST(DispensingExecutionUseCaseInternalTest, ExecuteAsyncCleansExpiredSchedulerTasksBeforeSchedulerSubmit) {
    auto scheduler = std::make_shared<CapturingTaskSchedulerPort>();
    auto use_case = CreateExecutionUseCase(std::make_shared<StubDispensingProcessPort>(), scheduler);

    auto task_result = use_case->ExecuteAsync(BuildExecutionRequest(true));

    ASSERT_TRUE(task_result.IsSuccess());
    EXPECT_EQ(scheduler->cleanup_calls, 1);
    EXPECT_EQ(scheduler->submit_calls, 1);
    EXPECT_TRUE(static_cast<bool>(scheduler->last_executor));
}

TEST(DispensingExecutionUseCaseInternalTest, ExecuteAsyncStoresSchedulerTaskIdWhenScheduled) {
    auto scheduler = std::make_shared<CapturingTaskSchedulerPort>();
    auto use_case = CreateExecutionUseCase(std::make_shared<StubDispensingProcessPort>(), scheduler);

    auto task_result = use_case->ExecuteAsync(BuildExecutionRequest(true));

    ASSERT_TRUE(task_result.IsSuccess());
    const auto task_id = task_result.Value();
    const auto task_it = use_case->tasks_.find(task_id);
    ASSERT_NE(task_it, use_case->tasks_.end());
    std::lock_guard<std::mutex> lock(task_it->second->mutex_);
    EXPECT_EQ(task_it->second->scheduler_task_id, "scheduler-task-1");
    EXPECT_FALSE(task_it->second->execution_started.load());
}

TEST(DispensingExecutionUseCaseInternalTest, ExecuteAsyncFallsBackToLocalWorkerWhenSchedulerSubmitFails) {
    auto scheduler = std::make_shared<CapturingTaskSchedulerPort>();
    scheduler->submit_error = Siligen::Shared::Types::Error(
        ErrorCode::UNKNOWN_ERROR,
        "submit failed",
        "CapturingTaskSchedulerPort");
    auto use_case = CreateExecutionUseCase(std::make_shared<StubDispensingProcessPort>(), scheduler);

    auto task_result = use_case->ExecuteAsync(BuildExecutionRequest(true));

    ASSERT_TRUE(task_result.IsSuccess());
    const auto task_id = task_result.Value();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < deadline) {
        auto task_it = use_case->tasks_.find(task_id);
        if (task_it != use_case->tasks_.end() && task_it->second->execution_started.load()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const auto task_it = use_case->tasks_.find(task_id);
    ASSERT_NE(task_it, use_case->tasks_.end());
    EXPECT_TRUE(task_it->second->execution_started.load());
    std::lock_guard<std::mutex> lock(task_it->second->mutex_);
    EXPECT_TRUE(task_it->second->scheduler_task_id.empty());
}

TEST(DispensingExecutionUseCaseInternalTest, CancelTaskCancelsPendingSchedulerTaskBeforeExecutionStarts) {
    auto scheduler = std::make_shared<CapturingTaskSchedulerPort>();
    auto use_case = CreateExecutionUseCase(std::make_shared<StubDispensingProcessPort>(), scheduler);

    auto task_result = use_case->ExecuteAsync(BuildExecutionRequest(true));

    ASSERT_TRUE(task_result.IsSuccess());
    const auto task_id = task_result.Value();
    auto cancel_result = use_case->CancelTask(task_id);

    ASSERT_TRUE(cancel_result.IsSuccess()) << cancel_result.GetError().GetMessage();
    EXPECT_TRUE(scheduler->cancelled);

    const auto task_it = use_case->tasks_.find(task_id);
    ASSERT_NE(task_it, use_case->tasks_.end());
    EXPECT_FALSE(task_it->second->execution_started.load());
    EXPECT_TRUE(task_it->second->terminal_committed.load());
    EXPECT_EQ(task_it->second->committed_terminal_state.load(), TaskState::CANCELLED);
}

TEST(DispensingExecutionUseCaseInternalTest, StopJobCommitsCancelledStateImmediatelyAfterTaskCancelConfirmed) {
    auto use_case = CreateExecutionUseCase();

    auto task_context = std::make_shared<TaskExecutionContext>();
    task_context->task_id = "task-stop";
    task_context->state.store(TaskState::RUNNING);
    task_context->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    use_case->tasks_[task_context->task_id] = task_context;
    use_case->active_task_id_ = task_context->task_id;

    auto job_context = std::make_shared<JobExecutionContext>();
    job_context->job_id = "job-stop";
    job_context->plan_id = "plan-stop";
    job_context->plan_fingerprint = "fp-stop";
    job_context->execution_request = std::make_shared<DispensingExecutionRequest>(BuildExecutionRequest(true));
    job_context->state.store(JobState::RUNNING);
    job_context->target_count.store(1);
    job_context->current_cycle.store(1);
    job_context->dry_run = true;
    job_context->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    {
        std::lock_guard<std::mutex> lock(job_context->mutex_);
        job_context->active_task_id = task_context->task_id;
    }
    use_case->jobs_[job_context->job_id] = job_context;
    use_case->active_job_id_ = job_context->job_id;

    std::thread cancel_completion([task_context]() {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        while (!task_context->cancel_requested.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        task_context->state.store(TaskState::CANCELLED);
        task_context->committed_terminal_state.store(TaskState::CANCELLED);
        task_context->terminal_committed.store(true);
        task_context->end_time = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(task_context->mutex_);
        task_context->error_message = "failure_stage=cancel_confirm;failure_code=cancelled;message=执行已取消";
    });

    auto stop_result = use_case->StopJob(job_context->job_id);
    cancel_completion.join();

    ASSERT_TRUE(stop_result.IsSuccess()) << stop_result.GetError().GetMessage();
    EXPECT_EQ(job_context->state.load(), JobState::CANCELLED);
    EXPECT_TRUE(job_context->final_state_committed.load());
    EXPECT_TRUE(use_case->active_job_id_.empty());

    auto job_status_result = use_case->GetJobStatus(job_context->job_id);
    ASSERT_TRUE(job_status_result.IsSuccess());
    EXPECT_EQ(job_status_result.Value().state, "cancelled");
    EXPECT_EQ(job_status_result.Value().error_message, "failure_stage=cancel_confirm;failure_code=cancelled;message=执行已取消");
}

TEST(DispensingExecutionUseCaseInternalTest, TerminalTaskStatusIsReturnedOnceThenReleased) {
    auto use_case = CreateExecutionUseCase();

    auto context = std::make_shared<TaskExecutionContext>();
    context->task_id = "task-terminal";
    context->state.store(TaskState::COMPLETED);
    context->committed_terminal_state.store(TaskState::COMPLETED);
    context->terminal_committed.store(true);
    context->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    context->end_time = std::chrono::steady_clock::now();
    context->total_segments.store(8);
    context->executed_segments.store(8);
    use_case->tasks_[context->task_id] = context;
    use_case->active_task_id_ = context->task_id;

    auto first_status_result = use_case->GetTaskStatus(context->task_id);
    ASSERT_TRUE(first_status_result.IsSuccess());
    EXPECT_EQ(first_status_result.Value().task_id, context->task_id);
    EXPECT_EQ(first_status_result.Value().state, "completed");
    EXPECT_EQ(use_case->tasks_.find(context->task_id), use_case->tasks_.end());
    EXPECT_TRUE(use_case->active_task_id_.empty());

    auto second_status_result = use_case->GetTaskStatus(context->task_id);
    ASSERT_TRUE(second_status_result.IsError());
    EXPECT_EQ(second_status_result.GetError().GetCode(), ErrorCode::INVALID_STATE);
}

TEST(DispensingExecutionUseCaseInternalTest, TerminalJobStatusRemainsReadableUntilCleanup) {
    auto use_case = CreateExecutionUseCase();

    auto context = std::make_shared<JobExecutionContext>();
    context->job_id = "job-terminal";
    context->plan_id = "plan-terminal";
    context->plan_fingerprint = "fp-terminal";
    context->state.store(Siligen::Application::UseCases::Dispensing::JobState::COMPLETED);
    context->final_state_committed.store(true);
    context->target_count.store(1);
    context->completed_count.store(1);
    context->current_cycle.store(1);
    context->current_segment.store(12);
    context->total_segments.store(12);
    context->cycle_progress_percent.store(100);
    context->dry_run = true;
    context->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    context->end_time = std::chrono::steady_clock::now();
    use_case->jobs_[context->job_id] = context;

    auto first_status_result = use_case->GetJobStatus(context->job_id);
    ASSERT_TRUE(first_status_result.IsSuccess());
    EXPECT_EQ(first_status_result.Value().job_id, context->job_id);
    EXPECT_EQ(first_status_result.Value().state, "completed");
    EXPECT_NE(use_case->jobs_.find(context->job_id), use_case->jobs_.end());

    auto second_status_result = use_case->GetJobStatus(context->job_id);
    ASSERT_TRUE(second_status_result.IsSuccess());
    EXPECT_EQ(second_status_result.Value().job_id, context->job_id);
    EXPECT_EQ(second_status_result.Value().state, "completed");
}

TEST(DispensingExecutionUseCaseInternalTest, CleanupTerminalJobsLockedDropsOlderTerminalJobs) {
    auto use_case = CreateExecutionUseCase();

    auto terminal_job = std::make_shared<JobExecutionContext>();
    terminal_job->job_id = "job-terminal-old";
    terminal_job->plan_id = "plan-terminal-old";
    terminal_job->plan_fingerprint = "fp-terminal-old";
    terminal_job->execution_request = std::make_shared<DispensingExecutionRequest>(BuildExecutionRequest(true));
    terminal_job->state.store(JobState::COMPLETED);
    terminal_job->final_state_committed.store(true);
    terminal_job->target_count.store(1);
    terminal_job->completed_count.store(1);
    terminal_job->current_cycle.store(1);
    terminal_job->cycle_progress_percent.store(100);
    terminal_job->dry_run = true;
    terminal_job->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(2);
    terminal_job->end_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    use_case->jobs_[terminal_job->job_id] = terminal_job;
    use_case->active_job_id_ = terminal_job->job_id;

    {
        std::lock_guard<std::mutex> lock(use_case->jobs_mutex_);
        use_case->CleanupTerminalJobsLocked();
    }

    EXPECT_EQ(use_case->jobs_.find(terminal_job->job_id), use_case->jobs_.end());
    EXPECT_TRUE(use_case->active_job_id_.empty());
}

TEST(DispensingExecutionUseCaseInternalTest, DryRunExecutionRequestResolvesToExplicitSafeProfile) {
    DispensingExecutionRequest request;
    request.dry_run = true;

    EXPECT_EQ(request.ResolveMachineMode(), MachineMode::Test);
    EXPECT_EQ(request.ResolveExecutionMode(), JobExecutionMode::ValidationDryCycle);
    EXPECT_EQ(request.ResolveOutputPolicy(), ProcessOutputPolicy::Inhibited);
}

TEST(DispensingExecutionUseCaseInternalTest, PlannedExecutionRequestRequiresTrajectory) {
    DispensingExecutionRequest request;
    request.dispensing_speed_mm_s = 25.0f;
    request.dry_run_speed_mm_s = 80.0f;

    auto validation = request.Validate();
    ASSERT_TRUE(validation.IsError());
    EXPECT_EQ(validation.GetError().GetCode(), ErrorCode::INVALID_PARAMETER);
}
