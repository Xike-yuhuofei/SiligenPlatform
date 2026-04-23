#define MODULE_NAME "DispensingExecutionUseCase"

#include "DispensingExecutionUseCase.Internal.h"

#include "domain/safety/domain-services/SafetyOutputGuard.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/interfaces/ILoggingService.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <thread>
#include <utility>

using namespace Siligen::Shared::Types;

namespace Siligen::Application::UseCases::Dispensing {

using Domain::Dispensing::ValueObjects::JobExecutionMode;
using Domain::Dispensing::ValueObjects::ProcessOutputPolicy;
using Domain::Machine::ValueObjects::MachineMode;
using Domain::Safety::DomainServices::SafetyOutputGuard;

namespace {

bool PrepareVelocityTraceFile(const std::string& output_path,
                              std::ofstream& file,
                              std::string& error);

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

class VelocityTraceObserver : public Domain::Dispensing::Ports::IDispensingExecutionObserver {
   public:
    VelocityTraceObserver(const std::shared_ptr<Domain::Motion::Ports::IMotionStatePort>& motion_state_port,
                          const std::string& output_path,
                          int32 interval_ms,
                          bool dispense_enabled)
        : motion_state_port_(motion_state_port),
          output_path_(output_path),
          interval_ms_(interval_ms),
          dispense_flag_(dispense_enabled ? 1 : 0) {}

    void OnMotionStart() noexcept override {
        if (started_) {
            return;
        }
        started_ = true;
        if (output_path_.empty()) {
            SILIGEN_LOG_WARNING("速度采样启用但输出路径为空，跳过采样");
            return;
        }

        const int trace_interval_ms = std::max(1, interval_ms_);
        const std::string trace_path = output_path_;

        SILIGEN_LOG_INFO_FMT_HELPER(
            "启动速度采样线程: interval_ms=%d, path=%s",
            trace_interval_ms,
            trace_path.c_str());

        stop_flag_.store(false);
        worker_ = std::thread([this, trace_interval_ms, trace_path]() {
            std::ofstream file;
            std::string error;
            if (!PrepareVelocityTraceFile(trace_path, file, error)) {
                SILIGEN_LOG_WARNING("速度采样文件初始化失败: " + error);
                return;
            }

            const auto interval = std::chrono::milliseconds(trace_interval_ms);
            auto start_time = std::chrono::steady_clock::now();
            auto next_tick = start_time;
            size_t count = 0;
            bool warned_position = false;
            bool warned_velocity = false;
            bool warned_motion_port = false;

            while (!stop_flag_.load()) {
                auto now = std::chrono::steady_clock::now();
                if (now < next_tick) {
                    std::this_thread::sleep_for(next_tick - now);
                    continue;
                }

                const double ts = std::chrono::duration<double>(now - start_time).count();
                float32 x = 0.0f;
                float32 y = 0.0f;

                if (motion_state_port_) {
                    auto pos_result = motion_state_port_->GetCurrentPosition();
                    if (pos_result.IsSuccess()) {
                        x = pos_result.Value().x;
                        y = pos_result.Value().y;
                    } else if (!warned_position) {
                        warned_position = true;
                        SILIGEN_LOG_WARNING("速度采样读取位置失败: " +
                                            pos_result.GetError().GetMessage());
                    }
                } else if (!warned_motion_port) {
                    warned_motion_port = true;
                    SILIGEN_LOG_WARNING("速度采样未注入MotionState端口，位置/速度输出为0");
                }

                float32 velocity = 0.0f;

                if (motion_state_port_) {
                    auto vx_result = motion_state_port_->GetAxisVelocity(LogicalAxisId::X);
                    auto vy_result = motion_state_port_->GetAxisVelocity(LogicalAxisId::Y);
                    if (vx_result.IsSuccess() && vy_result.IsSuccess()) {
                        const float32 vx = vx_result.Value();
                        const float32 vy = vy_result.Value();
                        velocity = std::sqrt(vx * vx + vy * vy);
                    } else if (vx_result.IsSuccess() && !vy_result.IsSuccess()) {
                        velocity = std::fabs(vx_result.Value());
                    } else if (vy_result.IsSuccess() && !vx_result.IsSuccess()) {
                        velocity = std::fabs(vy_result.Value());
                    } else if (!warned_velocity) {
                        warned_velocity = true;
                        SILIGEN_LOG_WARNING("速度采样读取轴速度失败，将输出0");
                    }
                }

                file << ts << ',' << x << ',' << y << ',' << velocity << ',' << dispense_flag_ << '\n';
                ++count;
                next_tick += interval;
            }

            file.close();
            SILIGEN_LOG_INFO_FMT_HELPER(
                "速度采样完成: path=%s, points=%zu",
                trace_path.c_str(),
                count);
        });
    }

