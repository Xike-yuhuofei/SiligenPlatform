#define MODULE_NAME "DispensingWorkflowUseCase"

#include "../include/application/phase-control/DispensingWorkflowUseCase.h"
#include "../planning-trigger/PlanningUseCaseInternal.h"

#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace Siligen::Application::UseCases::Dispensing {

using Domain::Motion::Ports::IMotionStatePort;
using Domain::Safety::Ports::IInterlockSignalPort;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

namespace {

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

bool IsRuntimeTerminalState(const std::string& state) {
    return state == "completed" || state == "failed" || state == "cancelled";
}

std::uint32_t ToElapsedMs(const std::chrono::steady_clock::time_point start_time) {
    return static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count());
}

}  // namespace

DispensingWorkflowUseCase::DispensingWorkflowUseCase(
    std::shared_ptr<IUploadFilePort> upload_use_case,
    std::shared_ptr<PlanningUseCase> planning_use_case,
    std::shared_ptr<Siligen::Application::Ports::Dispensing::IWorkflowExecutionPort> execution_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
    std::shared_ptr<IMotionStatePort> motion_state_port,
    std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port,
    std::shared_ptr<IInterlockSignalPort> interlock_signal_port)
    : upload_use_case_(std::move(upload_use_case)),
      planning_use_case_(std::move(planning_use_case)),
      execution_port_(std::move(execution_port)),
      connection_port_(std::move(connection_port)),
      motion_state_port_(std::move(motion_state_port)),
      homing_port_(std::move(homing_port)),
      interlock_signal_port_(std::move(interlock_signal_port)) {
    if (!upload_use_case_ || !planning_use_case_ || !execution_port_) {
        throw std::invalid_argument("DispensingWorkflowUseCase: dependencies cannot be null");
    }
}

DispensingWorkflowUseCase::~DispensingWorkflowUseCase() = default;

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
    const auto prepare_start = std::chrono::steady_clock::now();
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

    auto planning_request = request.planning_request;
    if (planning_request.dxf_filepath.empty()) {
        planning_request.dxf_filepath = artifact.upload_response.filepath;
    }
    const auto authority_cache_key = planning_use_case_->BuildAuthorityCacheKey(planning_request);
    auto authority_resolve_result = ResolveAuthorityPreview(authority_cache_key, planning_request);
    if (authority_resolve_result.IsError()) {
        return Result<PreparePlanResponse>::Failure(authority_resolve_result.GetError());
    }
    const auto authority_resolution = authority_resolve_result.Value();
    const auto& authority_preview = authority_resolution.preview;

    PlanExecutionLaunch execution_launch;
    execution_launch.authority_preview = authority_preview;
    execution_launch.planning_request = planning_request;
    execution_launch.authority_cache_key = authority_cache_key;
    execution_launch.runtime_overrides = request.runtime_overrides;
    if (execution_launch.runtime_overrides.source_path.empty()) {
        execution_launch.runtime_overrides.source_path = artifact.upload_response.filepath;
    }

    PreparePlanResponse response;
    response.success = true;
    response.artifact_id = request.artifact_id;
    response.plan_id = GenerateId("plan");
    response.plan_fingerprint = BuildPlanFingerprint(request.artifact_id, authority_preview, execution_launch);
    response.filepath = artifact.upload_response.filepath;
    response.segment_count = static_cast<std::uint32_t>(std::max(0, authority_preview.artifacts.segment_count));
    response.point_count = static_cast<std::uint32_t>(authority_preview.artifacts.preview_trajectory_points.size());
    response.total_length_mm = authority_preview.artifacts.total_length;
    response.estimated_time_s = authority_preview.artifacts.estimated_time;
    response.preview_validation_classification = authority_preview.artifacts.preview_validation_classification;
    response.preview_exception_reason = authority_preview.artifacts.preview_exception_reason;
    response.preview_failure_reason = authority_preview.artifacts.preview_failure_reason;
    response.preview_diagnostic_code = authority_preview.preview_diagnostic_code;
    response.generated_at = ToIso8601UtcNow();
    response.performance_profile.authority_cache_hit = authority_resolution.cache_hit;
    response.performance_profile.authority_joined_inflight = authority_resolution.joined_inflight;
    response.performance_profile.authority_wait_ms = authority_resolution.wait_ms;
    response.performance_profile.pb_prepare_ms = authority_preview.authority_profile.pb_prepare_ms;
    response.performance_profile.path_load_ms = authority_preview.authority_profile.path_load_ms;
    response.performance_profile.process_path_ms = authority_preview.authority_profile.process_path_ms;
    response.performance_profile.authority_build_ms = authority_preview.authority_profile.authority_build_ms;
    response.performance_profile.authority_total_ms = authority_preview.authority_profile.total_ms;
    response.performance_profile.prepare_total_ms = ToElapsedMs(prepare_start);

    const auto log_prepare_profile = [&](const PreparePlanResponse& profile_response) {
        const auto& profile = profile_response.performance_profile;
        std::ostringstream oss;
        oss << "workflow_prepare_plan_profile"
            << " artifact_id=" << profile_response.artifact_id
            << " plan_id=" << profile_response.plan_id
            << " authority_cache_hit=" << (profile.authority_cache_hit ? 1 : 0)
            << " authority_joined_inflight=" << (profile.authority_joined_inflight ? 1 : 0)
            << " authority_wait_ms=" << profile.authority_wait_ms
            << " pb_ms=" << profile.pb_prepare_ms
            << " load_ms=" << profile.path_load_ms
            << " process_path_ms=" << profile.process_path_ms
            << " authority_build_ms=" << profile.authority_build_ms
            << " authority_total_ms=" << profile.authority_total_ms
            << " workflow_total_ms=" << profile.prepare_total_ms;
        SILIGEN_LOG_INFO(oss.str());
    };

    PlanRecord record;
    record.response = response;
    record.execution_launch = std::move(execution_launch);
    record.execution_trajectory_points = authority_preview.artifacts.preview_trajectory_points;
    record.glue_points = authority_preview.artifacts.glue_points;
    record.authority_trigger_layout = authority_preview.artifacts.authority_trigger_layout;
    record.preview_authority_ready = authority_preview.artifacts.preview_authority_ready;
    record.preview_authority_shared_with_execution = false;
    record.preview_binding_ready = authority_preview.artifacts.preview_binding_ready;
    record.execution_authority_shared_with_execution = false;
    record.execution_binding_ready = false;
    record.preview_spacing_valid = authority_preview.artifacts.preview_spacing_valid;
    record.preview_has_short_segment_exceptions = authority_preview.artifacts.preview_has_short_segment_exceptions;
    record.preview_validation_classification = authority_preview.artifacts.preview_validation_classification;
    record.preview_exception_reason = authority_preview.artifacts.preview_exception_reason;
    record.preview_failure_reason = authority_preview.artifacts.preview_failure_reason;
    record.preview_diagnostic_code = authority_preview.preview_diagnostic_code;
    record.preview_state = PlanPreviewState::PREPARED;
    record.latest = true;

    {
        std::lock_guard<std::mutex> lock(plans_mutex_);
        PlanRecord* reusable = nullptr;
        for (auto& [existing_plan_id, existing_record] : plans_) {
            if (existing_record.response.artifact_id == request.artifact_id &&
                existing_record.response.plan_fingerprint == response.plan_fingerprint) {
                reusable = &existing_record;
                break;
            }
        }
        if (reusable != nullptr) {
            for (auto& [existing_plan_id, existing_record] : plans_) {
                if (&existing_record == reusable) {
                    continue;
                }
                if (existing_record.response.artifact_id == request.artifact_id && existing_record.latest) {
                    existing_record.latest = false;
                    existing_record.preview_state = PlanPreviewState::STALE;
                    existing_record.confirmed_at.clear();
                }
            }
            reusable->latest = true;
            reusable->execution_launch.planning_request = planning_request;
            reusable->execution_launch.authority_preview = authority_preview;
            reusable->execution_launch.authority_cache_key = authority_cache_key;
            reusable->execution_launch.runtime_overrides = request.runtime_overrides;
            if (reusable->execution_launch.runtime_overrides.source_path.empty()) {
                reusable->execution_launch.runtime_overrides.source_path = artifact.upload_response.filepath;
            }
            reusable->response.filepath = artifact.upload_response.filepath;
            reusable->response.segment_count = static_cast<std::uint32_t>(std::max(0, authority_preview.artifacts.segment_count));
            reusable->response.point_count =
                static_cast<std::uint32_t>(authority_preview.artifacts.preview_trajectory_points.size());
            reusable->response.total_length_mm = authority_preview.artifacts.total_length;
            reusable->response.estimated_time_s = authority_preview.artifacts.estimated_time;
            reusable->response.generated_at = response.generated_at;
            reusable->response.performance_profile = response.performance_profile;
            reusable->response.preview_validation_classification = authority_preview.artifacts.preview_validation_classification;
            reusable->response.preview_exception_reason = authority_preview.artifacts.preview_exception_reason;
            reusable->response.preview_failure_reason = authority_preview.artifacts.preview_failure_reason;
            reusable->response.preview_diagnostic_code = authority_preview.preview_diagnostic_code;
            reusable->preview_authority_ready = authority_preview.artifacts.preview_authority_ready;
            reusable->preview_authority_shared_with_execution = false;
            reusable->preview_binding_ready = authority_preview.artifacts.preview_binding_ready;
            reusable->preview_spacing_valid = authority_preview.artifacts.preview_spacing_valid;
            reusable->preview_has_short_segment_exceptions = authority_preview.artifacts.preview_has_short_segment_exceptions;
            reusable->preview_validation_classification = authority_preview.artifacts.preview_validation_classification;
            reusable->preview_exception_reason = authority_preview.artifacts.preview_exception_reason;
            reusable->preview_failure_reason = authority_preview.artifacts.preview_failure_reason;
            reusable->preview_diagnostic_code = authority_preview.preview_diagnostic_code;
            reusable->glue_points = authority_preview.artifacts.glue_points;
            reusable->execution_trajectory_points = authority_preview.artifacts.preview_trajectory_points;
            reusable->authority_trigger_layout = authority_preview.artifacts.authority_trigger_layout;
            if (reusable->preview_state == PlanPreviewState::STALE) {
                reusable->preview_state =
                    (!reusable->confirmed_at.empty() &&
                     reusable->preview_snapshot_hash == reusable->response.plan_fingerprint)
                    ? PlanPreviewState::CONFIRMED
                    : (!reusable->preview_snapshot_hash.empty()
                           ? PlanPreviewState::SNAPSHOT_READY
                           : PlanPreviewState::PREPARED);
            }
            log_prepare_profile(reusable->response);
            return Result<PreparePlanResponse>::Success(reusable->response);
        }
        for (auto& [existing_plan_id, existing_record] : plans_) {
            if (existing_plan_id == response.plan_id) {
                continue;
            }
            if (existing_record.response.artifact_id == request.artifact_id && existing_record.latest) {
                existing_record.latest = false;
                existing_record.preview_state = PlanPreviewState::STALE;
                existing_record.confirmed_at.clear();
            }
        }
        plans_[response.plan_id] = record;
    }

    log_prepare_profile(response);
    return Result<PreparePlanResponse>::Success(std::move(response));
}

