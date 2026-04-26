#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "runtime_execution/contracts/dispensing/ProfileCompareExecutionCompiler.h"
#include "usecases/dispensing/DispensingExecutionUseCase.Internal.h"
#include "usecases/dispensing/VelocityTracePathPolicy.h"

namespace {

using Siligen::Application::UseCases::Dispensing::DispensingExecutionRequest;
using Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase;
using Siligen::Application::UseCases::Dispensing::ExecutionTransitionState;
using Siligen::Application::UseCases::Dispensing::JobExecutionContext;
using Siligen::Application::UseCases::Dispensing::JobCycleAdvanceMode;
using Siligen::Application::UseCases::Dispensing::JobState;
using Siligen::Application::UseCases::Dispensing::RuntimeJobTraceabilityResponse;
using Siligen::Application::UseCases::Dispensing::RuntimeStartJobRequest;
using Siligen::Application::UseCases::Dispensing::RuntimeJobStatusResponse;
using Siligen::Application::UseCases::Dispensing::RuntimeJobTraceabilityResponse;
using Siligen::Application::UseCases::Dispensing::TaskExecutionContext;
using Siligen::Application::UseCases::Dispensing::TaskState;
using Siligen::Domain::Dispensing::Ports::TaskExecutor;
using Siligen::Domain::Dispensing::Ports::TaskStatus;
using Siligen::Domain::Dispensing::Ports::TaskStatusInfo;
using Siligen::Domain::Dispensing::ValueObjects::ProfileCompareActualTraceItem;
using Siligen::Domain::Dispensing::ValueObjects::ProfileCompareExpectedTrace;
using Siligen::Domain::Dispensing::ValueObjects::ProfileCompareExpectedTraceItem;
using Siligen::Domain::Dispensing::ValueObjects::ProfileCompareTraceabilityMismatch;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionOptions;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionPlan;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionReport;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeOverrides;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeParams;
using Siligen::Domain::Dispensing::ValueObjects::JobExecutionMode;
using Siligen::Domain::Dispensing::ValueObjects::ProcessOutputPolicy;
using Siligen::Domain::Dispensing::ValueObjects::ProductionTriggerMode;
using Siligen::JobIngest::Contracts::DxfInputQuality;
using Siligen::Domain::Machine::ValueObjects::MachineMode;
using Siligen::Domain::Motion::Ports::IMotionStatePort;
using Siligen::Domain::Motion::Ports::MotionState;
using Siligen::Domain::Motion::Ports::MotionStatus;
using Siligen::Domain::Motion::Ports::InterpolationData;
using Siligen::Domain::Motion::Ports::InterpolationType;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint;
using Siligen::RuntimeExecution::Contracts::Dispensing::CompileProfileCompareExecutionSchedule;
using Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareExecutionCompileInput;
using Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareExecutionSchedule;
using Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareRuntimeContract;
using Siligen::Device::Contracts::Commands::DeviceConnection;
using Siligen::Device::Contracts::Ports::DeviceConnectionPort;
using Siligen::Device::Contracts::State::DeviceConnectionSnapshot;
using Siligen::Device::Contracts::State::DeviceConnectionState;
using Siligen::Device::Contracts::State::HeartbeatSnapshot;
using Siligen::Shared::Types::DispensingExecutionGeometryKind;
using Siligen::Shared::Types::DispensingExecutionStrategy;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using RuntimeDispensingProcessPort = Siligen::RuntimeExecution::Contracts::Dispensing::IDispensingProcessPort;
using RuntimeTaskSchedulerPort = Siligen::Domain::Dispensing::Ports::ITaskSchedulerPort;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::uint32;

class StubDispensingProcessPort : public RuntimeDispensingProcessPort {
   public:
    Result<void> ValidateHardwareConnection() noexcept override {
        return Result<void>::Success();
    }

    Result<DispensingRuntimeParams> BuildRuntimeParams(const DispensingRuntimeOverrides&) noexcept override {
        DispensingRuntimeParams params;
        params.dispensing_velocity = 10.0f;
        return Result<DispensingRuntimeParams>::Success(params);
    }

    Result<DispensingExecutionReport> ExecuteProcess(
                                                     const Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated& execution_package,
                                                     const DispensingRuntimeParams&,
                                                     const DispensingExecutionOptions&,
                                                     std::atomic<bool>*,
                                                     std::atomic<bool>*,
                                                     std::atomic<bool>*,
                                                     Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver*)
        noexcept override {
        execute_called = true;
        last_execution_nominal_time_s = execution_package.execution_nominal_time_s;
        const auto& plan = execution_package.execution_plan;
        DispensingExecutionReport report;
        report.executed_segments = static_cast<std::uint32_t>(plan.interpolation_segments.size());
        report.total_distance = plan.total_length_mm;
        return Result<DispensingExecutionReport>::Success(report);
    }

    void StopExecution(std::atomic<bool>*, std::atomic<bool>* = nullptr) noexcept override {}

    bool execute_called = false;
    float32 last_execution_nominal_time_s = 0.0f;
};

class TracingStubDispensingProcessPort final : public StubDispensingProcessPort {
   public:
    Result<DispensingExecutionReport> ExecuteProcess(
                                                     const Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated& execution_package,
                                                     const DispensingRuntimeParams& params,
                                                     const DispensingExecutionOptions& options,
                                                     std::atomic<bool>* stop_flag,
                                                     std::atomic<bool>* pause_flag,
                                                     std::atomic<bool>* pause_applied_flag,
                                                     Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver* observer)
        noexcept override {
        ++execute_process_calls;
        if (execute_process_calls <= scripted_reports.size()) {
            execute_called = true;
            last_execution_nominal_time_s = execution_package.execution_nominal_time_s;
            return Result<DispensingExecutionReport>::Success(scripted_reports[execute_process_calls - 1U]);
        }
        return StubDispensingProcessPort::ExecuteProcess(
            execution_package,
            params,
            options,
            stop_flag,
            pause_flag,
            pause_applied_flag,
            observer);
    }

