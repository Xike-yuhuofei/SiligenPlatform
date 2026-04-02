#define MODULE_NAME "DispensingExecutionUseCase"

#include "DispensingExecutionUseCase.Internal.h"

#include "runtime_execution/contracts/safety/SafetyOutputGuard.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/interfaces/ILoggingService.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

using namespace Siligen::Shared::Types;

namespace Siligen::Application::UseCases::Dispensing {

using Domain::Dispensing::ValueObjects::JobExecutionMode;
using Domain::Dispensing::ValueObjects::ProcessOutputPolicy;
using Domain::Machine::ValueObjects::MachineMode;
using Domain::Safety::DomainServices::SafetyOutputGuard;

namespace {

Result<void> ValidateRuntimeOverridesExplicit(
    const Domain::Dispensing::ValueObjects::DispensingRuntimeOverrides& overrides) {
    const float32 dispensing_speed = overrides.dispensing_speed_mm_s.value_or(0.0f);
    const float32 dry_run_speed = overrides.dry_run_speed_mm_s.value_or(0.0f);
    const float32 rapid_speed = overrides.rapid_speed_mm_s.value_or(0.0f);
    const float32 acceleration = overrides.acceleration_mm_s2.value_or(0.0f);

    if (overrides.dispensing_speed_mm_s.has_value() && dispensing_speed < 0.0f) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "dispensing_speed_mm_s不能为负数"));
    }
    if (overrides.dry_run_speed_mm_s.has_value() && dry_run_speed < 0.0f) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "dry_run_speed_mm_s不能为负数"));
    }
    if (overrides.rapid_speed_mm_s.has_value() && rapid_speed < 0.0f) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "rapid_speed_mm_s不能为负数"));
    }
    if (dispensing_speed <= 0.0f && dry_run_speed <= 0.0f) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "点胶速度与空走速度至少提供一个"));
    }
    if (overrides.acceleration_mm_s2.has_value() && acceleration < 0.0f) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "acceleration_mm_s2不能为负数"));
    }
    if (overrides.velocity_guard_ratio < 0.0f || overrides.velocity_guard_ratio > 1.0f) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "velocity_guard_ratio超出范围(0-1)"));
    }
    if (overrides.velocity_guard_abs_mm_s < 0.0f || overrides.velocity_guard_min_expected_mm_s < 0.0f) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "velocity_guard_abs_mm_s/velocity_guard_min_expected_mm_s不能为负数"));
    }
    if (overrides.velocity_guard_grace_ms < 0 ||
        overrides.velocity_guard_interval_ms < 0 ||
        overrides.velocity_guard_max_consecutive < 0) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "velocity_guard_grace_ms/velocity_guard_interval_ms/velocity_guard_max_consecutive不能为负数"));
    }
    if (overrides.velocity_guard_enabled) {
        if (overrides.velocity_guard_ratio <= 0.0f && overrides.velocity_guard_abs_mm_s <= 0.0f) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "速度保护阈值无效(比例/绝对阈值需至少一个>0)"));
        }
        if (overrides.velocity_guard_interval_ms == 0 || overrides.velocity_guard_max_consecutive == 0) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "速度保护interval/max_consecutive必须大于0"));
        }
    }
    if (overrides.dry_run) {
        if (overrides.machine_mode.has_value() &&
            overrides.machine_mode.value() != MachineMode::Test) {
            return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "dry_run与machine_mode语义冲突"));
        }
        if (overrides.execution_mode.has_value() &&
            overrides.execution_mode.value() != JobExecutionMode::ValidationDryCycle) {
            return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "dry_run与execution_mode语义冲突"));
        }
        if (overrides.output_policy.has_value() &&
            overrides.output_policy.value() != ProcessOutputPolicy::Inhibited) {
            return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "dry_run与output_policy语义冲突"));
        }
    }

    return Result<void>::Success();
}
}  // namespace

MachineMode DispensingExecutionRequest::ResolveMachineMode() const noexcept {
    if (machine_mode.has_value()) {
        return machine_mode.value();
    }
    return dry_run ? MachineMode::Test : MachineMode::Production;
}

JobExecutionMode DispensingExecutionRequest::ResolveExecutionMode() const noexcept {
    if (execution_mode.has_value()) {
        return execution_mode.value();
    }
    return dry_run ? JobExecutionMode::ValidationDryCycle : JobExecutionMode::Production;
}