Result<PreviewSnapshotResponse> DispensingWorkflowUseCase::GetPreviewSnapshot(const PreviewSnapshotRequest& request) {
    if (request.plan_id.empty()) {
        return Result<PreviewSnapshotResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "plan_id is required", "DispensingWorkflowUseCase"));
    }

    auto binding_resolution = ResolvePreviewBindingRequirement(request.plan_id, false, nullptr, true);
    if (binding_resolution.IsError()) {
        return Result<PreviewSnapshotResponse>::Failure(binding_resolution.GetError());
    }

    auto snapshot_result = PromotePlanToSnapshotReady(
        request.plan_id,
        binding_resolution.Value().require_execution_binding);
    if (snapshot_result.IsError()) {
        return Result<PreviewSnapshotResponse>::Failure(snapshot_result.GetError());
    }

    auto snapshot_record = snapshot_result.Value();
    const bool has_export_process_path =
        !snapshot_record.execution_assembly.export_request.process_path.segments.empty();
    const bool has_authority_process_path =
        !snapshot_record.execution_launch.authority_preview.process_path.segments.empty();
    if (snapshot_record.execution_assembly.motion_trajectory_points.empty() &&
        !has_export_process_path &&
        !has_authority_process_path) {
        return Result<PreviewSnapshotResponse>::Failure(
            Error(
                ErrorCode::INVALID_STATE,
                "motion trajectory snapshot unavailable for preview",
                "DispensingWorkflowUseCase"));
    }

    return Result<PreviewSnapshotResponse>::Success(
        BuildPreviewSnapshotResponse(snapshot_record, request.max_polyline_points, request.max_glue_points));
}

Result<ConfirmPreviewResponse> DispensingWorkflowUseCase::ConfirmPreview(const ConfirmPreviewRequest& request) {
    if (request.plan_id.empty()) {
        return Result<ConfirmPreviewResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "plan_id is required", "DispensingWorkflowUseCase"));
    }
    if (request.snapshot_hash.empty()) {
        return Result<ConfirmPreviewResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "snapshot_hash is required", "DispensingWorkflowUseCase"));
    }

    auto binding_resolution = ResolvePreviewBindingRequirement(request.plan_id, true, &request.snapshot_hash, true);
    if (binding_resolution.IsError()) {
        return Result<ConfirmPreviewResponse>::Failure(binding_resolution.GetError());
    }

    return ConfirmPreviewReadyPlan(
        request.plan_id,
        request.snapshot_hash,
        binding_resolution.Value().require_execution_binding);
}

