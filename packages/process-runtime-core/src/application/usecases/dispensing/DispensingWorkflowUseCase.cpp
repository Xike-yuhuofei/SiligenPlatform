#define MODULE_NAME "DispensingWorkflowUseCase"

#include "DispensingWorkflowUseCase.h"

#include "domain/safety/domain-services/InterlockPolicy.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/types/Error.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>

namespace Siligen::Application::UseCases::Dispensing {

using Domain::Machine::Ports::IHardwareConnectionPort;
using Domain::Motion::Ports::IMotionStatePort;
using Domain::Motion::Ports::MotionState;
using Domain::Motion::Ports::MotionStatus;
using Domain::Safety::DomainServices::InterlockPolicy;
using Domain::Safety::Ports::IInterlockSignalPort;
using Domain::Safety::ValueObjects::InterlockCause;
using Domain::Safety::ValueObjects::InterlockPolicyConfig;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::LogicalAxisId;

namespace {

constexpr auto kJobPollInterval = std::chrono::milliseconds(100);

std::string ToIso8601UtcNow() {
    const auto now = std::chrono::system_clock::now();
    const auto time_value = std::chrono::system_clock::to_time_t(now);
    std::tm tm_value{};
#ifdef _WIN32
    gmtime_s(&tm_value, &time_value);
#else
    gmtime_r(&time_value, &tm_value);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_value, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string HexEncodeUint64(std::uint64_t value) {
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << value;
    return oss.str();
}

std::uint64_t Fnv1a64(const std::string& text) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : text) {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ULL;
    }
    return hash;
}

bool IsTerminal(DispensingWorkflowUseCase::WorkflowJobState state) {
    return state == DispensingWorkflowUseCase::WorkflowJobState::COMPLETED ||
           state == DispensingWorkflowUseCase::WorkflowJobState::FAILED ||
           state == DispensingWorkflowUseCase::WorkflowJobState::CANCELLED;
}

}  // namespace

DispensingWorkflowUseCase::DispensingWorkflowUseCase(
    std::shared_ptr<UploadFileUseCase> upload_use_case,
    std::shared_ptr<PlanningUseCase> planning_use_case,
    std::shared_ptr<DispensingExecutionUseCase> execution_use_case,
    std::shared_ptr<IHardwareConnectionPort> connection_port,
    std::shared_ptr<IMotionStatePort> motion_state_port,
    std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port,
    std::shared_ptr<IInterlockSignalPort> interlock_signal_port)
    : upload_use_case_(std::move(upload_use_case)),
      planning_use_case_(std::move(planning_use_case)),
      execution_use_case_(std::move(execution_use_case)),
      connection_port_(std::move(connection_port)),
      motion_state_port_(std::move(motion_state_port)),
      homing_port_(std::move(homing_port)),
      interlock_signal_port_(std::move(interlock_signal_port)) {
    if (!upload_use_case_ || !planning_use_case_ || !execution_use_case_) {
        throw std::invalid_argument("DispensingWorkflowUseCase: dependencies cannot be null");
    }
}

Result<CreateArtifactResponse> DispensingWorkflowUseCase::CreateArtifact(const UploadRequest& request) {
    auto upload_result = upload_use_case_->Execute(request);
    if (upload_result.IsError()) {
        return Result<CreateArtifactResponse>::Failure(upload_result.GetError());
    }

    const auto& upload = upload_result.Value();
    CreateArtifactResponse response;
    response.success = true;
    response.artifact_id = GenerateId("artifact");
    response.filepath = upload.filepath;
    response.original_name = upload.original_name;
    response.generated_filename = upload.generated_filename;
    response.size = upload.size;
    response.timestamp = upload.timestamp;

    ArtifactRecord record;
    record.response = response;
    record.upload_response = upload;

    {
        std::lock_guard<std::mutex> lock(artifacts_mutex_);
        artifacts_[response.artifact_id] = record;
    }

    return Result<CreateArtifactResponse>::Success(std::move(response));
}