ProcessOutputPolicy DispensingExecutionRequest::ResolveOutputPolicy() const noexcept {
    if (output_policy.has_value()) {
        return output_policy.value();
    }
    return dry_run ? ProcessOutputPolicy::Inhibited : ProcessOutputPolicy::Enabled;
}

Result<void> DispensingExecutionRequest::Validate() const noexcept {
    auto package_validation = execution_package.Validate();
    if (package_validation.IsError()) {
        return package_validation;
    }
    if (max_jerk < 0.0f) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "max_jerk不能为负数"));
    }
    if (arc_tolerance_mm < 0.0f) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "arc_tolerance_mm不能为负数"));
    }

    Domain::Dispensing::ValueObjects::DispensingRuntimeOverrides overrides;
    overrides.dry_run = dry_run;
    overrides.machine_mode = machine_mode;
    overrides.execution_mode = execution_mode;
    overrides.output_policy = output_policy;
    overrides.dispensing_speed_mm_s = dispensing_speed_mm_s;
    overrides.dry_run_speed_mm_s = dry_run_speed_mm_s;
    overrides.rapid_speed_mm_s = rapid_speed_mm_s;
    overrides.acceleration_mm_s2 = acceleration_mm_s2;
    overrides.velocity_guard_enabled = velocity_guard_enabled;
    overrides.velocity_guard_ratio = velocity_guard_ratio;
    overrides.velocity_guard_abs_mm_s = velocity_guard_abs_mm_s;
    overrides.velocity_guard_min_expected_mm_s = velocity_guard_min_expected_mm_s;
    overrides.velocity_guard_grace_ms = velocity_guard_grace_ms;
    overrides.velocity_guard_interval_ms = velocity_guard_interval_ms;
    overrides.velocity_guard_max_consecutive = velocity_guard_max_consecutive;
    overrides.velocity_guard_stop_on_violation = velocity_guard_stop_on_violation;

    auto override_validation = ValidateRuntimeOverridesExplicit(overrides);
    if (override_validation.IsError()) {
        return Result<void>::Failure(override_validation.GetError());
    }

    auto guard_validation = SafetyOutputGuard::Evaluate(
        ResolveMachineMode(),
        ResolveExecutionMode(),
        ResolveOutputPolicy());
    if (guard_validation.IsError()) {
        return Result<void>::Failure(guard_validation.GetError());
    }

    return Result<void>::Success();
}

DispensingExecutionUseCase::DispensingExecutionUseCase(
    std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port,
    std::shared_ptr<Domain::Motion::Ports::IInterpolationPort> interpolation_port,
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
    std::shared_ptr<RuntimeDispensingProcessPort> process_port,
    std::shared_ptr<RuntimeEventPublisherPort> event_port,
    std::shared_ptr<RuntimeTaskSchedulerPort> task_scheduler_port,
    std::shared_ptr<RuntimeHomingPort> homing_port,
    std::shared_ptr<RuntimeInterlockSignalPort> interlock_signal_port)
    : impl_(std::make_unique<Impl>(
          std::move(valve_port),
          std::move(interpolation_port),
          std::move(motion_state_port),
          std::move(connection_port),
          std::move(config_port),
          std::move(process_port),
          std::move(event_port),
          std::move(task_scheduler_port),
          std::move(homing_port),
          std::move(interlock_signal_port))) {}

DispensingExecutionUseCase::~DispensingExecutionUseCase() = default;

Result<DispensingExecutionResult> DispensingExecutionUseCase::Execute(const DispensingExecutionRequest& request) {
    return impl_->Execute(request);
}

Result<JobID> DispensingExecutionUseCase::StartJob(const RuntimeStartJobRequest& request) {
    return impl_->StartJob(request);
}

Result<RuntimeJobStatusResponse> DispensingExecutionUseCase::GetJobStatus(const JobID& job_id) const {
    return impl_->GetJobStatus(job_id);
}

JobID DispensingExecutionUseCase::GetActiveJobId() const {
    return impl_->GetActiveJobId();
}

Result<void> DispensingExecutionUseCase::PauseJob(const JobID& job_id) {
    return impl_->PauseJob(job_id);
}

Result<void> DispensingExecutionUseCase::ResumeJob(const JobID& job_id) {
    return impl_->ResumeJob(job_id);
}