Result<StartJobResponse> DispensingWorkflowUseCase::StartJob(const StartJobRequest& request) {
    if (request.plan_id.empty()) {
        return Result<StartJobResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "plan_id is required", "DispensingWorkflowUseCase"));
    }
    if (request.plan_fingerprint.empty()) {
        return Result<StartJobResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "plan_fingerprint is required", "DispensingWorkflowUseCase"));
    }
    if (request.target_count == 0) {
        return Result<StartJobResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "target_count must be greater than 0", "DispensingWorkflowUseCase"));
    }

    auto initial_launch_result = ResolveStartJobExecutionLaunch(request, false);
    if (initial_launch_result.IsError()) {
        return Result<StartJobResponse>::Failure(initial_launch_result.GetError());
    }

    auto ensure_execution_result = EnsureExecutionAssemblyReady(request.plan_id);
    if (ensure_execution_result.IsError()) {
        return Result<StartJobResponse>::Failure(ensure_execution_result.GetError());
    }
    const auto execution_resolution = ensure_execution_result.Value();

    auto execution_launch_result = ResolveStartJobExecutionLaunch(request, true);
    if (execution_launch_result.IsError()) {
        return Result<StartJobResponse>::Failure(execution_launch_result.GetError());
    }
    auto execution_launch = execution_launch_result.Value();

    Siligen::Application::Ports::Dispensing::WorkflowRuntimeStartJobRequest runtime_request;
    runtime_request.plan_id = request.plan_id;
    runtime_request.execution_request = BuildExecutionRequest(execution_launch);
    runtime_request.plan_fingerprint = request.plan_fingerprint;
    runtime_request.target_count = request.target_count;
    auto execution_validation = runtime_request.execution_request.Validate();
    if (execution_validation.IsError()) {
        return Result<StartJobResponse>::Failure(execution_validation.GetError());
    }

    auto runtime_result = execution_port_->StartJob(runtime_request);
    if (runtime_result.IsError()) {
        return Result<StartJobResponse>::Failure(runtime_result.GetError());
    }

    const auto& job_id = runtime_result.Value();
    {
        std::lock_guard<std::mutex> lock(plans_mutex_);
        auto it = plans_.find(request.plan_id);
        if (it != plans_.end() && it->second.latest) {
            it->second.runtime_job_id = job_id;
        }
    }
    {
        std::lock_guard<std::mutex> lock(job_plan_index_mutex_);
        job_plan_index_[job_id] = request.plan_id;
    }

    StartJobResponse response;
    response.started = true;
    response.job_id = job_id;
    response.plan_id = request.plan_id;
    response.plan_fingerprint = request.plan_fingerprint;
    response.target_count = request.target_count;
    response.performance_profile.execution_cache_hit = execution_resolution.cache_hit;
    response.performance_profile.execution_joined_inflight = execution_resolution.joined_inflight;
    response.performance_profile.execution_wait_ms = execution_resolution.wait_ms;
    response.performance_profile.motion_plan_ms = execution_resolution.assembly.execution_profile.motion_plan_ms;
    response.performance_profile.assembly_ms = execution_resolution.assembly.execution_profile.assembly_ms;
    response.performance_profile.export_ms = execution_resolution.assembly.execution_profile.export_ms;
    response.performance_profile.execution_total_ms = execution_resolution.assembly.execution_profile.total_ms;
    return Result<StartJobResponse>::Success(std::move(response));
}

void DispensingWorkflowUseCase::OnRuntimeJobTerminal(const JobID& job_id, const PlanID& plan_id) const {
    if (job_id.empty() && plan_id.empty()) {
        return;
    }

    PlanID resolved_plan_id = plan_id;
    if (!job_id.empty()) {
        std::lock_guard<std::mutex> lock(job_plan_index_mutex_);
        auto it = job_plan_index_.find(job_id);
        if (it != job_plan_index_.end()) {
            if (resolved_plan_id.empty()) {
                resolved_plan_id = it->second;
            }
            job_plan_index_.erase(it);
        }
    }

    ReleaseConfirmedPreviewForPlan(resolved_plan_id, job_id.empty() ? nullptr : &job_id);
}

#ifdef SILIGEN_TEST_HOOKS
Result<ExecutionAssemblyTestProbe> DispensingWorkflowUseCase::EnsureExecutionAssemblyReadyForTesting(
    const PlanID& plan_id) const {
    auto assembly_result = EnsureExecutionAssemblyReady(plan_id);
    if (assembly_result.IsError()) {
        return Result<ExecutionAssemblyTestProbe>::Failure(assembly_result.GetError());
    }

    ExecutionAssemblyTestProbe probe;
    const auto& resolved = assembly_result.Value();
    probe.cache_hit = resolved.cache_hit;
    probe.joined_inflight = resolved.joined_inflight;
    probe.wait_ms = resolved.wait_ms;
    probe.plan_id = plan_id;
    probe.plan_fingerprint = resolved.assembly.authority_trigger_layout.plan_fingerprint;
    {
        std::lock_guard<std::mutex> lock(plans_mutex_);
        const auto it = plans_.find(plan_id);
        if (it != plans_.end()) {
            probe.plan_fingerprint = it->second.response.plan_fingerprint;
            probe.has_execution_launch_package = static_cast<bool>(it->second.execution_launch.execution_package);
            probe.has_execution_assembly_package = static_cast<bool>(it->second.execution_assembly.execution_package);
            probe.plan_execution_trajectory_point_count = it->second.execution_trajectory_points.size();
            probe.execution_assembly_trajectory_point_count =
                it->second.execution_assembly.execution_trajectory_points.size();
            if (it->second.execution_launch.execution_package) {
                const auto& launch_plan = it->second.execution_launch.execution_package->execution_plan;
                probe.execution_launch_interpolation_segment_count = launch_plan.interpolation_segments.size();
                probe.execution_launch_interpolation_point_count = launch_plan.interpolation_points.size();
                probe.execution_launch_motion_point_count = launch_plan.motion_trajectory.points.size();
            }
            if (it->second.execution_assembly.execution_package) {
                const auto& assembly_plan = it->second.execution_assembly.execution_package->execution_plan;
                probe.execution_assembly_interpolation_segment_count = assembly_plan.interpolation_segments.size();
                probe.execution_assembly_interpolation_point_count = assembly_plan.interpolation_points.size();
                probe.execution_assembly_motion_point_count = assembly_plan.motion_trajectory.points.size();
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(execution_assembly_cache_mutex_);
        probe.execution_cache_entry_count = execution_assembly_cache_.size();
        probe.execution_cache_contains_plan = execution_assembly_cache_.find(probe.plan_fingerprint) !=
            execution_assembly_cache_.end();
    }
    probe.authority_layout_id = resolved.assembly.authority_trigger_layout.layout_id;
    probe.execution_authority_shared_with_execution = resolved.assembly.preview_authority_shared_with_execution;
    probe.execution_binding_ready = resolved.assembly.execution_binding_ready;
    return Result<ExecutionAssemblyTestProbe>::Success(std::move(probe));
}
#endif

Result<Domain::Safety::ValueObjects::InterlockSignals> DispensingWorkflowUseCase::ReadInterlockSignals() const {
    if (!interlock_signal_port_) {
        return Result<Domain::Safety::ValueObjects::InterlockSignals>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "interlock signal port not available", "DispensingWorkflowUseCase"));
    }
    return interlock_signal_port_->ReadSignals();
}

bool DispensingWorkflowUseCase::IsInterlockLatched() const {
    if (!interlock_signal_port_) {
        return false;
    }
    return interlock_signal_port_->IsLatched();
}