    void OnMotionStop() noexcept override {
        stop_flag_.store(true);
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    ~VelocityTraceObserver() override {
        OnMotionStop();
    }

   private:
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port_;
    std::string output_path_;
    int32 interval_ms_ = 0;
    int dispense_flag_ = 0;
    std::atomic<bool> stop_flag_{false};
    std::thread worker_;
    bool started_ = false;
};

class TaskTrackingObserver final : public Domain::Dispensing::Ports::IDispensingExecutionObserver {
   public:
    TaskTrackingObserver(
        Domain::Dispensing::Ports::IDispensingExecutionObserver* inner,
        const std::shared_ptr<TaskExecutionContext>& context)
        : inner_(inner), context_(context) {}

    void OnMotionStart() noexcept override {
        if (inner_) {
            inner_->OnMotionStart();
        }
        if (context_ && !context_->terminal_committed.load()) {
            context_->state.store(TaskState::RUNNING);
        }
    }

    void OnMotionStop() noexcept override {
        if (inner_) {
            inner_->OnMotionStop();
        }
    }

    void OnProgress(std::uint32_t executed_segments, std::uint32_t total_segments) noexcept override {
        if (inner_) {
            inner_->OnProgress(executed_segments, total_segments);
        }
        if (context_ && !context_->terminal_committed.load()) {
            context_->executed_segments.store(executed_segments);
            context_->total_segments.store(total_segments);
            context_->reported_executed_segments.store(executed_segments);
            const auto progress_percent =
                total_segments > 0 ? static_cast<uint32>((static_cast<std::uint64_t>(executed_segments) * 100ULL) / total_segments) : 0U;
            context_->reported_progress_percent.store(progress_percent);
        }
    }

    void OnPauseStateChanged(bool paused) noexcept override {
        if (inner_) {
            inner_->OnPauseStateChanged(paused);
        }
        if (context_ && !context_->terminal_committed.load()) {
            context_->state.store(paused ? TaskState::PAUSED : TaskState::RUNNING);
        }
    }

   private:
    Domain::Dispensing::Ports::IDispensingExecutionObserver* inner_ = nullptr;
    std::shared_ptr<TaskExecutionContext> context_;
};

std::string ResolveVelocityTracePath(const std::string& dxf_path, const std::string& output_path) {
    std::string resolved = output_path.empty() ? std::string("logs/velocity_trace") : output_path;
    std::filesystem::path path(resolved);
    if (path.extension().empty()) {
        std::filesystem::path dxf(dxf_path);
        std::string stem = dxf.stem().string();
        if (stem.empty()) {
            stem = "dispensing_execution";
        }
        path /= (stem + "_velocity_trace.csv");
    }
    return path.string();
}

bool PrepareVelocityTraceFile(const std::string& output_path,
                              std::ofstream& file,
                              std::string& error) {
    std::filesystem::path path(output_path);
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    file.open(output_path, std::ios::binary);
    if (!file) {
        error = "无法写入速度采样文件: " + output_path;
        return false;
    }

    file.setf(std::ios::fixed);
    file << std::setprecision(6);
    file << "timestamp_s,x_mm,y_mm,velocity_mm_s,dispense_on\n";
    return true;
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
    if (execution_package.execution_plan.production_trigger_mode ==
        Domain::Dispensing::ValueObjects::ProductionTriggerMode::PROFILE_COMPARE) {
        if (!profile_compare_schedule) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "PROFILE_COMPARE execution request 缺少 profile compare schedule"));
        }
        if (!expected_trace || expected_trace->Empty()) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "PROFILE_COMPARE execution request 缺少 expected trace"));
        }
        auto schedule_validation =
            Siligen::RuntimeExecution::Contracts::Dispensing::ValidateProfileCompareExecutionSchedule(
                execution_package.execution_plan,
                *profile_compare_schedule);
        if (schedule_validation.IsError()) {
            return Result<void>::Failure(schedule_validation.GetError());
        }
        if (expected_trace->items.size() != profile_compare_schedule->expected_trigger_count) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_PARAMETER,
                "PROFILE_COMPARE execution request expected trace 数量与 schedule 不一致"));
        }
    } else if (profile_compare_schedule || expected_trace) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "非 PROFILE_COMPARE execution request 不允许携带 profile compare traceability"));
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
    std::shared_ptr<RuntimeInterlockSignalPort> interlock_signal_port,
    std::shared_ptr<Siligen::Application::Services::Motion::Execution::MotionReadinessService>
        readiness_service)
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
          std::move(interlock_signal_port),
          std::move(readiness_service))) {}

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

Result<RuntimeJobTraceabilityResponse> DispensingExecutionUseCase::GetJobTraceability(const JobID& job_id) const {
    return impl_->GetJobTraceability(job_id);
}