    std::vector<DispensingExecutionReport> scripted_reports;
    std::size_t execute_process_calls = 0U;
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

class FakeHardwareConnectionPort final : public DeviceConnectionPort {
   public:
    Result<void> Connect(const DeviceConnection&) override { return Result<void>::Success(); }
    Result<void> Disconnect() override { return Result<void>::Success(); }

    Result<DeviceConnectionSnapshot> ReadConnection() const override {
        DeviceConnectionSnapshot snapshot;
        snapshot.state = connected ? DeviceConnectionState::Connected : DeviceConnectionState::Disconnected;
        return Result<DeviceConnectionSnapshot>::Success(snapshot);
    }

    bool IsConnected() const override { return connected; }
    Result<void> Reconnect() override { return Result<void>::Success(); }
    void SetConnectionStateCallback(std::function<void(const DeviceConnectionSnapshot&)>) override {}
    Result<void> StartStatusMonitoring(std::uint32_t = 1000) override { return Result<void>::Success(); }
    void StopStatusMonitoring() override {}
    std::string GetLastError() const override { return {}; }
    void ClearError() override {}
    Result<void> StartHeartbeat(const HeartbeatSnapshot&) override { return Result<void>::Success(); }
    void StopHeartbeat() override {}
    HeartbeatSnapshot ReadHeartbeat() const override { return {}; }
    Result<bool> Ping() const override { return Result<bool>::Success(connected); }

    bool connected = true;
};

class FakeMotionStatePort final : public IMotionStatePort {
   public:
    Result<Point2D> GetCurrentPosition() const override { return Result<Point2D>::Success(status.position); }

    Result<float32> GetAxisPosition(LogicalAxisId axis) const override {
        return Result<float32>::Success(axis == LogicalAxisId::Y ? status.position.y : status.position.x);
    }

    Result<float32> GetAxisVelocity(LogicalAxisId) const override { return Result<float32>::Success(status.velocity); }

    Result<MotionStatus> GetAxisStatus(LogicalAxisId axis) const override {
        MotionStatus axis_status = status;
        axis_status.axis_position_mm = axis == LogicalAxisId::Y ? status.position.y : status.position.x;
        return Result<MotionStatus>::Success(axis_status);
    }

    Result<bool> IsAxisMoving(LogicalAxisId) const override {
        return Result<bool>::Success(status.state == MotionState::MOVING);
    }

    Result<bool> IsAxisInPosition(LogicalAxisId) const override {
        return Result<bool>::Success(status.in_position);
    }

    Result<std::vector<MotionStatus>> GetAllAxesStatus() const override {
        MotionStatus x_status = status;
        x_status.axis_position_mm = status.position.x;
        MotionStatus y_status = status;
        y_status.axis_position_mm = status.position.y;
        return Result<std::vector<MotionStatus>>::Success({x_status, y_status});
    }