PreviewSnapshotResponse DispensingWorkflowUseCase::BuildPreviewSnapshotResponse(
    const PlanRecord& plan_record,
    std::size_t max_polyline_points,
    std::size_t max_glue_points) {
    Siligen::Application::Services::Dispensing::PreviewSnapshotInput input;
    const auto& retained_authority_process_path = plan_record.execution_launch.authority_preview.process_path;
    input.snapshot_id = plan_record.preview_snapshot_id;
    input.snapshot_hash = plan_record.preview_snapshot_hash;
    input.plan_id = plan_record.response.plan_id;
    input.preview_state = PreviewStateToString(plan_record.preview_state);
    input.confirmed_at = plan_record.confirmed_at;
    input.segment_count = plan_record.response.segment_count;
    input.point_count = plan_record.response.point_count;
    input.total_length_mm = plan_record.response.total_length_mm;
    input.estimated_time_s = plan_record.response.estimated_time_s;
    input.generated_at = plan_record.preview_generated_at;
    input.trajectory_points = &plan_record.execution_trajectory_points;
    if (!plan_record.execution_assembly.motion_trajectory_points.empty()) {
        input.motion_trajectory_points = &plan_record.execution_assembly.motion_trajectory_points;
    } else if (!plan_record.execution_assembly.export_request.process_path.segments.empty()) {
        input.process_path = &plan_record.execution_assembly.export_request.process_path;
    } else if (!retained_authority_process_path.segments.empty()) {
        input.process_path = &retained_authority_process_path;
    }
    input.glue_points = &plan_record.glue_points;
    input.authority_trigger_layout = &plan_record.authority_trigger_layout;
    input.authority_layout_id = plan_record.authority_trigger_layout.layout_id;
    input.binding_ready = plan_record.preview_binding_ready;
    input.validation_classification = plan_record.preview_validation_classification;
    input.exception_reason = plan_record.preview_exception_reason;
    input.failure_reason = plan_record.preview_failure_reason;
    input.diagnostic_code = plan_record.preview_diagnostic_code;

    Siligen::Application::Services::Dispensing::PreviewSnapshotService snapshot_service;
    return snapshot_service.BuildResponse(input, max_polyline_points, max_glue_points);
}

std::string DispensingWorkflowUseCase::GenerateId(const char* prefix) {
    const auto seq = id_sequence_.fetch_add(1, std::memory_order_relaxed) + 1;
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return std::string(prefix) + "-" + std::to_string(millis) + "-" + std::to_string(seq);
}

Siligen::Application::Ports::Dispensing::WorkflowExecutionRequest
DispensingWorkflowUseCase::BuildExecutionRequest(const PlanExecutionLaunch& launch) const {
    Siligen::Application::Ports::Dispensing::WorkflowExecutionRequest request;
    if (launch.execution_package) {
        request.execution_package = *launch.execution_package;
    }
    request.source_path = launch.runtime_overrides.source_path;
    request.use_hardware_trigger = launch.runtime_overrides.use_hardware_trigger;
    request.dry_run = launch.runtime_overrides.dry_run;
    request.machine_mode = launch.runtime_overrides.machine_mode;
    request.execution_mode = launch.runtime_overrides.execution_mode;
    request.output_policy = launch.runtime_overrides.output_policy;
    request.max_jerk = launch.runtime_overrides.max_jerk;
    request.arc_tolerance_mm = launch.runtime_overrides.arc_tolerance_mm;
    request.dispensing_speed_mm_s = launch.runtime_overrides.dispensing_speed_mm_s;
    request.dry_run_speed_mm_s = launch.runtime_overrides.dry_run_speed_mm_s;
    request.rapid_speed_mm_s = launch.runtime_overrides.rapid_speed_mm_s;
    request.acceleration_mm_s2 = launch.runtime_overrides.acceleration_mm_s2;
    request.velocity_trace_enabled = launch.runtime_overrides.velocity_trace_enabled;
    request.velocity_trace_interval_ms = launch.runtime_overrides.velocity_trace_interval_ms;
    request.velocity_trace_path = launch.runtime_overrides.velocity_trace_path;
    request.velocity_guard_enabled = launch.runtime_overrides.velocity_guard_enabled;
    request.velocity_guard_ratio = launch.runtime_overrides.velocity_guard_ratio;
    request.velocity_guard_abs_mm_s = launch.runtime_overrides.velocity_guard_abs_mm_s;
    request.velocity_guard_min_expected_mm_s = launch.runtime_overrides.velocity_guard_min_expected_mm_s;
    request.velocity_guard_grace_ms = launch.runtime_overrides.velocity_guard_grace_ms;
    request.velocity_guard_interval_ms = launch.runtime_overrides.velocity_guard_interval_ms;
    request.velocity_guard_max_consecutive = launch.runtime_overrides.velocity_guard_max_consecutive;
    request.velocity_guard_stop_on_violation = launch.runtime_overrides.velocity_guard_stop_on_violation;
    return request;
}

std::string DispensingWorkflowUseCase::BuildPlanFingerprint(
    const ArtifactID& artifact_id,
    const PreparedAuthorityPreview& authority_preview,
    const PlanExecutionLaunch& execution_launch) const {
    std::ostringstream oss;
    const auto runtime_request = BuildExecutionRequest(execution_launch);
    const auto machine_mode = runtime_request.ResolveMachineMode();
    const auto execution_mode = runtime_request.ResolveExecutionMode();
    const auto output_policy = runtime_request.ResolveOutputPolicy();

    oss << artifact_id << '|'
        << execution_launch.authority_cache_key << '|'
        << runtime_request.dry_run << '|'
        << Domain::Machine::ValueObjects::ToString(machine_mode) << '|'
        << Domain::Dispensing::ValueObjects::ToString(execution_mode) << '|'
        << Domain::Dispensing::ValueObjects::ToString(output_policy) << '|'
        << authority_preview.artifacts.segment_count << '|'
        << authority_preview.artifacts.preview_trajectory_points.size() << '|'
        << authority_preview.artifacts.glue_points.size() << '|'
        << authority_preview.artifacts.preview_authority_ready << '|'
        << authority_preview.artifacts.preview_binding_ready << '|'
        << authority_preview.artifacts.preview_spacing_valid << '|'
        << authority_preview.artifacts.preview_has_short_segment_exceptions << '|'
        << authority_preview.artifacts.preview_validation_classification << '|'
        << authority_preview.artifacts.preview_exception_reason << '|'
        << authority_preview.artifacts.authority_trigger_layout.layout_id << '|'
        << authority_preview.artifacts.authority_trigger_points.size() << '|'
        << authority_preview.artifacts.total_length << '|'
        << authority_preview.artifacts.estimated_time << '|'
        << runtime_request.use_hardware_trigger << '|'
        << runtime_request.max_jerk << '|'
        << runtime_request.arc_tolerance_mm << '|'
        << runtime_request.dispensing_speed_mm_s.value_or(0.0f) << '|'
        << runtime_request.dry_run_speed_mm_s.value_or(0.0f) << '|'
        << runtime_request.rapid_speed_mm_s.value_or(0.0f) << '|'
        << runtime_request.acceleration_mm_s2.value_or(0.0f) << '|'
        << runtime_request.velocity_trace_enabled << '|'
        << runtime_request.velocity_trace_interval_ms << '|'
        << runtime_request.velocity_trace_path << '|'
        << runtime_request.velocity_guard_enabled << '|'
        << runtime_request.velocity_guard_ratio << '|'
        << runtime_request.velocity_guard_abs_mm_s << '|'
        << runtime_request.velocity_guard_min_expected_mm_s << '|'
        << runtime_request.velocity_guard_grace_ms << '|'
        << runtime_request.velocity_guard_interval_ms << '|'
        << runtime_request.velocity_guard_max_consecutive << '|'
        << runtime_request.velocity_guard_stop_on_violation;
    return HexEncodeUint64(Fnv1a64(oss.str()));
}

