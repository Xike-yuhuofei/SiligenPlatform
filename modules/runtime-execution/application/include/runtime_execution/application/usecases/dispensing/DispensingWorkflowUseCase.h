#pragma once

#include "runtime_execution/application/usecases/dispensing/WorkflowExecutionPort.h"
#include "dispense_packaging/application/usecases/dispensing/PlanningUseCase.h"
#include "application/services/dispensing/PreviewSnapshotService.h"
#include "job_ingest/contracts/dispensing/UploadContracts.h"
#include "runtime_execution/contracts/motion/IHomingPort.h"
#include "runtime_execution/contracts/motion/IMotionStatePort.h"
#include "runtime_execution/contracts/safety/IInterlockSignalPort.h"
#include "siligen/device/contracts/ports/device_ports.h"
#include "shared/types/Error.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Siligen::Application::UseCases::Dispensing {

using ArtifactID = std::string;
using JobID = std::string;
using PlanID = std::string;
using Siligen::JobIngest::Contracts::IUploadFilePort;
using Siligen::JobIngest::Contracts::UploadRequest;
using Siligen::JobIngest::Contracts::UploadResponse;

struct CreateArtifactResponse {
    bool success = false;
    ArtifactID artifact_id;
    std::string filepath;
    std::string original_name;
    std::string generated_filename;
    std::size_t size = 0;
    int64_t timestamp = 0;
};

struct PreparePlanRuntimeOverrides {
    std::string source_path;
    bool use_hardware_trigger = true;
    bool dry_run = false;
    std::optional<Domain::Machine::ValueObjects::MachineMode> machine_mode;
    std::optional<Domain::Dispensing::ValueObjects::JobExecutionMode> execution_mode;
    std::optional<Domain::Dispensing::ValueObjects::ProcessOutputPolicy> output_policy;
    float32 max_jerk = 0.0f;
    float32 arc_tolerance_mm = 0.0f;
    std::optional<float32> dispensing_speed_mm_s;
    std::optional<float32> dry_run_speed_mm_s;
    std::optional<float32> rapid_speed_mm_s;
    std::optional<float32> acceleration_mm_s2;
    bool velocity_trace_enabled = false;
    int32 velocity_trace_interval_ms = 0;
    std::string velocity_trace_path;
    bool velocity_guard_enabled = true;
    float32 velocity_guard_ratio = 0.3f;
    float32 velocity_guard_abs_mm_s = 5.0f;
    float32 velocity_guard_min_expected_mm_s = 5.0f;
    int32 velocity_guard_grace_ms = 800;
    int32 velocity_guard_interval_ms = 200;
    int32 velocity_guard_max_consecutive = 3;
    bool velocity_guard_stop_on_violation = false;
};

struct PreparePlanRequest {
    ArtifactID artifact_id;
    PlanningRequest planning_request;
    PreparePlanRuntimeOverrides runtime_overrides;
};

struct PreparePlanResponse {
    struct PerformanceProfile {
        bool authority_cache_hit = false;
        bool authority_joined_inflight = false;
        std::uint32_t authority_wait_ms = 0;
        std::uint32_t pb_prepare_ms = 0;
        std::uint32_t path_load_ms = 0;
        std::uint32_t process_path_ms = 0;
        std::uint32_t authority_build_ms = 0;
        std::uint32_t authority_total_ms = 0;
        std::uint32_t prepare_total_ms = 0;
    };

    bool success = false;
    ArtifactID artifact_id;
    PlanID plan_id;
    std::string plan_fingerprint;
    std::string filepath;
    std::uint32_t segment_count = 0;
    std::uint32_t point_count = 0;
    float32 total_length_mm = 0.0f;
    float32 estimated_time_s = 0.0f;
    std::string preview_validation_classification;
    std::string preview_exception_reason;
    std::string preview_failure_reason;
    std::string preview_diagnostic_code;
    std::string generated_at;
    PerformanceProfile performance_profile;
};

using PreviewSnapshotPoint = Services::Dispensing::PreviewSnapshotPoint;
using PreviewSnapshotRequest = Services::Dispensing::PreviewSnapshotRequest;
using PreviewSnapshotResponse = Services::Dispensing::PreviewSnapshotResponse;