Result<void> DispensingExecutionUseCase::StopJob(const JobID& job_id) {
    return impl_->StopJob(job_id);
}

JobID DispensingExecutionUseCase::Impl::GetActiveJobId() const {
    return session_store_.GetActiveJobId();
}

#ifdef SILIGEN_TEST_HOOKS
void DispensingExecutionUseCase::SeedJobStateForTesting(
    const RuntimeJobStatusResponse& status,
    bool pause_requested) {
    impl_->SeedJobStateForTesting(status, pause_requested);
}

void DispensingExecutionUseCase::SetActiveJobForTesting(const JobID& job_id) {
    impl_->SetActiveJobForTesting(job_id);
}
#endif

DispensingExecutionUseCase::Impl::Impl(
    std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port,
    std::shared_ptr<Domain::Motion::Ports::IInterpolationPort> interpolation_port,
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
    std::shared_ptr<RuntimeDispensingProcessPort> process_port,
    std::shared_ptr<RuntimeEventPublisherPort> event_port,
    std::shared_ptr<RuntimeTaskSchedulerPort> task_scheduler_port,
    std::shared_ptr<RuntimeHomingPort> homing_port,
    std::shared_ptr<RuntimeInterlockSignalPort> interlock_signal_port)
    : valve_port_(std::move(valve_port)),
      interpolation_port_(std::move(interpolation_port)),
      motion_state_port_(std::move(motion_state_port)),
      connection_port_(std::move(connection_port)),
      config_port_(std::move(config_port)),
      process_port_(std::move(process_port)),
      event_port_(std::move(event_port)),
      task_scheduler_port_(std::move(task_scheduler_port)),
      homing_port_(std::move(homing_port)),
      interlock_signal_port_(std::move(interlock_signal_port)) {}

DispensingExecutionUseCase::Impl::~Impl() {
    stop_requested_.store(true);
    const JobID active_job = session_store_.GetActiveJobId();
    if (!active_job.empty()) {
        (void)StopJob(active_job);
    }
    worker_coordinator_.JoinJobWorkerThread();

    std::shared_ptr<TaskExecutionContext> active_context;
    {
        std::lock_guard<std::mutex> lock(session_store_.tasks_mutex_);
        auto active_it = session_store_.tasks_.find(session_store_.active_task_id_);
        if (active_it != session_store_.tasks_.end()) {
            active_context = active_it->second;
        }
    }
    if (active_context) {
        active_context->cancel_requested.store(true);
        active_context->pause_requested.store(false);

        if (task_scheduler_port_) {
            std::string scheduler_task_id;
            {
                std::lock_guard<std::mutex> context_lock(active_context->mutex_);
                scheduler_task_id = active_context->scheduler_task_id;
            }
            if (!scheduler_task_id.empty()) {
                auto scheduler_cancel_result = task_scheduler_port_->CancelTask(scheduler_task_id);
                if (scheduler_cancel_result.IsSuccess()) {
                    ExecutionSessionStore::TryCommitTerminalState(
                        active_context,
                        TaskState::CANCELLED,
                        "failure_stage=destructor_cancel;failure_code=scheduler_cancelled;message=execution_destroyed");
                    worker_coordinator_.ReleaseTaskInflight(active_context);
                }
            }
        }
    }
    if (process_port_) {
        process_port_->StopExecution(&stop_requested_);
    }

    constexpr auto kInflightDrainTimeout = std::chrono::seconds(5);
    constexpr std::uint32_t kInflightDrainMaxRetries = 3;

    std::string inflight_diagnostics;
    bool inflight_drained = WaitForAllInflightTasks(kInflightDrainTimeout, &inflight_diagnostics);
    for (std::uint32_t attempt = 1; !inflight_drained && attempt <= kInflightDrainMaxRetries; ++attempt) {
        SILIGEN_LOG_WARNING(
            "failure_stage=destructor_wait_inflight;failure_code=INFLIGHT_DRAIN_TIMEOUT;attempt=" +
            std::to_string(attempt) + ";message=" + inflight_diagnostics);

        ReconcileStalledInflightTasks();
        inflight_diagnostics.clear();
        inflight_drained = WaitForAllInflightTasks(kInflightDrainTimeout, &inflight_diagnostics);
    }

    while (!inflight_drained) {
        SILIGEN_LOG_ERROR(
            "failure_stage=destructor_wait_inflight;failure_code=INFLIGHT_STILL_PENDING;message=" + inflight_diagnostics);
        ReconcileStalledInflightTasks();
        inflight_diagnostics.clear();
        inflight_drained = WaitForAllInflightTasks(kInflightDrainTimeout, &inflight_diagnostics);
    }

    worker_coordinator_.JoinWorkerThread();
}