Result<DispensingWorkflowUseCase::AuthorityPreviewResolveResult> DispensingWorkflowUseCase::ResolveAuthorityPreview(
    const std::string& authority_cache_key,
    const PlanningRequest& planning_request) const {
    AuthorityPreviewResolveResult resolved;
    {
        std::lock_guard<std::mutex> lock(authority_preview_cache_mutex_);
        auto cache_it = authority_preview_cache_.find(authority_cache_key);
        if (cache_it != authority_preview_cache_.end()) {
            resolved.preview = cache_it->second.preview;
            resolved.cache_hit = true;
            return Result<AuthorityPreviewResolveResult>::Success(std::move(resolved));
        }
    }

    std::shared_ptr<AuthorityPreviewInFlight> inflight;
    bool is_leader = false;
    {
        std::lock_guard<std::mutex> lock(authority_preview_inflight_mutex_);
        auto inflight_it = authority_preview_inflight_.find(authority_cache_key);
        if (inflight_it != authority_preview_inflight_.end()) {
            inflight = inflight_it->second;
            resolved.joined_inflight = true;
        } else {
            inflight = std::make_shared<AuthorityPreviewInFlight>();
            authority_preview_inflight_[authority_cache_key] = inflight;
            is_leader = true;
        }
    }

    if (!is_leader) {
        const auto wait_start = std::chrono::steady_clock::now();
        std::unique_lock<std::mutex> wait_lock(inflight->mutex);
        inflight->cv.wait(wait_lock, [&inflight]() { return inflight->ready; });
        resolved.wait_ms = ToElapsedMs(wait_start);
        if (inflight->error.has_value()) {
            return Result<AuthorityPreviewResolveResult>::Failure(*inflight->error);
        }
        resolved.preview = inflight->preview;
        return Result<AuthorityPreviewResolveResult>::Success(std::move(resolved));
    }

    auto authority_result = PlanningUseCaseInternalAccess::PrepareAuthorityPreview(
        *planning_use_case_,
        planning_request);
    if (authority_result.IsSuccess()) {
        std::lock_guard<std::mutex> cache_lock(authority_preview_cache_mutex_);
        authority_preview_cache_[authority_cache_key] = AuthorityPreviewCacheEntry{authority_result.Value()};
    }
    {
        std::lock_guard<std::mutex> entry_lock(inflight->mutex);
        if (authority_result.IsSuccess()) {
            inflight->preview = authority_result.Value();
        } else {
            inflight->error = authority_result.GetError();
        }
        inflight->ready = true;
    }
    {
        std::lock_guard<std::mutex> lock(authority_preview_inflight_mutex_);
        authority_preview_inflight_.erase(authority_cache_key);
    }
    inflight->cv.notify_all();

    if (authority_result.IsError()) {
        return Result<AuthorityPreviewResolveResult>::Failure(authority_result.GetError());
    }
    resolved.preview = authority_result.Value();
    return Result<AuthorityPreviewResolveResult>::Success(std::move(resolved));
}

Result<DispensingWorkflowUseCase::ExecutionAssemblyResolveResult> DispensingWorkflowUseCase::ResolveExecutionAssembly(
    const std::string& execution_cache_key,
    const PlanExecutionLaunch& execution_launch) const {
    ExecutionAssemblyResolveResult resolved;
    {
        std::lock_guard<std::mutex> lock(execution_assembly_cache_mutex_);
        auto cache_it = execution_assembly_cache_.find(execution_cache_key);
        if (cache_it != execution_assembly_cache_.end()) {
            resolved.assembly = cache_it->second.assembly;
            resolved.cache_hit = true;
            return Result<ExecutionAssemblyResolveResult>::Success(std::move(resolved));
        }
    }

    std::shared_ptr<ExecutionAssemblyInFlight> inflight;
    bool is_leader = false;
    {
        std::lock_guard<std::mutex> lock(execution_assembly_inflight_mutex_);
        auto inflight_it = execution_assembly_inflight_.find(execution_cache_key);
        if (inflight_it != execution_assembly_inflight_.end()) {
            inflight = inflight_it->second;
            resolved.joined_inflight = true;
        } else {
            inflight = std::make_shared<ExecutionAssemblyInFlight>();
            execution_assembly_inflight_[execution_cache_key] = inflight;
            is_leader = true;
        }
    }

    if (!is_leader) {
        const auto wait_start = std::chrono::steady_clock::now();
        std::unique_lock<std::mutex> wait_lock(inflight->mutex);
        inflight->cv.wait(wait_lock, [&inflight]() { return inflight->ready; });
        resolved.wait_ms = ToElapsedMs(wait_start);
        if (inflight->error.has_value()) {
            return Result<ExecutionAssemblyResolveResult>::Failure(*inflight->error);
        }
        resolved.assembly = inflight->assembly;
        return Result<ExecutionAssemblyResolveResult>::Success(std::move(resolved));
    }

    auto assembly_result = PlanningUseCaseInternalAccess::AssembleExecutionFromAuthority(
        *planning_use_case_,
        execution_launch.planning_request,
        execution_launch.authority_preview);
    if (assembly_result.IsSuccess()) {
        std::lock_guard<std::mutex> cache_lock(execution_assembly_cache_mutex_);
        execution_assembly_cache_[execution_cache_key] = ExecutionAssemblyCacheEntry{assembly_result.Value()};
    }
    {
        std::lock_guard<std::mutex> entry_lock(inflight->mutex);
        if (assembly_result.IsSuccess()) {
            inflight->assembly = assembly_result.Value();
        } else {
            inflight->error = assembly_result.GetError();
        }
        inflight->ready = true;
    }
    {
        std::lock_guard<std::mutex> lock(execution_assembly_inflight_mutex_);
        execution_assembly_inflight_.erase(execution_cache_key);
    }
    inflight->cv.notify_all();

    if (assembly_result.IsError()) {
        return Result<ExecutionAssemblyResolveResult>::Failure(assembly_result.GetError());
    }
    resolved.assembly = assembly_result.Value();
    return Result<ExecutionAssemblyResolveResult>::Success(std::move(resolved));
}