Result<PreparePlanResponse> DispensingWorkflowUseCase::PreparePlan(const PreparePlanRequest& request) {
    ArtifactRecord artifact;
    {
        std::lock_guard<std::mutex> lock(artifacts_mutex_);
        auto it = artifacts_.find(request.artifact_id);
        if (it == artifacts_.end()) {
            return Result<PreparePlanResponse>::Failure(
                Error(ErrorCode::NOT_FOUND, "artifact not found", "DispensingWorkflowUseCase"));
        }
        artifact = it->second;
    }

    auto execution_request = request.execution_request;
    execution_request.dxf_filepath = artifact.upload_response.filepath;
    auto validation = execution_request.Validate();
    if (validation.IsError()) {
        return Result<PreparePlanResponse>::Failure(validation.GetError());
    }

    auto planning_request = BuildPlanningRequest(artifact.upload_response.filepath, execution_request);
    auto planning_result = planning_use_case_->Execute(planning_request);
    if (planning_result.IsError()) {
        return Result<PreparePlanResponse>::Failure(planning_result.GetError());
    }

    const auto& planning = planning_result.Value();
    PreparePlanResponse response;
    response.success = true;
    response.artifact_id = request.artifact_id;
    response.plan_id = GenerateId("plan");
    response.plan_fingerprint = BuildPlanFingerprint(request.artifact_id, planning, execution_request);
    response.filepath = artifact.upload_response.filepath;
    response.segment_count = static_cast<std::uint32_t>(std::max(0, planning.segment_count));
    response.point_count = static_cast<std::uint32_t>(planning.trajectory_points.size());
    response.total_length_mm = planning.total_length;
    response.estimated_time_s = planning.estimated_time;
    response.generated_at = ToIso8601UtcNow();

    PlanRecord record;
    record.response = response;
    record.execution_request = execution_request;

    {
        std::lock_guard<std::mutex> lock(plans_mutex_);
        plans_[response.plan_id] = record;
    }

    return Result<PreparePlanResponse>::Success(std::move(response));
}

Result<JobID> DispensingWorkflowUseCase::StartJob(const StartJobRequest& request) {
    if (request.plan_id.empty()) {
        return Result<JobID>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "plan_id is required", "DispensingWorkflowUseCase"));
    }
    if (request.target_count == 0) {
        return Result<JobID>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "target_count must be greater than 0", "DispensingWorkflowUseCase"));
    }

    PlanRecord plan_record;
    {
        std::lock_guard<std::mutex> lock(plans_mutex_);
        auto it = plans_.find(request.plan_id);
        if (it == plans_.end()) {
            return Result<JobID>::Failure(
                Error(ErrorCode::NOT_FOUND, "plan not found", "DispensingWorkflowUseCase"));
        }
        plan_record = it->second;
    }

    auto precondition_result = ValidateExecutionPreconditions();
    if (precondition_result.IsError()) {
        return Result<JobID>::Failure(precondition_result.GetError());
    }

    const auto job_id = GenerateId("job");
    auto context = std::make_shared<JobContext>();
    context->job_id = job_id;
    context->plan_id = request.plan_id;
    context->plan_fingerprint = plan_record.response.plan_fingerprint;
    context->execution_request = plan_record.execution_request;
    context->state.store(WorkflowJobState::PENDING);
    context->target_count.store(request.target_count);
    context->dry_run = plan_record.execution_request.dry_run;
    context->start_time = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        if (!active_job_id_.empty()) {
            auto active_it = jobs_.find(active_job_id_);
            if (active_it != jobs_.end() && !IsTerminal(active_it->second->state.load())) {
                return Result<JobID>::Failure(
                    Error(ErrorCode::INVALID_STATE, "another job is already active", "DispensingWorkflowUseCase"));
            }
        }
        jobs_[job_id] = context;
        active_job_id_ = job_id;
    }

    std::thread([this, context, plan_record]() { RunJob(context, plan_record); }).detach();
    return Result<JobID>::Success(job_id);
}

Result<JobStatusResponse> DispensingWorkflowUseCase::GetJobStatus(const JobID& job_id) const {
    std::shared_ptr<JobContext> context;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) {
            return Result<JobStatusResponse>::Failure(
                Error(ErrorCode::NOT_FOUND, "job not found", "DispensingWorkflowUseCase"));
        }
        context = it->second;
    }
    return Result<JobStatusResponse>::Success(BuildJobStatusResponse(context));
}

