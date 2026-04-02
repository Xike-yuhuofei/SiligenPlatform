#pragma once

#include "application/usecases/dispensing/PlanningUseCase.h"
#include "job_ingest/contracts/dispensing/UploadContracts.h"
#include "domain/motion/ports/IHomingPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "domain/safety/ports/IInterlockSignalPort.h"
#include "siligen/device/contracts/ports/device_ports.h"
#include "shared/types/Error.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "workflow/contracts/WorkflowExecutionPort.h"

#include <atomic>
#include <chrono>
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
using PlanID = std::string;
using JobID = Siligen::Workflow::Contracts::WorkflowJobId;

struct CreateArtifactResponse {
    bool success = false;
    ArtifactID artifact_id;
    std::string filepath;
    std::string original_name;
    std::string generated_filename;
    std::size_t size = 0;
    int64_t timestamp = 0;
};

using PreparePlanRuntimeOverrides = Siligen::Workflow::Contracts::WorkflowExecutionRuntimeOverrides;

struct PreparePlanRequest {
    ArtifactID artifact_id;
    PlanningRequest planning_request;
    PreparePlanRuntimeOverrides runtime_overrides;
};

struct PreparePlanResponse {
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
    std::string generated_at;
};

struct PreviewSnapshotRequest {
    PlanID plan_id;
    std::size_t max_polyline_points = 4000;
};

struct PreviewSnapshotPoint {
    float32 x = 0.0f;
    float32 y = 0.0f;
};

struct PreviewSnapshotResponse {
    std::string snapshot_id;
    std::string snapshot_hash;
    PlanID plan_id;
    std::string preview_state;
    std::string preview_source;
    std::string preview_kind;
    std::string confirmed_at;
    std::uint32_t segment_count = 0;
    std::uint32_t point_count = 0;
    std::uint32_t glue_point_count = 0;
    std::uint32_t execution_point_count = 0;
    std::uint32_t execution_polyline_source_point_count = 0;
    std::uint32_t execution_polyline_point_count = 0;
    std::vector<PreviewSnapshotPoint> glue_points;
    std::vector<PreviewSnapshotPoint> execution_polyline;
    float32 total_length_mm = 0.0f;
    float32 estimated_time_s = 0.0f;
    std::string generated_at;
};

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
        std::shared_ptr<Siligen::Workflow::Contracts::IWorkflowExecutionPort> execution_port,
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
    Result<JobID> StartJob(const StartJobRequest& request);
    Result<JobStatusResponse> GetJobStatus(const JobID& job_id) const;
    Result<void> PauseJob(const JobID& job_id);
    Result<void> ResumeJob(const JobID& job_id);
    Result<void> StopJob(const JobID& job_id);
    Result<Domain::Safety::ValueObjects::InterlockSignals> ReadInterlockSignals() const;
    bool IsInterlockLatched() const;

   private:
    using PlanExecutionLaunch = Siligen::Workflow::Contracts::WorkflowExecutionLaunch;

    struct ArtifactRecord {
        CreateArtifactResponse response;
        UploadResponse upload_response;
    };

    struct PlanRecord {
        PreparePlanResponse response;
        PlanExecutionLaunch execution_launch;
        std::vector<TrajectoryPoint> execution_trajectory_points;
        std::vector<Siligen::Shared::Types::Point2D> glue_points;
        Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout authority_trigger_layout;
        bool preview_authority_ready = false;
        bool preview_authority_shared_with_execution = false;
        bool preview_binding_ready = false;
        bool preview_spacing_valid = false;
        bool preview_has_short_segment_exceptions = false;
        std::string preview_validation_classification;
        std::string preview_exception_reason;
        std::string preview_failure_reason;
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
        bool spacing_valid = false;
        std::string validation_classification;
        std::string exception_reason;
        std::string failure_reason;
        std::size_t glue_point_count = 0;
    };

    std::shared_ptr<IUploadFilePort> upload_use_case_;
    std::shared_ptr<PlanningUseCase> planning_use_case_;
    std::shared_ptr<Siligen::Workflow::Contracts::IWorkflowExecutionPort> execution_port_;
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

    std::atomic<std::uint64_t> id_sequence_{0};

    PreviewSnapshotResponse BuildPreviewSnapshotResponse(const PlanRecord& plan_record, std::size_t max_polyline_points);
    std::string GenerateId(const char* prefix);
    Siligen::Workflow::Contracts::WorkflowExecutionStartRequest BuildExecutionStartRequest(
        const PlanID& plan_id,
        const std::string& plan_fingerprint,
        std::uint32_t target_count,
        const PlanExecutionLaunch& launch) const;
    std::string BuildPlanFingerprint(
        const ArtifactID& artifact_id,
        const PlanningResponse& planning,
        const PlanExecutionLaunch& execution_launch) const;
    PreviewGateDiagnostic BuildPreviewGateDiagnostic(const PlanRecord& plan_record) const;
    std::optional<Siligen::Shared::Types::Error> BuildPreviewGateError(
        PlanRecord& plan_record,
        bool mark_failed) const;
    std::string ResolvePreviewGateFailure(const PlanRecord& plan_record) const;
    std::string PreviewStateToString(PlanPreviewState state) const;
    void ReleaseConfirmedPreviewForPlan(const PlanID& plan_id, const JobID* runtime_job_id = nullptr) const;
    void SyncPlanStateFromRuntimeStatus(
        const JobID& job_id,
        const Siligen::Workflow::Contracts::WorkflowExecutionStatus& runtime_status) const;
};

}  // namespace Siligen::Application::UseCases::Dispensing