Result<DispensingWorkflowUseCase::ExecutionAssemblyResolveResult> DispensingWorkflowUseCase::EnsureExecutionAssemblyReady(
    const PlanID& plan_id) const {
    if (plan_id.empty()) {
        return Result<ExecutionAssemblyResolveResult>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "plan_id is required", "DispensingWorkflowUseCase"));
    }

    PlanExecutionLaunch execution_launch;
    std::string execution_cache_key;
    ExecutionAssemblyResolveResult assembly_resolution;
    bool already_resolved = false;
    {
        std::lock_guard<std::mutex> lock(plans_mutex_);
        auto it = plans_.find(plan_id);
        if (it == plans_.end() || !it->second.latest) {
            return Result<ExecutionAssemblyResolveResult>::Failure(
                Error(ErrorCode::NOT_FOUND, "plan not found", "DispensingWorkflowUseCase"));
        }
        if (it->second.execution_launch.execution_package) {
            assembly_resolution.assembly = it->second.execution_assembly;
            assembly_resolution.cache_hit = true;
            already_resolved = true;
        } else {
            execution_launch = it->second.execution_launch;
            execution_cache_key = it->second.response.plan_fingerprint;
        }
    }

    if (!already_resolved) {
        auto assembly_resolve_result = ResolveExecutionAssembly(execution_cache_key, execution_launch);
        if (assembly_resolve_result.IsError()) {
            std::lock_guard<std::mutex> lock(plans_mutex_);
            auto it = plans_.find(plan_id);
            if (it != plans_.end() && it->second.latest) {
                it->second.preview_authority_shared_with_execution = false;
                it->second.preview_binding_ready = false;
                it->second.execution_authority_shared_with_execution = false;
                it->second.execution_binding_ready = false;
                it->second.preview_failure_reason = assembly_resolve_result.GetError().GetMessage();
            }
            return Result<ExecutionAssemblyResolveResult>::Failure(assembly_resolve_result.GetError());
        }
        assembly_resolution = assembly_resolve_result.Value();
    }

    const auto& assembly = assembly_resolution.assembly;

    std::lock_guard<std::mutex> lock(plans_mutex_);
    auto it = plans_.find(plan_id);
    if (it == plans_.end() || !it->second.latest) {
        return Result<ExecutionAssemblyResolveResult>::Failure(
            Error(ErrorCode::NOT_FOUND, "plan not found", "DispensingWorkflowUseCase"));
    }
    it->second.execution_launch.execution_package = assembly.execution_package;
    it->second.execution_assembly = assembly;
    it->second.execution_trajectory_points = assembly.execution_trajectory_points;
    it->second.response.point_count = static_cast<std::uint32_t>(assembly.execution_trajectory_points.size());
    it->second.authority_trigger_layout = assembly.authority_trigger_layout;
    it->second.preview_authority_shared_with_execution = assembly.preview_authority_shared_with_execution;
    it->second.preview_binding_ready = assembly.execution_binding_ready;
    it->second.execution_authority_shared_with_execution = assembly.preview_authority_shared_with_execution;
    it->second.execution_binding_ready = assembly.execution_binding_ready;
    if (!assembly.execution_failure_reason.empty()) {
        it->second.preview_failure_reason = assembly.execution_failure_reason;
    } else {
        it->second.preview_failure_reason = it->second.execution_launch.authority_preview.artifacts.preview_failure_reason;
    }
    std::ostringstream oss;
    oss << "workflow_execution_assembly_profile"
        << " plan_id=" << plan_id
        << " execution_cache_hit=" << (assembly_resolution.cache_hit ? 1 : 0)
        << " execution_joined_inflight=" << (assembly_resolution.joined_inflight ? 1 : 0)
        << " execution_wait_ms=" << assembly_resolution.wait_ms
        << " motion_plan_ms=" << assembly.execution_profile.motion_plan_ms
        << " assembly_ms=" << assembly.execution_profile.assembly_ms
        << " export_ms=" << assembly.execution_profile.export_ms
        << " execution_total_ms=" << assembly.execution_profile.total_ms;
    SILIGEN_LOG_INFO(oss.str());
    return Result<ExecutionAssemblyResolveResult>::Success(assembly_resolution);
}

bool DispensingWorkflowUseCase::RequiresExecutionBinding(const PlanRecord& plan_record) const {
    return !plan_record.execution_launch.authority_cache_key.empty() ||
           static_cast<bool>(plan_record.execution_launch.execution_package) ||
           plan_record.execution_authority_shared_with_execution ||
           plan_record.execution_binding_ready;
}

bool DispensingWorkflowUseCase::ShouldResolveExecutionBinding(const PlanRecord& plan_record) const {
    const bool require_execution_binding = RequiresExecutionBinding(plan_record);
    return require_execution_binding &&
           (!plan_record.execution_launch.execution_package ||
            !plan_record.preview_authority_shared_with_execution ||
            !plan_record.preview_binding_ready ||
            !plan_record.execution_authority_shared_with_execution ||
            !plan_record.execution_binding_ready);
}

Result<DispensingWorkflowUseCase::PreviewBindingResolution> DispensingWorkflowUseCase::ResolvePreviewBindingRequirement(
    const PlanID& plan_id,
    bool require_snapshot_ready,
    const std::string* snapshot_hash,
    bool mark_failed) const {
    PreviewBindingResolution resolution;
    bool should_resolve_execution = false;
    {
        std::lock_guard<std::mutex> lock(plans_mutex_);
        auto it = plans_.find(plan_id);
        if (it == plans_.end()) {
            return Result<PreviewBindingResolution>::Failure(
                Error(ErrorCode::NOT_FOUND, "plan not found", "DispensingWorkflowUseCase"));
        }
        if (!it->second.latest) {
            return Result<PreviewBindingResolution>::Failure(
                Error(ErrorCode::INVALID_STATE, "plan is stale", "DispensingWorkflowUseCase"));
        }
        if (require_snapshot_ready &&
            it->second.preview_state != PlanPreviewState::SNAPSHOT_READY &&
            it->second.preview_state != PlanPreviewState::CONFIRMED) {
            return Result<PreviewBindingResolution>::Failure(
                Error(ErrorCode::INVALID_STATE, "preview snapshot not prepared", "DispensingWorkflowUseCase"));
        }
        if (snapshot_hash != nullptr && *snapshot_hash != it->second.preview_snapshot_hash) {
            return Result<PreviewBindingResolution>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "snapshot hash mismatch", "DispensingWorkflowUseCase"));
        }
        if (auto preview_gate_error = BuildPreviewGateError(it->second, mark_failed, false); preview_gate_error.has_value()) {
            return Result<PreviewBindingResolution>::Failure(preview_gate_error.value());
        }

        resolution.require_execution_binding = RequiresExecutionBinding(it->second);
        should_resolve_execution = ShouldResolveExecutionBinding(it->second);
    }

    if (should_resolve_execution) {
        auto ensure_result = EnsureExecutionAssemblyReady(plan_id);
        if (ensure_result.IsError()) {
            resolution.require_execution_binding = true;
        }
    }

    return Result<PreviewBindingResolution>::Success(std::move(resolution));
}

Result<DispensingWorkflowUseCase::PlanRecord> DispensingWorkflowUseCase::PromotePlanToSnapshotReady(
    const PlanID& plan_id,
    bool require_execution_binding) {
    PlanRecord snapshot_record;
    {
        std::lock_guard<std::mutex> lock(plans_mutex_);
        auto it = plans_.find(plan_id);
        if (it == plans_.end()) {
            return Result<PlanRecord>::Failure(
                Error(ErrorCode::NOT_FOUND, "plan not found", "DispensingWorkflowUseCase"));
        }
        if (!it->second.latest) {
            return Result<PlanRecord>::Failure(
                Error(ErrorCode::INVALID_STATE, "plan is stale", "DispensingWorkflowUseCase"));
        }
        if (auto preview_gate_error = BuildPreviewGateError(
                it->second, true, require_execution_binding); preview_gate_error.has_value()) {
            return Result<PlanRecord>::Failure(preview_gate_error.value());
        }

        const bool keep_confirmed_state =
            it->second.preview_state == PlanPreviewState::CONFIRMED &&
            !it->second.preview_snapshot_hash.empty() &&
            it->second.preview_snapshot_hash == it->second.response.plan_fingerprint;

        it->second.preview_snapshot_id = it->second.response.plan_id;
        it->second.preview_snapshot_hash = it->second.response.plan_fingerprint;
        it->second.preview_generated_at = ToIso8601UtcNow();
        if (!keep_confirmed_state) {
            it->second.preview_state = PlanPreviewState::SNAPSHOT_READY;
            it->second.confirmed_at.clear();
        }
        it->second.failure_message.clear();
        snapshot_record = it->second;
    }

    return Result<PlanRecord>::Success(std::move(snapshot_record));
}