Result<void> DispensingWorkflowUseCase::PauseJob(const JobID& job_id) {
    std::shared_ptr<JobContext> context;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) {
            return Result<void>::Failure(
                Error(ErrorCode::NOT_FOUND, "job not found", "DispensingWorkflowUseCase"));
        }
        context = it->second;
    }

    const auto state = context->state.load();
    if (state == WorkflowJobState::PAUSED) {
        return Result<void>::Success();
    }
    if (IsTerminal(state)) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "job already finished", "DispensingWorkflowUseCase"));
    }

    context->pause_requested.store(true);
    std::string active_task_id;
    {
        std::lock_guard<std::mutex> lock(context->mutex_);
        active_task_id = context->active_task_id;
    }
    if (!active_task_id.empty()) {
        auto pause_result = execution_use_case_->PauseTask(active_task_id);
        if (pause_result.IsError()) {
            return pause_result;
        }
    }
    return Result<void>::Success();
}

Result<void> DispensingWorkflowUseCase::ResumeJob(const JobID& job_id) {
    std::shared_ptr<JobContext> context;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) {
            return Result<void>::Failure(
                Error(ErrorCode::NOT_FOUND, "job not found", "DispensingWorkflowUseCase"));
        }
        context = it->second;
    }

    const auto state = context->state.load();
    if (IsTerminal(state)) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "job already finished", "DispensingWorkflowUseCase"));
    }

    auto precondition_result = ValidateExecutionPreconditions();
    if (precondition_result.IsError()) {
        return Result<void>::Failure(precondition_result.GetError());
    }

    context->pause_requested.store(false);
    std::string active_task_id;
    {
        std::lock_guard<std::mutex> lock(context->mutex_);
        active_task_id = context->active_task_id;
    }
    if (!active_task_id.empty()) {
        auto resume_result = execution_use_case_->ResumeTask(active_task_id);
        if (resume_result.IsError()) {
            return resume_result;
        }
    }
    return Result<void>::Success();
}

Result<void> DispensingWorkflowUseCase::StopJob(const JobID& job_id) {
    std::shared_ptr<JobContext> context;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) {
            return Result<void>::Failure(
                Error(ErrorCode::NOT_FOUND, "job not found", "DispensingWorkflowUseCase"));
        }
        context = it->second;
    }

    if (IsTerminal(context->state.load())) {
        return Result<void>::Success();
    }

    context->stop_requested.store(true);
    context->pause_requested.store(false);

    std::string active_task_id;
    {
        std::lock_guard<std::mutex> lock(context->mutex_);
        active_task_id = context->active_task_id;
    }
    if (!active_task_id.empty()) {
        auto cancel_result = execution_use_case_->CancelTask(active_task_id);
        if (cancel_result.IsError()) {
            return cancel_result;
        }
    } else {
        FinalizeJob(context, WorkflowJobState::CANCELLED, "job cancelled");
    }

    return Result<void>::Success();
}

Result<Domain::Safety::ValueObjects::InterlockSignals> DispensingWorkflowUseCase::ReadInterlockSignals() const {
    if (!interlock_signal_port_) {
        return Result<Domain::Safety::ValueObjects::InterlockSignals>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "interlock signal port not available", "DispensingWorkflowUseCase"));
    }
    return interlock_signal_port_->ReadSignals();
}