struct ConfirmPreviewRequest {
    PlanID plan_id;
    std::string snapshot_hash;
};

struct ConfirmPreviewResponse {
    bool confirmed = false;
    PlanID plan_id;
    std::string snapshot_hash;
    std::string preview_state;
    std::string confirmed_at;
};

struct StartJobRequest {
    PlanID plan_id;
    std::string plan_fingerprint;
    std::uint32_t target_count = 1;
};

struct StartJobResponse {
    struct PerformanceProfile {
        bool execution_cache_hit = false;
        bool execution_joined_inflight = false;
        std::uint32_t execution_wait_ms = 0;
        std::uint32_t motion_plan_ms = 0;
        std::uint32_t assembly_ms = 0;
        std::uint32_t export_ms = 0;
        std::uint32_t execution_total_ms = 0;
    };

    bool started = false;
    JobID job_id;
    PlanID plan_id;
    std::string plan_fingerprint;
    std::uint32_t target_count = 0;
    PerformanceProfile performance_profile;
};

struct ExecutionAssemblyTestProbe {
    bool cache_hit = false;
    bool joined_inflight = false;
    std::uint32_t wait_ms = 0;
    std::string plan_id;
    std::string plan_fingerprint;
    std::string authority_layout_id;
    bool execution_authority_shared_with_execution = false;
    bool execution_binding_ready = false;
    bool has_execution_launch_package = false;
    bool has_execution_assembly_package = false;
    std::size_t plan_execution_trajectory_point_count = 0;
    std::size_t execution_launch_interpolation_segment_count = 0;
    std::size_t execution_launch_interpolation_point_count = 0;
    std::size_t execution_launch_motion_point_count = 0;
    std::size_t execution_assembly_trajectory_point_count = 0;
    std::size_t execution_assembly_interpolation_segment_count = 0;
    std::size_t execution_assembly_interpolation_point_count = 0;
    std::size_t execution_assembly_motion_point_count = 0;
    std::size_t execution_cache_entry_count = 0;
    bool execution_cache_contains_plan = false;
};

struct JobStatusResponse {
    JobID job_id;
    PlanID plan_id;
    std::string plan_fingerprint;
    std::string state;
    std::uint32_t target_count = 0;
    std::uint32_t completed_count = 0;
    std::uint32_t current_cycle = 0;
    std::uint32_t current_segment = 0;
    std::uint32_t total_segments = 0;
    std::uint32_t cycle_progress_percent = 0;
    std::uint32_t overall_progress_percent = 0;
    float32 elapsed_seconds = 0.0f;
    std::string error_message;
    bool dry_run = false;
};

class DispensingWorkflowUseCase {
   public:
    enum class PlanPreviewState {
        PREPARED,
        SNAPSHOT_READY,
        CONFIRMED,
        STALE,
        FAILED
    };

    DispensingWorkflowUseCase(
        std::shared_ptr<IUploadFilePort> upload_use_case,
        std::shared_ptr<PlanningUseCase> planning_use_case,
        std::shared_ptr<Siligen::Application::Ports::Dispensing::IWorkflowExecutionPort> execution_port,
        std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
        std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port,
        std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port = nullptr,
        std::shared_ptr<Domain::Safety::Ports::IInterlockSignalPort> interlock_signal_port = nullptr);

    ~DispensingWorkflowUseCase();

    DispensingWorkflowUseCase(const DispensingWorkflowUseCase&) = delete;
    DispensingWorkflowUseCase& operator=(const DispensingWorkflowUseCase&) = delete;

    Result<CreateArtifactResponse> CreateArtifact(const UploadRequest& request);
    Result<PreparePlanResponse> PreparePlan(const PreparePlanRequest& request);
    Result<PreviewSnapshotResponse> GetPreviewSnapshot(const PreviewSnapshotRequest& request);
    Result<ConfirmPreviewResponse> ConfirmPreview(const ConfirmPreviewRequest& request);
    Result<StartJobResponse> StartJob(const StartJobRequest& request);
    void OnRuntimeJobTerminal(const JobID& job_id, const PlanID& plan_id = {}) const;
    Result<Domain::Safety::ValueObjects::InterlockSignals> ReadInterlockSignals() const;
    bool IsInterlockLatched() const;
#ifdef SILIGEN_TEST_HOOKS
    Result<ExecutionAssemblyTestProbe> EnsureExecutionAssemblyReadyForTesting(const PlanID& plan_id) const;
#endif