Result<ConfirmPreviewResponse> DispensingWorkflowUseCase::ConfirmPreviewReadyPlan(
    const PlanID& plan_id,
    const std::string& snapshot_hash,
    bool require_execution_binding) {
    ConfirmPreviewResponse response;
    {
        std::lock_guard<std::mutex> lock(plans_mutex_);
        auto it = plans_.find(plan_id);
        if (it == plans_.end()) {
            return Result<ConfirmPreviewResponse>::Failure(
                Error(ErrorCode::NOT_FOUND, "plan not found", "DispensingWorkflowUseCase"));
        }
        if (!it->second.latest) {
            return Result<ConfirmPreviewResponse>::Failure(
                Error(ErrorCode::INVALID_STATE, "plan is stale", "DispensingWorkflowUseCase"));
        }
        if (it->second.preview_state != PlanPreviewState::SNAPSHOT_READY &&
            it->second.preview_state != PlanPreviewState::CONFIRMED) {
            return Result<ConfirmPreviewResponse>::Failure(
                Error(ErrorCode::INVALID_STATE, "preview snapshot not prepared", "DispensingWorkflowUseCase"));
        }
        if (snapshot_hash != it->second.preview_snapshot_hash) {
            return Result<ConfirmPreviewResponse>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "snapshot hash mismatch", "DispensingWorkflowUseCase"));
        }
        if (auto preview_gate_error = BuildPreviewGateError(
                it->second, true, require_execution_binding); preview_gate_error.has_value()) {
            return Result<ConfirmPreviewResponse>::Failure(preview_gate_error.value());
        }

        it->second.preview_state = PlanPreviewState::CONFIRMED;
        if (it->second.confirmed_at.empty()) {
            it->second.confirmed_at = ToIso8601UtcNow();
        }
        it->second.failure_message.clear();

        response.confirmed = true;
        response.plan_id = it->second.response.plan_id;
        response.snapshot_hash = it->second.preview_snapshot_hash;
        response.preview_state = PreviewStateToString(it->second.preview_state);
        response.confirmed_at = it->second.confirmed_at;
    }

    return Result<ConfirmPreviewResponse>::Success(std::move(response));
}

Result<DispensingWorkflowUseCase::PlanExecutionLaunch> DispensingWorkflowUseCase::ResolveStartJobExecutionLaunch(
    const StartJobRequest& request,
    bool require_execution_binding) const {
    std::lock_guard<std::mutex> lock(plans_mutex_);
    auto it = plans_.find(request.plan_id);
    if (it == plans_.end()) {
        return Result<PlanExecutionLaunch>::Failure(
            Error(ErrorCode::NOT_FOUND, "plan not found", "DispensingWorkflowUseCase"));
    }
    if (!it->second.latest) {
        return Result<PlanExecutionLaunch>::Failure(
            Error(ErrorCode::INVALID_STATE, "plan is stale", "DispensingWorkflowUseCase"));
    }
    if (it->second.preview_state != PlanPreviewState::CONFIRMED) {
        return Result<PlanExecutionLaunch>::Failure(
            Error(ErrorCode::INVALID_STATE, "preview not confirmed", "DispensingWorkflowUseCase"));
    }
    if (auto preview_gate_error = BuildPreviewGateError(
            it->second, false, require_execution_binding); preview_gate_error.has_value()) {
        return Result<PlanExecutionLaunch>::Failure(preview_gate_error.value());
    }
    if (it->second.preview_snapshot_hash != it->second.response.plan_fingerprint) {
        return Result<PlanExecutionLaunch>::Failure(
            Error(ErrorCode::INVALID_STATE, "preview fingerprint mismatch", "DispensingWorkflowUseCase"));
    }
    if (request.plan_fingerprint != it->second.response.plan_fingerprint) {
        return Result<PlanExecutionLaunch>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "plan fingerprint mismatch", "DispensingWorkflowUseCase"));
    }

    return Result<PlanExecutionLaunch>::Success(it->second.execution_launch);
}

DispensingWorkflowUseCase::PreviewGateDiagnostic DispensingWorkflowUseCase::BuildPreviewGateDiagnostic(
    const PlanRecord& plan_record,
    bool require_execution_binding) const {
    PreviewGateDiagnostic diagnostic;
    diagnostic.layout_id = plan_record.authority_trigger_layout.layout_id;
    diagnostic.authority_ready = plan_record.preview_authority_ready;
    diagnostic.authority_shared_with_execution = plan_record.preview_authority_shared_with_execution;
    diagnostic.binding_ready = plan_record.preview_binding_ready;
    diagnostic.execution_authority_shared_with_execution = plan_record.execution_authority_shared_with_execution;
    diagnostic.execution_binding_ready = plan_record.execution_binding_ready;
    diagnostic.spacing_valid = plan_record.preview_spacing_valid;
    diagnostic.validation_classification = plan_record.preview_validation_classification;
    diagnostic.exception_reason = plan_record.preview_exception_reason;
    diagnostic.failure_reason = plan_record.preview_failure_reason;
    diagnostic.glue_point_count = plan_record.glue_points.size();

    if (diagnostic.layout_id.empty()) {
        diagnostic.owner_message = "preview authority layout id unavailable";
        return diagnostic;
    }
    if (!diagnostic.authority_ready) {
        diagnostic.owner_message = diagnostic.failure_reason.empty()
            ? "preview authority unavailable"
            : diagnostic.failure_reason;
        return diagnostic;
    }
    if (!diagnostic.binding_ready) {
        diagnostic.owner_message = diagnostic.failure_reason.empty()
            ? "preview binding unavailable"
            : diagnostic.failure_reason;
        return diagnostic;
    }
    if (!diagnostic.spacing_valid || diagnostic.validation_classification == "fail") {
        if (!diagnostic.failure_reason.empty()) {
            diagnostic.owner_message = diagnostic.failure_reason;
            return diagnostic;
        }
        if (diagnostic.validation_classification == "fail" && !diagnostic.exception_reason.empty()) {
            diagnostic.owner_message = diagnostic.exception_reason;
            return diagnostic;
        }
        diagnostic.owner_message = "preview spacing validation failed";
        return diagnostic;
    }
    if (diagnostic.glue_point_count == 0U) {
        diagnostic.owner_message = "glue points unavailable";
        return diagnostic;
    }
    if (require_execution_binding) {
        if (!diagnostic.execution_authority_shared_with_execution) {
            diagnostic.owner_message = "preview authority is not shared with execution";
            return diagnostic;
        }
        if (!diagnostic.execution_binding_ready) {
            diagnostic.owner_message = diagnostic.failure_reason.empty()
                ? "authority trigger binding unavailable"
                : diagnostic.failure_reason;
            return diagnostic;
        }
    }
    if (diagnostic.validation_classification == "pass_with_exception") {
        std::ostringstream oss;
        oss << "workflow_preview_gate_topology"
            << " stage=pass_with_exception"
            << " plan_id=" << plan_record.response.plan_id
            << " layout_id=" << plan_record.authority_trigger_layout.layout_id
            << " layout_dispatch_type="
            << Siligen::Domain::Dispensing::ValueObjects::ToString(
                   plan_record.authority_trigger_layout.dispatch_type)
            << " effective_components=" << plan_record.authority_trigger_layout.effective_component_count
            << " ignored_components=" << plan_record.authority_trigger_layout.ignored_component_count
            << " validation_classification=" << plan_record.preview_validation_classification
            << " failure_message=" << diagnostic.exception_reason
            << " spans=" << plan_record.authority_trigger_layout.spans.size();
        for (std::size_t index = 0; index < plan_record.authority_trigger_layout.spans.size() && index < 4U; ++index) {
            const auto& span = plan_record.authority_trigger_layout.spans[index];
            const auto outcome_it = std::find_if(
                plan_record.authority_trigger_layout.validation_outcomes.begin(),
                plan_record.authority_trigger_layout.validation_outcomes.end(),
                [&](const auto& outcome) { return outcome.span_ref == span.span_id; });
            oss << " span" << index
                << "_component_id=" << span.component_id
                << " span" << index
                << "_component_index=" << span.component_index
                << " span" << index
                << "_dispatch=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.dispatch_type)
                << " span" << index
                << "_topology=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.topology_type)
                << " span" << index
                << "_split=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.split_reason)
                << " span" << index
                << "_anchor_policy=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.anchor_policy)
                << " span" << index
                << "_phase_strategy=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.phase_strategy)
                << " span" << index
                << "_validation=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.validation_state)
                << " span" << index
                << "_candidate_corners=" << span.candidate_corner_count
                << " span" << index
                << "_accepted_corners=" << span.accepted_corner_count
                << " span" << index
                << "_dense_corner_clusters=" << span.dense_corner_cluster_count
                << " span" << index
                << "_anchor_constraint_state="
                << (outcome_it != plan_record.authority_trigger_layout.validation_outcomes.end() &&
                            !outcome_it->anchor_constraint_state.empty()
                        ? outcome_it->anchor_constraint_state
                        : "not_applicable");
        }
        SILIGEN_LOG_INFO(oss.str());
    }
    return diagnostic;
}