Result<void> DispensingWorkflowUseCase::ValidateExecutionPreconditions() const {
    if (!connection_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "hardware connection port not available", "DispensingWorkflowUseCase"));
    }
    if (!connection_port_->IsConnected()) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "hardware not connected", "DispensingWorkflowUseCase"));
    }
    if (!motion_state_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "motion state port not available", "DispensingWorkflowUseCase"));
    }

    const LogicalAxisId required_axes[] = {LogicalAxisId::X, LogicalAxisId::Y};
    for (LogicalAxisId axis : required_axes) {
        auto status_result = motion_state_port_->GetAxisStatus(axis);
        if (status_result.IsError()) {
            return Result<void>::Failure(status_result.GetError());
        }
        const MotionStatus& status = status_result.Value();
        if (status.state == MotionState::ESTOP) {
            return Result<void>::Failure(
                Error(ErrorCode::EMERGENCY_STOP_ACTIVATED, "axis in emergency stop state", "DispensingWorkflowUseCase"));
        }
        if (!status.enabled || status.state == MotionState::DISABLED) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "required axis not enabled", "DispensingWorkflowUseCase"));
        }
        bool axis_homed = false;
        if (homing_port_) {
            auto homed_result = homing_port_->IsAxisHomed(axis);
            if (homed_result.IsError()) {
                return Result<void>::Failure(homed_result.GetError());
            }
            axis_homed = homed_result.Value();
        } else {
            axis_homed = (status.state == MotionState::HOMED);
        }
        if (!axis_homed) {
            return Result<void>::Failure(
                Error(ErrorCode::AXIS_NOT_HOMED, "required axis not homed", "DispensingWorkflowUseCase"));
        }
        if (status.has_error || status.servo_alarm || status.following_error) {
            return Result<void>::Failure(
                Error(ErrorCode::HARDWARE_ERROR, "required axis has active error", "DispensingWorkflowUseCase"));
        }
    }

    if (interlock_signal_port_) {
        InterlockPolicyConfig interlock_config;
        auto interlock_result = InterlockPolicy::Evaluate(*interlock_signal_port_, interlock_config);
        if (interlock_result.IsError()) {
            return Result<void>::Failure(interlock_result.GetError());
        }

        const auto& decision = interlock_result.Value();
        if (decision.triggered) {
            const std::string reason = decision.reason == nullptr ? "interlock triggered" : decision.reason;
            switch (decision.cause) {
                case InterlockCause::EMERGENCY_STOP:
                    return Result<void>::Failure(
                        Error(ErrorCode::EMERGENCY_STOP_ACTIVATED, reason, "DispensingWorkflowUseCase"));
                case InterlockCause::SERVO_ALARM:
                    return Result<void>::Failure(
                        Error(ErrorCode::HARDWARE_ERROR, reason, "DispensingWorkflowUseCase"));
                case InterlockCause::SAFETY_DOOR_OPEN:
                case InterlockCause::PRESSURE_ABNORMAL:
                case InterlockCause::TEMPERATURE_ABNORMAL:
                case InterlockCause::VOLTAGE_ABNORMAL:
                    return Result<void>::Failure(
                        Error(ErrorCode::INVALID_STATE, reason, "DispensingWorkflowUseCase"));
                case InterlockCause::NONE:
                default:
                    return Result<void>::Failure(
                        Error(ErrorCode::INVALID_STATE, "interlock triggered", "DispensingWorkflowUseCase"));
            }
        }
    }

    return Result<void>::Success();
}

PlanningRequest DispensingWorkflowUseCase::BuildPlanningRequest(
    const std::string& filepath,
    const DispensingMVPRequest& request) const {
    PlanningRequest planning_request;
    planning_request.dxf_filepath = filepath;
    planning_request.optimize_path = request.optimize_path;
    planning_request.start_x = request.start_x;
    planning_request.start_y = request.start_y;
    planning_request.approximate_splines = request.approximate_splines;
    planning_request.two_opt_iterations = request.two_opt_iterations;
    planning_request.spline_max_step_mm = request.spline_max_step_mm;
    planning_request.spline_max_error_mm = request.spline_max_error_mm;
    planning_request.continuity_tolerance_mm = request.continuity_tolerance_mm;
    planning_request.curve_chain_angle_deg = request.curve_chain_angle_deg;
    planning_request.curve_chain_max_segment_mm = request.curve_chain_max_segment_mm;
    planning_request.use_hardware_trigger = request.use_hardware_trigger;
    planning_request.use_interpolation_planner = request.use_interpolation_planner;
    planning_request.interpolation_algorithm = request.interpolation_algorithm;
    planning_request.trajectory_config.arc_tolerance = request.arc_tolerance_mm > 0.0f
                                                           ? request.arc_tolerance_mm
                                                           : planning_request.trajectory_config.arc_tolerance;
    planning_request.trajectory_config.max_jerk = request.max_jerk > 0.0f
                                                      ? request.max_jerk
                                                      : planning_request.trajectory_config.max_jerk;

    if (request.acceleration_mm_s2.has_value() && request.acceleration_mm_s2.value() > 0.0f) {
        planning_request.trajectory_config.max_acceleration = request.acceleration_mm_s2.value();
    }

    if (request.dry_run && request.dry_run_speed_mm_s.has_value() && request.dry_run_speed_mm_s.value() > 0.0f) {
        planning_request.trajectory_config.max_velocity = request.dry_run_speed_mm_s.value();
    } else if (request.dispensing_speed_mm_s.has_value() && request.dispensing_speed_mm_s.value() > 0.0f) {
        planning_request.trajectory_config.max_velocity = request.dispensing_speed_mm_s.value();
    } else if (request.rapid_speed_mm_s.has_value() && request.rapid_speed_mm_s.value() > 0.0f) {
        planning_request.trajectory_config.max_velocity = request.rapid_speed_mm_s.value();
    }

    return planning_request;
}