Result<ExecutionTransitionSnapshot> DispensingExecutionUseCase::RequestJobTransition(
    const JobID& job_id,
    ExecutionTransitionState requested_transition_state) {
    return impl_->RequestJobTransition(job_id, requested_transition_state);
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

Result<void> DispensingExecutionUseCase::ContinueJob(const JobID& job_id) {
    return impl_->ContinueJob(job_id);
}

Result<void> DispensingExecutionUseCase::StopJob(const JobID& job_id) {
    return impl_->StopJob(job_id);
}

JobID DispensingExecutionUseCase::Impl::GetActiveJobId() const {
    std::lock_guard<std::mutex> lock(jobs_mutex_);
    return active_job_id_;
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
    std::shared_ptr<RuntimeInterlockSignalPort> interlock_signal_port,
    std::shared_ptr<Siligen::Application::Services::Motion::Execution::MotionReadinessService>
        readiness_service)
    : valve_port_(std::move(valve_port)),
      interpolation_port_(std::move(interpolation_port)),
      motion_state_port_(std::move(motion_state_port)),
      connection_port_(std::move(connection_port)),
      config_port_(std::move(config_port)),
      process_port_(std::move(process_port)),
      event_port_(std::move(event_port)),
      task_scheduler_port_(std::move(task_scheduler_port)),
      homing_port_(std::move(homing_port)),
      interlock_signal_port_(std::move(interlock_signal_port)),
      readiness_service_(std::move(readiness_service)) {}

DispensingExecutionUseCase::Impl::~Impl() {
    stop_requested_.store(true);
    JobID active_job;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        active_job = active_job_id_;
    }
    if (!active_job.empty()) {
        (void)StopJob(active_job);
    }

    std::shared_ptr<TaskExecutionContext> active_context;
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        auto active_it = tasks_.find(active_task_id_);
        if (active_it != tasks_.end()) {
            active_context = active_it->second;
        }
    }
    if (active_context) {
        active_context->cancel_requested.store(true);
        active_context->pause_requested.store(false);
    }
    if (process_port_) {
        process_port_->StopExecution(&stop_requested_);
    }
    JoinJobWorkerThread();
    JoinWorkerThread();

    if (active_context && task_scheduler_port_) {
        std::string scheduler_task_id;
        {
            std::lock_guard<std::mutex> context_lock(active_context->mutex_);
            scheduler_task_id = active_context->scheduler_task_id;
        }
        if (!scheduler_task_id.empty()) {
            auto scheduler_cancel_result = task_scheduler_port_->CancelTask(scheduler_task_id);
            if (scheduler_cancel_result.IsSuccess()) {
                TryCommitTerminalState(
                    active_context,
                    TaskState::CANCELLED,
                    "failure_stage=destructor_cancel;failure_code=scheduler_cancelled;message=execution_destroyed");
                ReleaseTaskInflight(active_context);
            }
        }
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

    if (!inflight_drained) {
        SILIGEN_LOG_ERROR(
            "failure_stage=destructor_wait_inflight;failure_code=INFLIGHT_STILL_PENDING;message=" + inflight_diagnostics);
    }
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
    if (!execution_plan.HasFormalTrajectory() && !execution_plan.RequiresStationaryPointShot()) {
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
    if (result.total_segments == 0 && execution_plan.RequiresStationaryPointShot()) {
        result.total_segments = 1U;
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
                ResolveExecutionNominalTimeS(execution_package) * 1000.0f));
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
    options.use_hardware_trigger =
        request.execution_package.execution_plan.production_trigger_mode ==
        Domain::Dispensing::ValueObjects::ProductionTriggerMode::PROFILE_COMPARE;
    options.machine_mode = resolved_execution_.machine_mode;
    options.execution_mode = resolved_execution_.execution_mode;
    options.output_policy = resolved_execution_.output_policy;
    options.guard_decision = guard_decision;
    options.profile_compare_schedule = request.profile_compare_schedule;
    options.expected_trace = request.expected_trace;

    std::unique_ptr<VelocityTraceObserver> trace_observer;
    if (trace_settings.enabled) {
        trace_observer = std::make_unique<VelocityTraceObserver>(
            motion_state_port_,
            trace_settings.output_path,
            trace_settings.interval_ms,
            dispense_enabled);
    }
    TaskTrackingObserver task_observer(trace_observer ? trace_observer.get() : nullptr, context);

    auto start_time = std::chrono::steady_clock::now();
    auto exec_result = process_port_->ExecuteProcess(execution_package,
                                                     runtime_params_,
                                                     options,
                                                     &stop_requested_,
                                                     context ? &context->pause_requested : nullptr,
                                                     context ? &context->pause_applied : nullptr,
                                                     &task_observer);
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
    result.actual_trace = exec_result.Value().actual_trace;
    result.traceability_mismatches = exec_result.Value().traceability_mismatches;
    result.traceability_verdict = exec_result.Value().traceability_verdict;
    result.traceability_verdict_reason = exec_result.Value().traceability_verdict_reason;
    result.strict_one_to_one_proven = exec_result.Value().strict_one_to_one_proven;

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