   private:
    struct ArtifactRecord {
        CreateArtifactResponse response;
        UploadResponse upload_response;
    };

    struct PlanExecutionLaunch {
        PreparedAuthorityPreview authority_preview;
        PlanningRequest planning_request;
        std::string authority_cache_key;
        std::shared_ptr<Domain::Dispensing::Contracts::ExecutionPackageValidated> execution_package;
        PreparePlanRuntimeOverrides runtime_overrides;
    };

    struct PlanRecord {
        PreparePlanResponse response;
        PlanExecutionLaunch execution_launch;
        ExecutionAssemblyResponse execution_assembly;
        std::vector<TrajectoryPoint> execution_trajectory_points;
        std::vector<Siligen::Shared::Types::Point2D> glue_points;
        Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout authority_trigger_layout;
        bool preview_authority_ready = false;
        bool preview_authority_shared_with_execution = false;
        bool preview_binding_ready = false;
        bool execution_authority_shared_with_execution = false;
        bool execution_binding_ready = false;
        bool preview_spacing_valid = false;
        bool preview_has_short_segment_exceptions = false;
        std::string preview_validation_classification;
        std::string preview_exception_reason;
        std::string preview_failure_reason;
        std::string preview_diagnostic_code;
        PlanPreviewState preview_state = PlanPreviewState::PREPARED;
        std::string preview_snapshot_id;
        std::string preview_snapshot_hash;
        std::string preview_generated_at;
        std::string confirmed_at;
        std::string failure_message;
        JobID runtime_job_id;
        bool latest = true;
    };

    struct PreviewGateDiagnostic {
        std::string owner_message;
        std::string layout_id;
        bool authority_ready = false;
        bool authority_shared_with_execution = false;
        bool binding_ready = false;
        bool execution_authority_shared_with_execution = false;
        bool execution_binding_ready = false;
        bool spacing_valid = false;
        std::string validation_classification;
        std::string exception_reason;
        std::string failure_reason;
        std::size_t glue_point_count = 0;
    };

    struct AuthorityPreviewCacheEntry {
        PreparedAuthorityPreview preview;
    };

    struct ExecutionAssemblyCacheEntry {
        ExecutionAssemblyResponse assembly;
    };

    struct AuthorityPreviewInFlight {
        std::mutex mutex;
        std::condition_variable cv;
        bool ready = false;
        PreparedAuthorityPreview preview;
        std::optional<Siligen::Shared::Types::Error> error;
    };

    struct ExecutionAssemblyInFlight {
        std::mutex mutex;
        std::condition_variable cv;
        bool ready = false;
        ExecutionAssemblyResponse assembly;
        std::optional<Siligen::Shared::Types::Error> error;
    };

    struct AuthorityPreviewResolveResult {
        PreparedAuthorityPreview preview;
        bool cache_hit = false;
        bool joined_inflight = false;
        std::uint32_t wait_ms = 0;
    };

    struct ExecutionAssemblyResolveResult {
        ExecutionAssemblyResponse assembly;
        bool cache_hit = false;
        bool joined_inflight = false;
        std::uint32_t wait_ms = 0;
    };

    struct PreviewBindingResolution {
        bool require_execution_binding = false;
    };

    std::shared_ptr<IUploadFilePort> upload_use_case_;
    std::shared_ptr<PlanningUseCase> planning_use_case_;
    std::shared_ptr<Siligen::Application::Ports::Dispensing::IWorkflowExecutionPort> execution_port_;
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port_;
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port_;
    std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port_;
    std::shared_ptr<Domain::Safety::Ports::IInterlockSignalPort> interlock_signal_port_;