JobStatusResponse DispensingWorkflowUseCase::BuildJobStatusResponse(const std::shared_ptr<JobContext>& context) const {
    JobStatusResponse response;
    response.job_id = context->job_id;
    response.plan_id = context->plan_id;
    response.plan_fingerprint = context->plan_fingerprint;
    response.state = JobStateToString(context->state.load());
    response.target_count = context->target_count.load();
    response.completed_count = context->completed_count.load();
    response.current_cycle = context->current_cycle.load();
    response.current_segment = context->current_segment.load();
    response.total_segments = context->total_segments.load();
    response.cycle_progress_percent = context->cycle_progress_percent.load();
    response.dry_run = context->dry_run;

    if (response.target_count > 0) {
        const std::uint64_t numerator =
            static_cast<std::uint64_t>(response.completed_count) * 100ULL + response.cycle_progress_percent;
        response.overall_progress_percent = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(100ULL, numerator / response.target_count));
    }
    if (response.state == "completed") {
        response.overall_progress_percent = 100;
        response.cycle_progress_percent = 100;
    }

    const auto end_point = IsTerminal(context->state.load()) ? context->end_time : std::chrono::steady_clock::now();
    response.elapsed_seconds =
        std::chrono::duration<float32>(end_point - context->start_time).count();

    {
        std::lock_guard<std::mutex> lock(context->mutex_);
        response.error_message = context->error_message;
        response.active_task_id = context->active_task_id;
    }
    return response;
}

std::string DispensingWorkflowUseCase::GenerateId(const char* prefix) {
    const auto seq = id_sequence_.fetch_add(1, std::memory_order_relaxed) + 1;
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return std::string(prefix) + "-" + std::to_string(millis) + "-" + std::to_string(seq);
}

std::string DispensingWorkflowUseCase::BuildPlanFingerprint(
    const ArtifactID& artifact_id,
    const PlanningResponse& planning,
    const DispensingMVPRequest& execution_request) const {
    std::ostringstream oss;
    oss << artifact_id << '|'
        << execution_request.dry_run << '|'
        << execution_request.optimize_path << '|'
        << execution_request.start_x << '|'
        << execution_request.start_y << '|'
        << execution_request.approximate_splines << '|'
        << execution_request.two_opt_iterations << '|'
        << execution_request.spline_max_step_mm << '|'
        << execution_request.spline_max_error_mm << '|'
        << execution_request.arc_tolerance_mm << '|'
        << execution_request.continuity_tolerance_mm << '|'
        << execution_request.curve_chain_angle_deg << '|'
        << execution_request.curve_chain_max_segment_mm << '|'
        << execution_request.use_hardware_trigger << '|'
        << execution_request.use_interpolation_planner << '|'
        << static_cast<int>(execution_request.interpolation_algorithm) << '|'
        << execution_request.dispensing_speed_mm_s.value_or(0.0f) << '|'
        << execution_request.dry_run_speed_mm_s.value_or(0.0f) << '|'
        << execution_request.rapid_speed_mm_s.value_or(0.0f) << '|'
        << execution_request.acceleration_mm_s2.value_or(0.0f) << '|'
        << planning.segment_count << '|'
        << planning.trajectory_points.size() << '|'
        << planning.total_length << '|'
        << planning.estimated_time;
    return HexEncodeUint64(Fnv1a64(oss.str()));
}

std::string DispensingWorkflowUseCase::JobStateToString(WorkflowJobState state) const {
    switch (state) {
        case WorkflowJobState::PENDING:
            return "pending";
        case WorkflowJobState::RUNNING:
            return "running";
        case WorkflowJobState::PAUSED:
            return "paused";
        case WorkflowJobState::COMPLETED:
            return "completed";
        case WorkflowJobState::FAILED:
            return "failed";
        case WorkflowJobState::CANCELLED:
            return "cancelled";
        default:
            return "unknown";
    }
}