    MotionStatus status{};
};

DispensingExecutionRequest BuildExecutionRequest(bool include_interpolation_points) {
    DispensingExecutionRequest request;
    request.dispensing_speed_mm_s = 10.0f;

    auto& package = request.execution_package;
    package.total_length_mm = 12.5f;
    package.execution_nominal_time_s = 1.25f;

    auto& plan = package.execution_plan;
    plan.total_length_mm = package.total_length_mm;

    if (include_interpolation_points) {
        plan.interpolation_points.emplace_back(0.0f, 0.0f, 10.0f);
        plan.interpolation_points.emplace_back(12.5f, 0.0f, 10.0f);
        plan.interpolation_points.front().timestamp = 0.0f;
        plan.interpolation_points.back().timestamp = 1.25f;
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

std::shared_ptr<ProfileCompareExpectedTrace> BuildExpectedTraceFromPlanAndSchedule(
    const DispensingExecutionPlan& plan,
    const ProfileCompareExecutionSchedule& schedule) {
    auto trace = std::make_shared<ProfileCompareExpectedTrace>();
    trace->items.reserve(plan.profile_compare_program.trigger_points.size());

    for (const auto& schedule_span : schedule.spans) {
        const auto future_compare_offset = schedule_span.trigger_begin_index + schedule_span.start_boundary_trigger_count;
        for (uint32 trigger_index = schedule_span.trigger_begin_index;
             trigger_index <= schedule_span.trigger_end_index;
             ++trigger_index) {
            const auto& owner_trigger = plan.profile_compare_program.trigger_points[trigger_index];
            ProfileCompareExpectedTraceItem item;
            item.cycle_index = 1U;
            item.trigger_sequence_id = owner_trigger.sequence_index;
            item.authority_trigger_index = owner_trigger.sequence_index;
            item.span_index = schedule_span.span_index;
            item.execution_interpolation_index = plan.interpolation_points.empty()
                ? 0U
                : std::min<uint32>(
                      trigger_index,
                      static_cast<uint32>(plan.interpolation_points.size() - 1U));
            item.authority_distance_mm = owner_trigger.profile_position_mm;
            item.execution_profile_position_mm = owner_trigger.profile_position_mm;
            item.execution_position_mm = owner_trigger.trigger_position_mm;
            item.execution_trigger_position_mm = owner_trigger.trigger_position_mm;
            item.compare_source_axis = schedule_span.compare_source_axis;
            item.pulse_width_us = owner_trigger.pulse_width_us;
            item.authority_trigger_ref = "trigger-" + std::to_string(owner_trigger.sequence_index);
            item.authority_span_ref = "span-" + std::to_string(schedule_span.span_index);
            if (trigger_index == schedule_span.trigger_begin_index &&
                schedule_span.start_boundary_trigger_count == 1U) {
                item.trigger_mode = "start_boundary";
                item.compare_position_pulse = 0L;
            } else {
                const auto future_compare_index =
                    static_cast<std::size_t>(trigger_index - future_compare_offset);
                item.trigger_mode = "future_compare";
                item.compare_position_pulse = schedule_span.compare_positions_pulse[future_compare_index];
            }
            trace->items.push_back(std::move(item));
        }
    }

    return trace;
}

DispensingExecutionRequest BuildProfileCompareExecutionRequest() {
    auto request = BuildExecutionRequest(true);
    auto& plan = request.execution_package.execution_plan;
    plan.production_trigger_mode = ProductionTriggerMode::PROFILE_COMPARE;
    plan.profile_compare_program.expected_trigger_count = 2U;
    plan.profile_compare_program.trigger_points = {
        {0U, 0.0f, Point2D{0.0f, 0.0f}, 2000U},
        {1U, 12.5f, Point2D{12.5f, 0.0f}, 2000U},
    };
    plan.profile_compare_program.spans = {
        {0U, 1U, 1, 1U, 0.0f, 12.5f, Point2D{0.0f, 0.0f}, Point2D{12.5f, 0.0f}},
    };
    plan.completion_contract.enabled = true;
    plan.completion_contract.final_target_position_mm = Point2D{12.5f, 0.0f};
    plan.completion_contract.final_position_tolerance_mm = 0.2f;
    plan.completion_contract.expected_trigger_count = 2U;

    ProfileCompareExecutionCompileInput compile_input{
        plan,
        ProfileCompareRuntimeContract{100.0f, 0x03, 1000U},
    };
    const auto schedule_result = CompileProfileCompareExecutionSchedule(compile_input);
    if (schedule_result.IsError()) {
        throw std::runtime_error(schedule_result.GetError().GetMessage());
    }

    request.profile_compare_schedule =
        std::make_shared<ProfileCompareExecutionSchedule>(schedule_result.Value());
    request.expected_trace = BuildExpectedTraceFromPlanAndSchedule(
        plan,
        *request.profile_compare_schedule);
    return request;
}

ProfileCompareActualTraceItem BuildActualTraceItem(
    const ProfileCompareExpectedTraceItem& expected_item,
    uint32 completion_sequence,
    uint32 local_completed_trigger_count,
    uint32 observed_completed_trigger_count) {
    ProfileCompareActualTraceItem item;
    item.cycle_index = expected_item.cycle_index;
    item.trigger_sequence_id = expected_item.trigger_sequence_id;
    item.completion_sequence = completion_sequence;
    item.span_index = expected_item.span_index;
    item.local_completed_trigger_count = local_completed_trigger_count;
    item.observed_completed_trigger_count = observed_completed_trigger_count;
    item.compare_source_axis = expected_item.compare_source_axis;
    item.compare_position_pulse = expected_item.compare_position_pulse;
    item.authority_trigger_ref = expected_item.authority_trigger_ref;
    item.trigger_mode = expected_item.trigger_mode;
    return item;
}

std::optional<RuntimeJobStatusResponse> WaitForTerminalJobStatus(
    DispensingExecutionUseCase::Impl& use_case,
    const std::string& job_id,
    std::chrono::milliseconds timeout = std::chrono::seconds(2)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        auto status_result = use_case.GetJobStatus(job_id);
        if (status_result.IsSuccess()) {
            const auto& status = status_result.Value();
            if (status.state == "completed" ||
                status.state == "failed" ||
                status.state == "cancelled") {
                return status;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return std::nullopt;
}

std::unique_ptr<DispensingExecutionUseCase::Impl> CreateExecutionUseCase(
    std::shared_ptr<RuntimeDispensingProcessPort> process_port = std::make_shared<StubDispensingProcessPort>(),
    std::shared_ptr<RuntimeTaskSchedulerPort> task_scheduler_port = nullptr,
    std::shared_ptr<DeviceConnectionPort> connection_port = std::make_shared<FakeHardwareConnectionPort>(),
    std::shared_ptr<IMotionStatePort> motion_state_port = [] {
        auto port = std::make_shared<FakeMotionStatePort>();
        port->status.state = MotionState::HOMED;
        port->status.homing_state = "homed";
        port->status.position = Point2D{0.0f, 0.0f};
        port->status.velocity = 0.0f;
        port->status.in_position = true;
        port->status.enabled = true;
        return port;
    }()) {
    return std::make_unique<DispensingExecutionUseCase::Impl>(
        nullptr,
        nullptr,
        std::move(motion_state_port),
        std::move(connection_port),
        nullptr,
        std::move(process_port),
        nullptr,
        std::move(task_scheduler_port),
        nullptr,
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

TEST(DispensingExecutionUseCaseInternalTest, ExecuteAsyncForwardsAuthorityEstimatedTimeToProcessPort) {
    auto scheduler = std::make_shared<CapturingTaskSchedulerPort>();
    auto process_port = std::make_shared<StubDispensingProcessPort>();
    auto use_case = CreateExecutionUseCase(process_port, scheduler);
    auto request = BuildExecutionRequest(true);
    request.execution_package.execution_nominal_time_s = 3.75f;
    request.execution_package.execution_plan.motion_trajectory.total_time = 0.4f;
    request.execution_package.execution_plan.interpolation_points.back().timestamp = 0.4f;

    auto task_result = use_case->ExecuteAsync(request);

    ASSERT_TRUE(task_result.IsSuccess());
    ASSERT_TRUE(static_cast<bool>(scheduler->last_executor));
    scheduler->last_executor();
    EXPECT_TRUE(process_port->execute_called);
    EXPECT_FLOAT_EQ(process_port->last_execution_nominal_time_s, 3.75f);
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

TEST(DispensingExecutionUseCaseInternalTest, RequestJobTransitionReturnsCancelingSnapshotAndStatus) {
    auto use_case = CreateExecutionUseCase();

    auto task_context = std::make_shared<TaskExecutionContext>();
    task_context->task_id = "task-cancel";
    task_context->state.store(TaskState::RUNNING);
    task_context->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    use_case->tasks_[task_context->task_id] = task_context;
    use_case->active_task_id_ = task_context->task_id;

    auto job_context = std::make_shared<JobExecutionContext>();
    job_context->job_id = "job-cancel";
    job_context->plan_id = "plan-cancel";
    job_context->plan_fingerprint = "fp-cancel";
    job_context->execution_request = std::make_shared<DispensingExecutionRequest>(BuildExecutionRequest(true));
    job_context->state.store(JobState::RUNNING);
    job_context->requested_transition_state.store(ExecutionTransitionState::RUNNING);
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

    auto transition_result = use_case->RequestJobTransition(job_context->job_id, ExecutionTransitionState::CANCELING);
    cancel_completion.join();

    ASSERT_TRUE(transition_result.IsSuccess()) << transition_result.GetError().GetMessage();
    EXPECT_TRUE(transition_result.Value().accepted);
    EXPECT_EQ(transition_result.Value().job_id, job_context->job_id);
    EXPECT_EQ(transition_result.Value().transition_state, ExecutionTransitionState::CANCELING);
    EXPECT_EQ(job_context->state.load(), JobState::STOPPING);
    EXPECT_EQ(job_context->requested_transition_state.load(), ExecutionTransitionState::CANCELING);

    auto job_status_result = use_case->GetJobStatus(job_context->job_id);
    ASSERT_TRUE(job_status_result.IsSuccess());
    EXPECT_EQ(job_status_result.Value().transition_state, ExecutionTransitionState::CANCELING);
    EXPECT_EQ(job_status_result.Value().state, "stopping");
}

TEST(DispensingExecutionUseCaseInternalTest, PauseJobKeepsCoarseRunningStateWhileTransitionStateIsPausing) {
    auto use_case = CreateExecutionUseCase();

    auto job_context = std::make_shared<JobExecutionContext>();
    job_context->job_id = "job-pausing";
    job_context->plan_id = "plan-pausing";
    job_context->plan_fingerprint = "fp-pausing";
    auto execution_request = std::make_shared<DispensingExecutionRequest>(BuildExecutionRequest(true));
    execution_request->dry_run = true;
    job_context->execution_request = execution_request;
    job_context->state.store(JobState::RUNNING);
    job_context->requested_transition_state.store(ExecutionTransitionState::RUNNING);
    job_context->dry_run = true;
    job_context->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    use_case->jobs_[job_context->job_id] = job_context;

    auto pause_result = use_case->PauseJob(job_context->job_id);

    ASSERT_TRUE(pause_result.IsSuccess()) << pause_result.GetError().GetMessage();

    auto job_status_result = use_case->GetJobStatus(job_context->job_id);
    ASSERT_TRUE(job_status_result.IsSuccess());
    EXPECT_EQ(job_status_result.Value().state, "running");
    EXPECT_EQ(job_status_result.Value().transition_state, ExecutionTransitionState::PAUSING);
}

TEST(DispensingExecutionUseCaseInternalTest, ResumeJobClearsPendingPauseBeforeTaskPauseApplies) {
    auto use_case = CreateExecutionUseCase();

    auto job_context = std::make_shared<JobExecutionContext>();
    job_context->job_id = "job-resume-pending-pause";
    job_context->plan_id = "plan-resume-pending-pause";
    job_context->plan_fingerprint = "fp-resume-pending-pause";
    auto execution_request = std::make_shared<DispensingExecutionRequest>(BuildExecutionRequest(true));
    execution_request->dry_run = true;
    job_context->execution_request = execution_request;
    job_context->state.store(JobState::RUNNING);
    job_context->requested_transition_state.store(ExecutionTransitionState::RUNNING);
    job_context->pause_requested.store(true);
    job_context->dry_run = true;
    job_context->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    use_case->jobs_[job_context->job_id] = job_context;

    auto resume_result = use_case->ResumeJob(job_context->job_id);

    ASSERT_TRUE(resume_result.IsSuccess()) << resume_result.GetError().GetMessage();

    auto job_status_result = use_case->GetJobStatus(job_context->job_id);
    ASSERT_TRUE(job_status_result.IsSuccess());
    EXPECT_EQ(job_status_result.Value().state, "running");
    EXPECT_EQ(job_status_result.Value().transition_state, ExecutionTransitionState::RUNNING);
}

TEST(DispensingExecutionUseCaseInternalTest, ContinueJobMarksAwaitingContinueJobAsReadyToAdvance) {
    auto use_case = CreateExecutionUseCase();

    auto job_context = std::make_shared<JobExecutionContext>();
    job_context->job_id = "job-awaiting-continue";
    job_context->plan_id = "plan-awaiting-continue";
    job_context->plan_fingerprint = "fp-awaiting-continue";
    auto execution_request = std::make_shared<DispensingExecutionRequest>(BuildExecutionRequest(true));
    execution_request->dry_run = true;
    job_context->execution_request = execution_request;
    job_context->state.store(JobState::WAITING_CONTINUE);
    job_context->requested_transition_state.store(ExecutionTransitionState::AWAITING_CONTINUE);
    job_context->target_count.store(2U);
    job_context->current_cycle.store(1U);
    job_context->dry_run = true;
    job_context->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    use_case->jobs_[job_context->job_id] = job_context;

    auto continue_result = use_case->ContinueJob(job_context->job_id);

    ASSERT_TRUE(continue_result.IsSuccess()) << continue_result.GetError().GetMessage();

    const auto job_it = use_case->jobs_.find(job_context->job_id);
    ASSERT_NE(job_it, use_case->jobs_.end());
    EXPECT_TRUE(job_it->second->continue_requested.load());

    auto job_status_result = use_case->GetJobStatus(job_context->job_id);
    ASSERT_TRUE(job_status_result.IsSuccess());
    EXPECT_EQ(job_status_result.Value().state, "awaiting_continue");
    EXPECT_EQ(job_status_result.Value().transition_state, ExecutionTransitionState::AWAITING_CONTINUE);
}

TEST(DispensingExecutionUseCaseInternalTest, StopJobLeavesJobInStoppingTransitionAfterTaskCancelConfirmed) {
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
    EXPECT_EQ(job_context->state.load(), JobState::STOPPING);
    EXPECT_FALSE(job_context->final_state_committed.load());
    EXPECT_EQ(use_case->active_job_id_, job_context->job_id);

    auto job_status_result = use_case->GetJobStatus(job_context->job_id);
    ASSERT_TRUE(job_status_result.IsSuccess());
    EXPECT_EQ(job_status_result.Value().state, "stopping");
    EXPECT_EQ(job_status_result.Value().error_message, "");
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

TEST(DispensingExecutionUseCaseInternalTest, GetJobTraceabilityRejectsUnknownJob) {
    auto use_case = CreateExecutionUseCase();

    auto traceability_result = use_case->GetJobTraceability("job-missing");

    ASSERT_TRUE(traceability_result.IsError());
    EXPECT_EQ(traceability_result.GetError().GetCode(), ErrorCode::NOT_FOUND);
}

TEST(DispensingExecutionUseCaseInternalTest, GetJobTraceabilityRejectsNonTerminalJob) {
    auto use_case = CreateExecutionUseCase();

    auto context = std::make_shared<JobExecutionContext>();
    context->job_id = "job-running";
    context->plan_id = "plan-running";
    context->plan_fingerprint = "fp-running";
    context->state.store(JobState::RUNNING);
    use_case->jobs_[context->job_id] = context;

    auto traceability_result = use_case->GetJobTraceability(context->job_id);

    ASSERT_TRUE(traceability_result.IsError());
    EXPECT_EQ(traceability_result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(traceability_result.GetError().GetMessage().find("terminal state"), std::string::npos);
}

TEST(DispensingExecutionUseCaseInternalTest, GetJobTraceabilityRejectsTerminalJobWithoutTraceabilityPayload) {
    auto use_case = CreateExecutionUseCase();

    auto context = std::make_shared<JobExecutionContext>();
    context->job_id = "job-no-trace";
    context->plan_id = "plan-no-trace";
    context->plan_fingerprint = "fp-no-trace";
    context->state.store(JobState::COMPLETED);
    context->final_state_committed.store(true);
    context->end_time = std::chrono::steady_clock::now();
    use_case->jobs_[context->job_id] = context;

    auto traceability_result = use_case->GetJobTraceability(context->job_id);

    ASSERT_TRUE(traceability_result.IsError());
    EXPECT_EQ(traceability_result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(traceability_result.GetError().GetMessage().find("does not carry"), std::string::npos);
}

TEST(DispensingExecutionUseCaseInternalTest, GetJobTraceabilityReturnsTerminalTraceabilitySnapshot) {
    auto use_case = CreateExecutionUseCase();

    auto context = std::make_shared<JobExecutionContext>();
    context->job_id = "job-trace";
    context->plan_id = "plan-trace";
    context->plan_fingerprint = "fp-trace";
    context->state.store(JobState::COMPLETED);
    context->final_state_committed.store(true);
    context->end_time = std::chrono::steady_clock::now();
    context->traceability_verdict = "passed";
    context->strict_one_to_one_proven = true;

    ProfileCompareExpectedTraceItem expected_item;
    expected_item.cycle_index = 1U;
    expected_item.trigger_sequence_id = 7U;
    expected_item.authority_trigger_ref = "trigger-7";
    expected_item.trigger_mode = "future_compare";
    expected_item.compare_position_pulse = 2400L;
    context->expected_trace.push_back(expected_item);

    ProfileCompareActualTraceItem actual_item;
    actual_item.cycle_index = 1U;
    actual_item.trigger_sequence_id = 7U;
    actual_item.completion_sequence = 1U;
    actual_item.authority_trigger_ref = "trigger-7";
    actual_item.trigger_mode = "future_compare";
    actual_item.compare_position_pulse = 2400L;
    context->actual_trace.push_back(actual_item);

    use_case->jobs_[context->job_id] = context;

    auto traceability_result = use_case->GetJobTraceability(context->job_id);

    ASSERT_TRUE(traceability_result.IsSuccess()) << traceability_result.GetError().GetMessage();
    const auto& response = traceability_result.Value();
    EXPECT_EQ(response.job_id, context->job_id);
    EXPECT_EQ(response.plan_id, context->plan_id);
    EXPECT_EQ(response.terminal_state, "completed");
    ASSERT_EQ(response.expected_trace.size(), 1U);
    EXPECT_EQ(response.expected_trace.front().trigger_sequence_id, 7U);
    ASSERT_EQ(response.actual_trace.size(), 1U);
    EXPECT_EQ(response.actual_trace.front().completion_sequence, 1U);
    EXPECT_TRUE(response.mismatches.empty());
    EXPECT_EQ(response.verdict, "passed");
    EXPECT_TRUE(response.verdict_reason.empty());
    EXPECT_TRUE(response.strict_one_to_one_proven);
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

TEST(DispensingExecutionUseCaseInternalTest, GetJobTraceabilityReturnsCompletedTerminalPayload) {
    auto use_case = CreateExecutionUseCase();
    const auto request = BuildProfileCompareExecutionRequest();

    auto job_context = std::make_shared<JobExecutionContext>();
    job_context->job_id = "job-trace-completed";
    job_context->plan_id = "plan-trace-completed";
    job_context->plan_fingerprint = "fp-trace-completed";
    job_context->execution_request = std::make_shared<DispensingExecutionRequest>(request);
    job_context->production_baseline.baseline_id = "baseline-trace-completed";
    job_context->production_baseline.baseline_fingerprint = "baseline-fp-trace-completed";
    job_context->input_quality.report_id = "input-quality-report-trace-completed";
    job_context->input_quality.classification = "success";
    job_context->input_quality.production_ready = true;
    job_context->state.store(JobState::COMPLETED);
    job_context->final_state_committed.store(true);
    job_context->target_count.store(1);
    job_context->completed_count.store(1);
    job_context->current_cycle.store(1);
    job_context->cycle_progress_percent.store(100);
    {
        std::lock_guard<std::mutex> lock(job_context->mutex_);
        job_context->expected_trace = request.expected_trace->items;
        job_context->actual_trace = {
            BuildActualTraceItem(request.expected_trace->items[0], 1U, 1U, 2U),
            BuildActualTraceItem(request.expected_trace->items[1], 2U, 2U, 2U),
        };
        job_context->traceability_verdict = "passed";
        job_context->traceability_verdict_reason.clear();
        job_context->strict_one_to_one_proven = true;
    }
    use_case->jobs_[job_context->job_id] = job_context;

    const auto traceability_result = use_case->GetJobTraceability(job_context->job_id);

    ASSERT_TRUE(traceability_result.IsSuccess()) << traceability_result.GetError().GetMessage();
    const auto& response = traceability_result.Value();
    EXPECT_EQ(response.job_id, job_context->job_id);
    EXPECT_EQ(response.plan_id, job_context->plan_id);
    EXPECT_EQ(response.plan_fingerprint, job_context->plan_fingerprint);
    EXPECT_EQ(response.terminal_state, "completed");
    ASSERT_EQ(response.expected_trace.size(), 2U);
    ASSERT_EQ(response.actual_trace.size(), 2U);
    EXPECT_EQ(response.actual_trace[0].completion_sequence, 1U);
    EXPECT_EQ(response.actual_trace[1].completion_sequence, 2U);
    EXPECT_EQ(response.verdict, "passed");
    EXPECT_TRUE(response.verdict_reason.empty());
    EXPECT_TRUE(response.strict_one_to_one_proven);
    EXPECT_EQ(response.production_baseline.baseline_id, "baseline-trace-completed");
    EXPECT_EQ(response.production_baseline.baseline_fingerprint, "baseline-fp-trace-completed");
    EXPECT_EQ(response.input_quality.report_id, "input-quality-report-trace-completed");
    EXPECT_EQ(response.input_quality.classification, "success");
    EXPECT_TRUE(response.input_quality.production_ready);
}

TEST(DispensingExecutionUseCaseInternalTest, GetJobTraceabilityFailsClosedForCancelledTerminal) {
    auto use_case = CreateExecutionUseCase();
    const auto request = BuildProfileCompareExecutionRequest();

    auto job_context = std::make_shared<JobExecutionContext>();
    job_context->job_id = "job-trace-cancelled";
    job_context->plan_id = "plan-trace-cancelled";
    job_context->plan_fingerprint = "fp-trace-cancelled";
    job_context->execution_request = std::make_shared<DispensingExecutionRequest>(request);
    job_context->state.store(JobState::CANCELLED);
    job_context->final_state_committed.store(true);
    job_context->target_count.store(1);
    job_context->completed_count.store(0);
    job_context->current_cycle.store(1);
    {
        std::lock_guard<std::mutex> lock(job_context->mutex_);
        job_context->expected_trace = request.expected_trace->items;
        job_context->traceability_verdict = "failed";
        job_context->traceability_verdict_reason =
            "job terminated in state cancelled before strict traceability could be proven";
        job_context->strict_one_to_one_proven = false;
    }
    use_case->jobs_[job_context->job_id] = job_context;

    const auto traceability_result = use_case->GetJobTraceability(job_context->job_id);

    ASSERT_TRUE(traceability_result.IsSuccess()) << traceability_result.GetError().GetMessage();
    const auto& response = traceability_result.Value();
    EXPECT_EQ(response.terminal_state, "cancelled");
    EXPECT_EQ(response.verdict, "failed");
    EXPECT_FALSE(response.strict_one_to_one_proven);
    EXPECT_NE(response.verdict_reason.find("cancelled"), std::string::npos);
    ASSERT_EQ(response.expected_trace.size(), request.expected_trace->items.size());
    EXPECT_TRUE(response.actual_trace.empty());
}

TEST(DispensingExecutionUseCaseInternalTest,
     StartJobAggregatesMultiCycleTraceAndReturnsInsufficientEvidenceWhenActualTraceCountIsIncomplete) {
    auto process_port = std::make_shared<TracingStubDispensingProcessPort>();
    const auto request = BuildProfileCompareExecutionRequest();

    DispensingExecutionReport first_cycle_report;
    first_cycle_report.executed_segments = 1U;
    first_cycle_report.total_distance = 12.5f;
    first_cycle_report.actual_trace = {
        BuildActualTraceItem(request.expected_trace->items[0], 1U, 1U, 1U),
    };
    first_cycle_report.traceability_verdict = "passed";
    first_cycle_report.strict_one_to_one_proven = true;

    DispensingExecutionReport second_cycle_report;
    second_cycle_report.executed_segments = 1U;
    second_cycle_report.total_distance = 12.5f;
    second_cycle_report.traceability_verdict = "passed";
    second_cycle_report.strict_one_to_one_proven = true;

    process_port->scripted_reports = {first_cycle_report, second_cycle_report};
    auto use_case = CreateExecutionUseCase(process_port);

    RuntimeStartJobRequest start_request;
    start_request.plan_id = "plan-trace-multi";
    start_request.execution_request = request;
    start_request.plan_fingerprint = "fp-trace-multi";
    start_request.target_count = 2U;
    start_request.cycle_advance_mode = JobCycleAdvanceMode::AUTO_CONTINUE;
    start_request.production_baseline.baseline_id = "baseline-trace-multi";
    start_request.production_baseline.baseline_fingerprint = "baseline-fp-trace-multi";
    start_request.input_quality.report_id = "input-quality-report-trace-multi";
    start_request.input_quality.classification = "success";
    start_request.input_quality.production_ready = true;

    const auto start_result = use_case->StartJob(start_request);
    ASSERT_TRUE(start_result.IsSuccess()) << start_result.GetError().GetMessage();

    const auto terminal_status = WaitForTerminalJobStatus(*use_case, start_result.Value());
    ASSERT_TRUE(terminal_status.has_value());
    EXPECT_EQ(terminal_status->state, "completed");
    EXPECT_EQ(terminal_status->completed_count, 2U);

    const auto traceability_result = use_case->GetJobTraceability(start_result.Value());
    ASSERT_TRUE(traceability_result.IsSuccess()) << traceability_result.GetError().GetMessage();
    const auto& response = traceability_result.Value();

    EXPECT_EQ(response.terminal_state, "completed");
    EXPECT_EQ(response.production_baseline.baseline_id, "baseline-trace-multi");
    EXPECT_EQ(response.production_baseline.baseline_fingerprint, "baseline-fp-trace-multi");
    EXPECT_EQ(response.input_quality.report_id, "input-quality-report-trace-multi");
    EXPECT_EQ(response.input_quality.classification, "success");
    EXPECT_TRUE(response.input_quality.production_ready);
    EXPECT_EQ(response.expected_trace.size(), 4U);
    EXPECT_EQ(response.actual_trace.size(), 1U);
    EXPECT_EQ(response.verdict, "insufficient_evidence");
    EXPECT_FALSE(response.strict_one_to_one_proven);
    EXPECT_NE(
        response.verdict_reason.find("actual trace count does not match expected trace count at terminal state"),
        std::string::npos);
    ASSERT_FALSE(response.mismatches.empty());
    EXPECT_EQ(response.mismatches.back().code, "actual_trace_count_mismatch");
    EXPECT_EQ(
        response.mismatches.back().message,
        "actual trace count does not match expected trace count at terminal state");
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

TEST(DispensingExecutionUseCaseInternalTest, StationaryPointExecutionRequestIsAccepted) {
    auto use_case = CreateExecutionUseCase();

    DispensingExecutionRequest request;
    request.dispensing_speed_mm_s = 25.0f;
    request.dry_run_speed_mm_s = 80.0f;
    request.execution_package.total_length_mm = 0.0f;
    request.execution_package.execution_nominal_time_s = 0.05f;

    auto& plan = request.execution_package.execution_plan;
    plan.geometry_kind = DispensingExecutionGeometryKind::POINT;
    plan.execution_strategy = DispensingExecutionStrategy::STATIONARY_SHOT;
    plan.total_length_mm = 0.0f;

    MotionTrajectoryPoint point;
    point.t = 0.0f;
    point.position = {5.0f, 5.0f, 0.0f};
    point.velocity = {0.0f, 0.0f, 0.0f};
    plan.motion_trajectory.points.push_back(point);

    auto result = use_case->Execute(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().total_segments, 1U);
}

TEST(DispensingExecutionUseCaseInternalTest, PointFlyingExecutionRequestIsAccepted) {
    auto use_case = CreateExecutionUseCase();

    DispensingExecutionRequest request;
    request.dispensing_speed_mm_s = 25.0f;
    request.dry_run_speed_mm_s = 80.0f;
    request.execution_package.total_length_mm = 3.0f;
    request.execution_package.execution_nominal_time_s = 0.12f;

    auto& plan = request.execution_package.execution_plan;
    plan.geometry_kind = DispensingExecutionGeometryKind::POINT;
    plan.execution_strategy = DispensingExecutionStrategy::FLYING_SHOT;
    plan.trigger_distances_mm = {0.0f};
    plan.total_length_mm = 3.0f;

    MotionTrajectoryPoint start_point;
    start_point.t = 0.0f;
    start_point.position = {5.0f, 5.0f, 0.0f};
    start_point.velocity = {25.0f, 0.0f, 0.0f};
    start_point.dispense_on = true;

    MotionTrajectoryPoint end_point;
    end_point.t = 0.12f;
    end_point.position = {8.0f, 5.0f, 0.0f};
    end_point.velocity = {0.0f, 0.0f, 0.0f};
    end_point.dispense_on = true;

    plan.motion_trajectory.points = {start_point, end_point};
    plan.motion_trajectory.total_length = 3.0f;
    plan.motion_trajectory.total_time = 0.12f;

    Siligen::TrajectoryPoint preview_start;
    preview_start.position = Point2D{5.0f, 5.0f};
    preview_start.sequence_id = 0U;
    preview_start.timestamp = 0.0f;
    preview_start.velocity = 25.0f;
    preview_start.enable_position_trigger = true;
    preview_start.trigger_position_mm = 0.0f;

    Siligen::TrajectoryPoint preview_end;
    preview_end.position = Point2D{8.0f, 5.0f};
    preview_end.sequence_id = 1U;
    preview_end.timestamp = 0.12f;
    preview_end.velocity = 0.0f;

    plan.interpolation_points = {preview_start, preview_end};

    InterpolationData segment;
    segment.type = InterpolationType::LINEAR;
    segment.positions = {8.0f, 5.0f};
    segment.velocity = 25.0f;
    segment.acceleration = 100.0f;
    segment.end_velocity = 0.0f;
    plan.interpolation_segments.push_back(segment);

    const auto result = use_case->Execute(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().total_segments, 1U);
}

TEST(DispensingExecutionUseCaseInternalTest, ValidateExecutionPreconditionsRejectsDisconnectedProductionRun) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    connection_port->connected = false;
    auto use_case = CreateExecutionUseCase(
        std::make_shared<StubDispensingProcessPort>(),
        nullptr,
        connection_port);

    const auto result = use_case->ValidateExecutionPreconditions(false);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::HARDWARE_NOT_CONNECTED);
}

TEST(DispensingExecutionUseCaseInternalTest, ValidateExecutionPreconditionsAllowsDisconnectedDryRun) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    connection_port->connected = false;
    auto use_case = CreateExecutionUseCase(
        std::make_shared<StubDispensingProcessPort>(),
        nullptr,
        connection_port);

    const auto result = use_case->ValidateExecutionPreconditions(true);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
}

TEST(DispensingExecutionUseCaseInternalTest, ValidateExecutionPreconditionsRejectsUnhomedAxis) {
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    motion_state_port->status.state = MotionState::MOVING;
    motion_state_port->status.position = Point2D{0.0f, 0.0f};
    motion_state_port->status.velocity = 1.0f;
    motion_state_port->status.in_position = false;
    motion_state_port->status.enabled = true;

    auto use_case = CreateExecutionUseCase(
        std::make_shared<StubDispensingProcessPort>(),
        nullptr,
        std::make_shared<FakeHardwareConnectionPort>(),
        motion_state_port);

    const auto result = use_case->ValidateExecutionPreconditions(false);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::AXIS_NOT_HOMED);
}

TEST(DispensingExecutionUseCaseInternalTest, VelocityTracePathPolicyRejectsAbsoluteConfiguredPath) {
    const auto source_dir =
        std::filesystem::temp_directory_path() / "siligen-runtime-execution-velocity-trace-absolute";
    std::filesystem::create_directories(source_dir);
    const auto source_path = (source_dir / "sample.dxf").string();
    const auto configured_path = (source_dir / "absolute.csv").string();

    const auto result =
        Siligen::Application::UseCases::Dispensing::VelocityTracePathPolicy::ResolveOutputPath(
            source_path,
            configured_path);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::FILE_PATH_INVALID);
    EXPECT_NE(result.GetError().GetMessage().find("absolute velocity trace path"), std::string::npos);
}

TEST(DispensingExecutionUseCaseInternalTest, VelocityTracePathPolicyRejectsEscapingParentDirectory) {
    const auto source_dir =
        std::filesystem::temp_directory_path() / "siligen-runtime-execution-velocity-trace-escape";
    std::filesystem::create_directories(source_dir);
    const auto source_path = (source_dir / "sample.dxf").string();

    const auto result =
        Siligen::Application::UseCases::Dispensing::VelocityTracePathPolicy::ResolveOutputPath(
            source_path,
            "..\\escape");

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::FILE_PATH_INVALID);
    EXPECT_NE(result.GetError().GetMessage().find("escapes controlled base directory"), std::string::npos);
}

TEST(DispensingExecutionUseCaseInternalTest, VelocityTracePathPolicyResolvesInsideRuntimeOwnedDirectory) {
    const auto source_dir =
        std::filesystem::temp_directory_path() / "siligen-runtime-execution-velocity-trace-owned";
    std::filesystem::create_directories(source_dir);
    const auto source_path = (source_dir / "sample.dxf").string();

    const auto result =
        Siligen::Application::UseCases::Dispensing::VelocityTracePathPolicy::ResolveOutputPath(
            source_path,
            "");

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();

    const auto resolved_path = std::filesystem::path(result.Value());
    EXPECT_EQ(resolved_path.filename().string(), "sample_velocity_trace.csv");
    EXPECT_EQ(resolved_path.parent_path().filename().string(), "velocity-traces");
    EXPECT_EQ(resolved_path.parent_path().parent_path().filename().string(), "_runtime-execution");
    EXPECT_EQ(resolved_path.parent_path().parent_path().parent_path(), source_dir);
}