Result<DispensingExecutionResult> DispensingExecutionUseCase::Impl::Execute(
    const DispensingExecutionRequest& request) {
    return ExecuteInternal(request, nullptr);
}

Result<DispensingExecutionResult> DispensingExecutionUseCase::Impl::ExecuteInternal(
    const DispensingExecutionRequest& request,
    const std::shared_ptr<TaskExecutionContext>& context) {
    const std::string source_path = request.source_path.empty()
                                        ? (request.execution_package.source_path.empty()
                                               ? std::string("<planned-execution>")
                                               : request.execution_package.source_path)
                                        : request.source_path;
    SILIGEN_LOG_INFO("开始执行点胶计划: " + source_path);

    std::lock_guard<std::mutex> lock(execution_mutex_);
    stop_requested_ = false;
    if (context) {
        context->pause_requested.store(false);
        context->pause_applied.store(false);
    }

    auto validation = request.Validate();
    if (!validation.IsSuccess()) {
        SILIGEN_LOG_ERROR("请求参数验证失败: " + validation.GetError().GetMessage());
        return Result<DispensingExecutionResult>::Failure(validation.GetError());
    }
    if (!process_port_) {
        return Result<DispensingExecutionResult>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "点胶流程服务未初始化", "DispensingExecutionUseCase"));
    }

    auto conn_check = ValidateHardwareConnection(request.dry_run);
    if (!conn_check.IsSuccess()) {
        SILIGEN_LOG_ERROR("硬件连接验证失败: " + conn_check.GetError().GetMessage());
        return Result<DispensingExecutionResult>::Failure(conn_check.GetError());
    }

    auto runtime_result = RefreshRuntimeParameters(request);
    if (runtime_result.IsError()) {
        return Result<DispensingExecutionResult>::Failure(runtime_result.GetError());
    }

    const auto& execution_package = request.execution_package;
    const auto& execution_plan = execution_package.execution_plan;
    if (execution_plan.motion_trajectory.points.size() < 2 && execution_plan.interpolation_points.size() < 2) {
        return Result<DispensingExecutionResult>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "轨迹点数量不足", "DispensingExecutionUseCase"));
    }

    VelocityTraceSettings trace_settings;
    if (request.velocity_trace_enabled) {
        trace_settings.enabled = true;
        trace_settings.interval_ms = request.velocity_trace_interval_ms;
        trace_settings.output_path = request.velocity_trace_path;

        if ((trace_settings.interval_ms <= 0 || trace_settings.output_path.empty()) && config_port_) {
            auto trace_config_result = config_port_->GetVelocityTraceConfig();
            if (trace_config_result.IsSuccess()) {
                const auto& trace_config = trace_config_result.Value();
                if (trace_settings.interval_ms <= 0) {
                    trace_settings.interval_ms = trace_config.interval_ms;
                }
                if (trace_settings.output_path.empty()) {
                    trace_settings.output_path = trace_config.output_path;
                }
            } else {
                SILIGEN_LOG_WARNING("读取速度采样配置失败: " + trace_config_result.GetError().GetMessage());
            }
        }
        if (trace_settings.interval_ms <= 0) {
            trace_settings.interval_ms = 50;
        }
        if (trace_settings.output_path.empty()) {
            trace_settings.output_path = "logs/velocity_trace";
        }
        trace_settings.output_path = ResolveVelocityTracePath(execution_package.source_path, trace_settings.output_path);

        SILIGEN_LOG_INFO_FMT_HELPER(
            "速度采样启用: interval_ms=%d, output_path=%s",
            trace_settings.interval_ms,
            trace_settings.output_path.c_str());
    }

    DispensingExecutionResult result;
    result.total_segments = static_cast<uint32>(execution_plan.interpolation_segments.size());
    if (result.total_segments == 0) {
        result.total_segments = static_cast<uint32>(execution_plan.interpolation_points.size() > 1
                                                        ? execution_plan.interpolation_points.size() - 1
                                                        : 0);
    }
    if (result.total_segments == 0 && execution_plan.motion_trajectory.points.size() > 1) {
        result.total_segments = static_cast<uint32>(execution_plan.motion_trajectory.points.size() - 1);
    }
    result.total_distance = execution_package.total_length_mm > 0.0f
                                ? execution_package.total_length_mm
                                : execution_plan.motion_trajectory.total_length;
    if (context) {
        context->total_segments.store(result.total_segments);
        context->executed_segments.store(0);
        context->reported_progress_percent.store(0);
        context->reported_executed_segments.store(0);
        const auto estimated_execution_ms = static_cast<uint32>(
            std::max(
                0.0f,
                (execution_package.estimated_time_s > 0.0f
                     ? execution_package.estimated_time_s
                     : execution_plan.motion_trajectory.total_time) * 1000.0f));
        context->estimated_execution_ms.store(estimated_execution_ms);
        if (!context->terminal_committed.load()) {
            context->state.store(TaskState::RUNNING);
        }
    }

    const auto& guard_decision = resolved_execution_.guard_decision;
    const bool dispense_enabled = guard_decision.AllowsDispensingOutputs();
    SILIGEN_LOG_INFO_FMT_HELPER(
        "执行模式解析: machine_mode=%s, execution_mode=%s, output_policy=%s, "
        "allow_motion=%d, allow_valve=%d, allow_supply=%d, allow_cmp=%d",
        Domain::Machine::ValueObjects::ToString(resolved_execution_.machine_mode),
        Domain::Dispensing::ValueObjects::ToString(resolved_execution_.execution_mode),
        Domain::Dispensing::ValueObjects::ToString(resolved_execution_.output_policy),
        guard_decision.allow_motion ? 1 : 0,
        guard_decision.allow_valve ? 1 : 0,
        guard_decision.allow_supply ? 1 : 0,
        guard_decision.allow_cmp ? 1 : 0);
    Domain::Dispensing::ValueObjects::DispensingExecutionOptions options;
    options.dispense_enabled = dispense_enabled;
    options.use_hardware_trigger = request.use_hardware_trigger;
    options.machine_mode = resolved_execution_.machine_mode;
    options.execution_mode = resolved_execution_.execution_mode;
    options.output_policy = resolved_execution_.output_policy;
    options.guard_decision = guard_decision;

    auto observer = CreateExecutionObserver(
        motion_state_port_,
        trace_settings.output_path,
        trace_settings.interval_ms,
        trace_settings.enabled,
        dispense_enabled,
        context);

    auto start_time = std::chrono::steady_clock::now();
    auto exec_result = process_port_->ExecuteProcess(execution_plan,
                                                     runtime_params_,
                                                     options,
                                                     &stop_requested_,
                                                     context ? &context->pause_requested : nullptr,
                                                     context ? &context->pause_applied : nullptr,
                                                     observer.get());
    auto end_time = std::chrono::steady_clock::now();

    result.execution_time_seconds = std::chrono::duration<float32>(end_time - start_time).count();

    if (!exec_result.IsSuccess()) {
        result.success = false;
        result.message = "点胶执行失败: " + exec_result.GetError().GetMessage();
        result.error_details = exec_result.GetError().GetMessage();
        result.quality_metrics.run_count = 1;
        result.quality_metrics.interruption_count = 1;
        result.quality_metrics_available = true;
        return Result<DispensingExecutionResult>::Failure(exec_result.GetError());
    }

    result.executed_segments = exec_result.Value().executed_segments;
    result.total_distance = exec_result.Value().total_distance;

    result.success = true;
    result.executed_segments = result.total_segments;
    result.failed_segments = 0;
    result.message = "点胶执行成功";
    result.quality_metrics.run_count = 1;
    result.quality_metrics.interruption_count = 0;
    result.quality_metrics_available = true;

    SILIGEN_LOG_INFO_FMT_HELPER(
        "点胶执行完成: segments=%u, distance=%.3fmm, time=%.2fs",
        result.executed_segments,
        result.total_distance,
        result.execution_time_seconds);

    return Result<DispensingExecutionResult>::Success(result);
}

}  // namespace Siligen::Application::UseCases::Dispensing


