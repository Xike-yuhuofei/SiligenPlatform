#pragma once

#include "domain/dispensing/contracts/ExecutionPackage.h"
#include "runtime_execution/contracts/configuration/IConfigurationPort.h"
#include "runtime_execution/contracts/dispensing/ITaskSchedulerPort.h"
#include "runtime_execution/contracts/dispensing/IValvePort.h"
#include "runtime_execution/contracts/dispensing/QualityMetrics.h"
#include "runtime_execution/contracts/motion/IHomingPort.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "runtime_execution/contracts/motion/IMotionStatePort.h"
#include "runtime_execution/contracts/safety/IInterlockSignalPort.h"
#include "runtime_execution/contracts/system/IEventPublisherPort.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "siligen/device/contracts/ports/device_ports.h"

#include <memory>
#include <optional>
#include <string>

namespace Siligen::Application::UseCases::Dispensing {

using Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated;
using Siligen::Domain::Dispensing::ValueObjects::JobExecutionMode;
using Siligen::Domain::Dispensing::ValueObjects::ProcessOutputPolicy;
using Siligen::Domain::Machine::ValueObjects::MachineMode;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using RuntimeEventPublisherPort = Siligen::RuntimeExecution::Contracts::System::IEventPublisherPort;
using RuntimeTaskSchedulerPort = Siligen::RuntimeExecution::Contracts::Dispensing::ITaskSchedulerPort;
using RuntimeHomingPort = Siligen::Domain::Motion::Ports::IHomingPort;
using RuntimeInterlockSignalPort = Siligen::Domain::Safety::Ports::IInterlockSignalPort;

struct DispensingExecutionRequest {
    ExecutionPackageValidated execution_package;
    std::string source_path;

    bool use_hardware_trigger = true;
    bool dry_run = false;
    std::optional<MachineMode> machine_mode;
    std::optional<JobExecutionMode> execution_mode;
    std::optional<ProcessOutputPolicy> output_policy;
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

    Shared::Types::Result<void> Validate() const noexcept;
    MachineMode ResolveMachineMode() const noexcept;
    JobExecutionMode ResolveExecutionMode() const noexcept;
    ProcessOutputPolicy ResolveOutputPolicy() const noexcept;
};

struct DispensingExecutionResult {
    bool success = false;
    std::string message;

    uint32 total_segments = 0;
    uint32 executed_segments = 0;
    uint32 failed_segments = 0;
    float32 total_distance = 0.0f;
    float32 execution_time_seconds = 0.0f;

    std::string error_details;

    Domain::Dispensing::ValueObjects::QualityMetrics quality_metrics;
    bool quality_metrics_available = false;
};

using JobID = std::string;

struct RuntimeStartJobRequest {
    std::string plan_id;
    DispensingExecutionRequest execution_request;
    std::string plan_fingerprint;
    uint32 target_count = 1;
};

struct RuntimeJobStatusResponse {
    JobID job_id;
    std::string plan_id;
    std::string plan_fingerprint;
    std::string state;
    uint32 target_count = 0;
    uint32 completed_count = 0;
    uint32 current_cycle = 0;
    uint32 current_segment = 0;
    uint32 total_segments = 0;
    uint32 cycle_progress_percent = 0;
    uint32 overall_progress_percent = 0;
    float32 elapsed_seconds = 0.0f;
    std::string error_message;
    bool dry_run = false;
};

class DispensingExecutionUseCase {
   public:
    explicit DispensingExecutionUseCase(
        std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port,
        std::shared_ptr<Domain::Motion::Ports::IInterpolationPort> interpolation_port,
        std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port,
        std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
        std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
        std::shared_ptr<RuntimeEventPublisherPort> event_port = nullptr,
        std::shared_ptr<RuntimeTaskSchedulerPort> task_scheduler_port = nullptr,
        std::shared_ptr<RuntimeHomingPort> homing_port = nullptr,
        std::shared_ptr<RuntimeInterlockSignalPort> interlock_signal_port = nullptr);

    ~DispensingExecutionUseCase();

    DispensingExecutionUseCase(const DispensingExecutionUseCase&) = delete;
    DispensingExecutionUseCase& operator=(const DispensingExecutionUseCase&) = delete;

    Shared::Types::Result<DispensingExecutionResult> Execute(const DispensingExecutionRequest& request);
    Shared::Types::Result<JobID> StartJob(const RuntimeStartJobRequest& request);
    Shared::Types::Result<RuntimeJobStatusResponse> GetJobStatus(const JobID& job_id) const;
    Shared::Types::Result<void> PauseJob(const JobID& job_id);
    Shared::Types::Result<void> ResumeJob(const JobID& job_id);
    Shared::Types::Result<void> StopJob(const JobID& job_id);

#ifdef SILIGEN_TEST_HOOKS
    void SeedJobStateForTesting(const RuntimeJobStatusResponse& status, bool pause_requested = false);
    void SetActiveJobForTesting(const JobID& job_id);
#endif

   private:
#ifdef SILIGEN_INTERNAL_RUNTIME_EXECUTION_TESTS
   public:
#endif
    struct Impl;
#ifdef SILIGEN_INTERNAL_RUNTIME_EXECUTION_TESTS
   private:
#endif
    std::unique_ptr<Impl> impl_;
};

}  // namespace Siligen::Application::UseCases::Dispensing