std::optional<Siligen::Shared::Types::Error> DispensingWorkflowUseCase::BuildPreviewGateError(
    PlanRecord& plan_record,
    bool mark_failed,
    bool require_execution_binding) const {
    const auto diagnostic = BuildPreviewGateDiagnostic(plan_record, require_execution_binding);
    if (diagnostic.owner_message.empty()) {
        return std::nullopt;
    }

    {
        std::ostringstream oss;
        oss << "workflow_preview_gate_topology"
            << " stage=failed"
            << " plan_id=" << plan_record.response.plan_id
            << " layout_id=" << plan_record.authority_trigger_layout.layout_id
            << " layout_dispatch_type="
            << Siligen::Domain::Dispensing::ValueObjects::ToString(
                   plan_record.authority_trigger_layout.dispatch_type)
            << " effective_components=" << plan_record.authority_trigger_layout.effective_component_count
            << " ignored_components=" << plan_record.authority_trigger_layout.ignored_component_count
            << " validation_classification=" << plan_record.preview_validation_classification
            << " failure_message=" << diagnostic.owner_message
            << " spans=" << plan_record.authority_trigger_layout.spans.size();
        for (std::size_t index = 0; index < plan_record.authority_trigger_layout.spans.size() && index < 4U; ++index) {
            const auto& span = plan_record.authority_trigger_layout.spans[index];
            const auto outcome_it = std::find_if(
                plan_record.authority_trigger_layout.validation_outcomes.begin(),
                plan_record.authority_trigger_layout.validation_outcomes.end(),
                [&](const auto& outcome) { return outcome.span_ref == span.span_id; });
            oss << " span" << index
                << "_component_id=" << span.component_id
                << " span" << index
                << "_component_index=" << span.component_index
                << " span" << index
                << "_dispatch=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.dispatch_type)
                << " span" << index
                << "_topology=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.topology_type)
                << " span" << index
                << "_split=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.split_reason)
                << " span" << index
                << "_anchor_policy=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.anchor_policy)
                << " span" << index
                << "_phase_strategy=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.phase_strategy)
                << " span" << index
                << "_validation=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.validation_state)
                << " span" << index
                << "_candidate_corners=" << span.candidate_corner_count
                << " span" << index
                << "_accepted_corners=" << span.accepted_corner_count
                << " span" << index
                << "_dense_corner_clusters=" << span.dense_corner_cluster_count
                << " span" << index
                << "_anchor_constraint_state="
                << (outcome_it != plan_record.authority_trigger_layout.validation_outcomes.end() &&
                            !outcome_it->anchor_constraint_state.empty()
                        ? outcome_it->anchor_constraint_state
                        : "not_applicable");
        }
        SILIGEN_LOG_INFO(oss.str());
    }

    if (mark_failed) {
        plan_record.preview_state = PlanPreviewState::FAILED;
        plan_record.confirmed_at.clear();
    }
    plan_record.failure_message = diagnostic.owner_message;
    return Error(
        ErrorCode::INVALID_STATE,
        diagnostic.owner_message,
        "DispensingWorkflowUseCase");
}

std::string DispensingWorkflowUseCase::ResolvePreviewGateFailure(const PlanRecord& plan_record) const {
    return BuildPreviewGateDiagnostic(plan_record, true).owner_message;
}

std::string DispensingWorkflowUseCase::PreviewStateToString(PlanPreviewState state) const {
    switch (state) {
        case PlanPreviewState::PREPARED:
            return "prepared";
        case PlanPreviewState::SNAPSHOT_READY:
            return "snapshot_ready";
        case PlanPreviewState::CONFIRMED:
            return "confirmed";
        case PlanPreviewState::STALE:
            return "stale";
        case PlanPreviewState::FAILED:
            return "failed";
        default:
            return "unknown";
    }
}

void DispensingWorkflowUseCase::ReleaseRetainedExecutionState(PlanRecord& plan_record) const {
    plan_record.execution_launch.execution_package.reset();
    plan_record.execution_assembly = {};
    plan_record.preview_authority_shared_with_execution = false;
    plan_record.execution_authority_shared_with_execution = false;
    plan_record.execution_binding_ready = false;
}

void DispensingWorkflowUseCase::EraseExecutionAssemblyCacheEntry(const std::string& execution_cache_key) const {
    if (execution_cache_key.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(execution_assembly_cache_mutex_);
    execution_assembly_cache_.erase(execution_cache_key);
}

void DispensingWorkflowUseCase::ReleaseConfirmedPreviewForPlan(
    const PlanID& plan_id,
    const JobID* runtime_job_id) const {
    if (plan_id.empty()) {
        return;
    }

    std::string execution_cache_key;
    bool release_execution_state = false;
    {
        std::lock_guard<std::mutex> lock(plans_mutex_);
        auto plan_it = plans_.find(plan_id);
        if (plan_it == plans_.end() || !plan_it->second.latest) {
            return;
        }

        if (runtime_job_id != nullptr &&
            !plan_it->second.runtime_job_id.empty() &&
            plan_it->second.runtime_job_id != *runtime_job_id) {
            return;
        }

        if (plan_it->second.preview_state == PlanPreviewState::CONFIRMED) {
            plan_it->second.preview_state = PlanPreviewState::SNAPSHOT_READY;
        }
        plan_it->second.confirmed_at.clear();
        if (runtime_job_id != nullptr && plan_it->second.runtime_job_id == *runtime_job_id) {
            plan_it->second.runtime_job_id.clear();
            execution_cache_key = plan_it->second.response.plan_fingerprint;
            ReleaseRetainedExecutionState(plan_it->second);
            release_execution_state = true;
        }
    }

    if (release_execution_state) {
        // execution_package/execution_assembly are rebuildable from authority preview;
        // dropping them here prevents terminal jobs from pinning large execution buffers.
        EraseExecutionAssemblyCacheEntry(execution_cache_key);
    }
}

}  // namespace Siligen::Application::UseCases::Dispensing


