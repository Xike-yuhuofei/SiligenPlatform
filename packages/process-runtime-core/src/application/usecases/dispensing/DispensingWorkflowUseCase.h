#pragma once

#include "application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "application/usecases/dispensing/PlanningUseCase.h"
#include "application/usecases/dispensing/UploadFileUseCase.h"
#include "domain/machine/ports/IHardwareConnectionPort.h"
#include "domain/motion/ports/IHomingPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "domain/safety/ports/IInterlockSignalPort.h"
#include "shared/types/Result.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Siligen::Application::UseCases::Dispensing {

using ArtifactID = std::string;
using PlanID = std::string;
using JobID = std::string;

struct CreateArtifactResponse {
    bool success = false;
    ArtifactID artifact_id;
    std::string filepath;
    std::string original_name;
    std::string generated_filename;
    std::size_t size = 0;
    int64_t timestamp = 0;
};

struct PreparePlanRequest {
    ArtifactID artifact_id;
    DispensingMVPRequest execution_request;
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
    std::string generated_at;
};

struct StartJobRequest {
    PlanID plan_id;
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
    std::string active_task_id;
    bool dry_run = false;
};

class DispensingWorkflowUseCase {
   public:
    enum class WorkflowJobState {
        PENDING,
        RUNNING,
        PAUSED,
        COMPLETED,
        FAILED,
        CANCELLED
    };

    DispensingWorkflowUseCase(
        std::shared_ptr<UploadFileUseCase> upload_use_case,
        std::shared_ptr<PlanningUseCase> planning_use_case,
        std::shared_ptr<DispensingExecutionUseCase> execution_use_case,
        std::shared_ptr<Domain::Machine::Ports::IHardwareConnectionPort> connection_port,
        std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port,
        std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port = nullptr,
        std::shared_ptr<Domain::Safety::Ports::IInterlockSignalPort> interlock_signal_port = nullptr);

    ~DispensingWorkflowUseCase() = default;

    DispensingWorkflowUseCase(const DispensingWorkflowUseCase&) = delete;
    DispensingWorkflowUseCase& operator=(const DispensingWorkflowUseCase&) = delete;

    Result<CreateArtifactResponse> CreateArtifact(const UploadRequest& request);
    Result<PreparePlanResponse> PreparePlan(const PreparePlanRequest& request);
    Result<JobID> StartJob(const StartJobRequest& request);
    Result<JobStatusResponse> GetJobStatus(const JobID& job_id) const;
    Result<void> PauseJob(const JobID& job_id);
    Result<void> ResumeJob(const JobID& job_id);
    Result<void> StopJob(const JobID& job_id);
    Result<Domain::Safety::ValueObjects::InterlockSignals> ReadInterlockSignals() const;

   private:
    struct ArtifactRecord {
        CreateArtifactResponse response;
        UploadResponse upload_response;
    };

    struct PlanRecord {
        PreparePlanResponse response;
        DispensingMVPRequest execution_request;
    };

    struct JobContext {
        JobID job_id;
        PlanID plan_id;
        std::string plan_fingerprint;
        DispensingMVPRequest execution_request;
        std::atomic<WorkflowJobState> state{WorkflowJobState::PENDING};
        std::atomic<std::uint32_t> target_count{0};
        std::atomic<std::uint32_t> completed_count{0};
        std::atomic<std::uint32_t> current_cycle{0};
        std::atomic<std::uint32_t> current_segment{0};
        std::atomic<std::uint32_t> total_segments{0};
        std::atomic<std::uint32_t> cycle_progress_percent{0};
        std::atomic<bool> stop_requested{false};
        std::atomic<bool> pause_requested{false};
        bool dry_run = false;
        std::chrono::steady_clock::time_point start_time{};
        std::chrono::steady_clock::time_point end_time{};
        mutable std::mutex mutex_;
        std::string active_task_id;
        std::string error_message;
    };

    std::shared_ptr<UploadFileUseCase> upload_use_case_;
    std::shared_ptr<PlanningUseCase> planning_use_case_;
    std::shared_ptr<DispensingExecutionUseCase> execution_use_case_;
    std::shared_ptr<Domain::Machine::Ports::IHardwareConnectionPort> connection_port_;
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port_;
    std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port_;
    std::shared_ptr<Domain::Safety::Ports::IInterlockSignalPort> interlock_signal_port_;

    mutable std::mutex artifacts_mutex_;
    std::unordered_map<ArtifactID, ArtifactRecord> artifacts_;

    mutable std::mutex plans_mutex_;
    std::unordered_map<PlanID, PlanRecord> plans_;

    mutable std::mutex jobs_mutex_;
    std::unordered_map<JobID, std::shared_ptr<JobContext>> jobs_;
    JobID active_job_id_;

    std::atomic<std::uint64_t> id_sequence_{0};

    Result<void> ValidateExecutionPreconditions() const;
    PlanningRequest BuildPlanningRequest(
        const std::string& filepath,
        const DispensingMVPRequest& request) const;
    JobStatusResponse BuildJobStatusResponse(const std::shared_ptr<JobContext>& context) const;
    std::string GenerateId(const char* prefix);
    std::string BuildPlanFingerprint(
        const ArtifactID& artifact_id,
        const PlanningResponse& planning,
        const DispensingMVPRequest& execution_request) const;
    std::string JobStateToString(WorkflowJobState state) const;
    void RunJob(
        const std::shared_ptr<JobContext>& context,
        const PlanRecord& plan_record);
    void FinalizeJob(
        const std::shared_ptr<JobContext>& context,
        WorkflowJobState final_state,
        const std::string& error_message = std::string());
};

}  // namespace Siligen::Application::UseCases::Dispensing