    mutable std::mutex artifacts_mutex_;
    std::unordered_map<ArtifactID, ArtifactRecord> artifacts_;

    mutable std::mutex plans_mutex_;
    mutable std::unordered_map<PlanID, PlanRecord> plans_;
    mutable std::mutex job_plan_index_mutex_;
    mutable std::unordered_map<JobID, PlanID> job_plan_index_;
    mutable std::mutex authority_preview_cache_mutex_;
    mutable std::unordered_map<std::string, AuthorityPreviewCacheEntry> authority_preview_cache_;
    mutable std::mutex authority_preview_inflight_mutex_;
    mutable std::unordered_map<std::string, std::shared_ptr<AuthorityPreviewInFlight>> authority_preview_inflight_;
    mutable std::mutex execution_assembly_cache_mutex_;
    mutable std::unordered_map<std::string, ExecutionAssemblyCacheEntry> execution_assembly_cache_;
    mutable std::mutex execution_assembly_inflight_mutex_;
    mutable std::unordered_map<std::string, std::shared_ptr<ExecutionAssemblyInFlight>> execution_assembly_inflight_;

    std::atomic<std::uint64_t> id_sequence_{0};

    PreviewSnapshotResponse BuildPreviewSnapshotResponse(
        const PlanRecord& plan_record,
        std::size_t max_polyline_points,
        std::size_t max_glue_points);
    std::string GenerateId(const char* prefix);
    Siligen::Application::Ports::Dispensing::WorkflowExecutionRequest BuildExecutionRequest(
        const PlanExecutionLaunch& launch) const;
    std::string BuildPlanFingerprint(
        const ArtifactID& artifact_id,
        const PreparedAuthorityPreview& authority_preview,
        const PlanExecutionLaunch& execution_launch) const;
    bool RequiresExecutionBinding(const PlanRecord& plan_record) const;
    bool ShouldResolveExecutionBinding(const PlanRecord& plan_record) const;
    Siligen::Shared::Types::Result<PreviewBindingResolution> ResolvePreviewBindingRequirement(
        const PlanID& plan_id,
        bool require_snapshot_ready,
        const std::string* snapshot_hash,
        bool mark_failed) const;
    Siligen::Shared::Types::Result<PlanRecord> PromotePlanToSnapshotReady(
        const PlanID& plan_id,
        bool require_execution_binding);
    Siligen::Shared::Types::Result<ConfirmPreviewResponse> ConfirmPreviewReadyPlan(
        const PlanID& plan_id,
        const std::string& snapshot_hash,
        bool require_execution_binding);
    Siligen::Shared::Types::Result<PlanExecutionLaunch> ResolveStartJobExecutionLaunch(
        const StartJobRequest& request,
        bool require_execution_binding) const;
    Siligen::Shared::Types::Result<AuthorityPreviewResolveResult> ResolveAuthorityPreview(
        const std::string& authority_cache_key,
        const PlanningRequest& planning_request) const;
    Siligen::Shared::Types::Result<ExecutionAssemblyResolveResult> ResolveExecutionAssembly(
        const std::string& execution_cache_key,
        const PlanExecutionLaunch& execution_launch) const;
    Siligen::Shared::Types::Result<ExecutionAssemblyResolveResult> EnsureExecutionAssemblyReady(
        const PlanID& plan_id) const;
    PreviewGateDiagnostic BuildPreviewGateDiagnostic(
        const PlanRecord& plan_record,
        bool require_execution_binding) const;
    std::optional<Siligen::Shared::Types::Error> BuildPreviewGateError(
        PlanRecord& plan_record,
        bool mark_failed,
        bool require_execution_binding) const;
    std::string ResolvePreviewGateFailure(const PlanRecord& plan_record) const;
    std::string PreviewStateToString(PlanPreviewState state) const;
    void ReleaseRetainedExecutionState(PlanRecord& plan_record) const;
    void EraseExecutionAssemblyCacheEntry(const std::string& execution_cache_key) const;
    void ReleaseConfirmedPreviewForPlan(const PlanID& plan_id, const JobID* runtime_job_id = nullptr) const;
};

}  // namespace Siligen::Application::UseCases::Dispensing