void DispensingWorkflowUseCase::RunJob(
    const std::shared_ptr<JobContext>& context,
    const PlanRecord& plan_record) {
    context->state.store(WorkflowJobState::RUNNING);
    const auto target_count = context->target_count.load();

    for (std::uint32_t cycle = 0; cycle < target_count; ++cycle) {
        context->current_cycle.store(cycle + 1);
        context->current_segment.store(0);
        context->total_segments.store(0);
        context->cycle_progress_percent.store(0);

        while (context->pause_requested.load()) {
            if (context->stop_requested.load()) {
                FinalizeJob(context, WorkflowJobState::CANCELLED, "job cancelled");
                return;
            }
            context->state.store(WorkflowJobState::PAUSED);
            std::this_thread::sleep_for(kJobPollInterval);
        }

        auto precondition_result = ValidateExecutionPreconditions();
        if (precondition_result.IsError()) {
            FinalizeJob(context, WorkflowJobState::FAILED, precondition_result.GetError().GetMessage());
            return;
        }

        auto task_result = execution_use_case_->ExecuteAsync(plan_record.execution_request);
        if (task_result.IsError()) {
            FinalizeJob(context, WorkflowJobState::FAILED, task_result.GetError().GetMessage());
            return;
        }

        const auto task_id = task_result.Value();
        {
            std::lock_guard<std::mutex> lock(context->mutex_);
            context->active_task_id = task_id;
        }

        bool pause_forwarded = false;
        while (true) {
            if (context->stop_requested.load()) {
                auto cancel_result = execution_use_case_->CancelTask(task_id);
                if (cancel_result.IsError()) {
                    FinalizeJob(context, WorkflowJobState::FAILED, cancel_result.GetError().GetMessage());
                    return;
                }
            }

            auto task_status_result = execution_use_case_->GetTaskStatus(task_id);
            if (task_status_result.IsError()) {
                FinalizeJob(context, WorkflowJobState::FAILED, task_status_result.GetError().GetMessage());
                return;
            }
            const auto& task_status = task_status_result.Value();
            context->current_segment.store(task_status.executed_segments);
            context->total_segments.store(task_status.total_segments);
            context->cycle_progress_percent.store(task_status.progress_percent);

            if (context->pause_requested.load()) {
                if (!pause_forwarded && task_status.state == "running") {
                    auto pause_result = execution_use_case_->PauseTask(task_id);
                    if (pause_result.IsError()) {
                        FinalizeJob(context, WorkflowJobState::FAILED, pause_result.GetError().GetMessage());
                        return;
                    }
                    pause_forwarded = true;
                }
            } else if (pause_forwarded && task_status.state == "paused") {
                auto resume_result = execution_use_case_->ResumeTask(task_id);
                if (resume_result.IsError()) {
                    FinalizeJob(context, WorkflowJobState::FAILED, resume_result.GetError().GetMessage());
                    return;
                }
                pause_forwarded = false;
            }

            if (task_status.state == "paused") {
                context->state.store(WorkflowJobState::PAUSED);
            } else if (task_status.state == "running" || task_status.state == "pending") {
                context->state.store(WorkflowJobState::RUNNING);
            } else if (task_status.state == "completed") {
                context->completed_count.store(cycle + 1);
                context->cycle_progress_percent.store(100);
                break;
            } else if (task_status.state == "cancelled") {
                FinalizeJob(context, WorkflowJobState::CANCELLED, task_status.error_message);
                return;
            } else if (task_status.state == "failed") {
                FinalizeJob(context, WorkflowJobState::FAILED, task_status.error_message);
                return;
            }

            std::this_thread::sleep_for(kJobPollInterval);
        }

        {
            std::lock_guard<std::mutex> lock(context->mutex_);
            context->active_task_id.clear();
        }
    }

    FinalizeJob(context, WorkflowJobState::COMPLETED);
}

void DispensingWorkflowUseCase::FinalizeJob(
    const std::shared_ptr<JobContext>& context,
    WorkflowJobState final_state,
    const std::string& error_message) {
    context->state.store(final_state);
    context->end_time = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(context->mutex_);
        context->active_task_id.clear();
        context->error_message = error_message;
    }
    if (final_state == WorkflowJobState::COMPLETED) {
        context->cycle_progress_percent.store(100);
        context->current_cycle.store(context->target_count.load());
        context->completed_count.store(context->target_count.load());
    }
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        if (active_job_id_ == context->job_id) {
            active_job_id_.clear();
        }
    }
}

}  // namespace Siligen::Application::UseCases::Dispensing
