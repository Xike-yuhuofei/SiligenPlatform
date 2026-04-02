#pragma once

#include "domain/dispensing/contracts/ExecutionPackage.h"
#include "shared/types/Result.h"

#include <cstdint>
#include <optional>
#include <string>

namespace Siligen::Workflow::Contracts {

using WorkflowJobId = std::string;

struct WorkflowExecutionRuntimeOverrides {
    std::string source_path;
    bool use_hardware_trigger = true;
    bool dry_run = false;
    std::optional<Siligen::Domain::Machine::ValueObjects::MachineMode> machine_mode;
    std::optional<Siligen::Domain::Dispensing::ValueObjects::JobExecutionMode> execution_mode;
    std::optional<Siligen::Domain::Dispensing::ValueObjects::ProcessOutputPolicy> output_policy;
    Siligen::Shared::Types::float32 max_jerk = 0.0f;
    Siligen::Shared::Types::float32 arc_tolerance_mm = 0.0f;
    std::optional<Siligen::Shared::Types::float32> dispensing_speed_mm_s;
    std::optional<Siligen::Shared::Types::float32> dry_run_speed_mm_s;
    std::optional<Siligen::Shared::Types::float32> rapid_speed_mm_s;
    std::optional<Siligen::Shared::Types::float32> acceleration_mm_s2;
    bool velocity_trace_enabled = false;
    Siligen::Shared::Types::int32 velocity_trace_interval_ms = 0;
    std::string velocity_trace_path;
    bool velocity_guard_enabled = true;
    Siligen::Shared::Types::float32 velocity_guard_ratio = 0.3f;
    Siligen::Shared::Types::float32 velocity_guard_abs_mm_s = 5.0f;
    Siligen::Shared::Types::float32 velocity_guard_min_expected_mm_s = 5.0f;
    Siligen::Shared::Types::int32 velocity_guard_grace_ms = 800;
    Siligen::Shared::Types::int32 velocity_guard_interval_ms = 200;
    Siligen::Shared::Types::int32 velocity_guard_max_consecutive = 3;
    bool velocity_guard_stop_on_violation = false;
};

struct WorkflowExecutionLaunch {
    Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated execution_package;
    WorkflowExecutionRuntimeOverrides runtime_overrides;
};

struct WorkflowExecutionStartRequest {
    std::string plan_id;
    WorkflowExecutionLaunch launch;
    std::string plan_fingerprint;
    std::uint32_t target_count = 1;
};

struct WorkflowExecutionStatus {
    WorkflowJobId job_id;
    std::string plan_id;
    std::string plan_fingerprint;
    std::string state;
    std::uint32_t target_count = 0;
    std::uint32_t completed_count = 0;
    std::uint32_t current_cycle = 0;
    std::uint32_t current_segment = 0;
    std::uint32_t total_segments = 0;
    std::uint32_t cycle_progress_percent = 0;
    std::uint32_t overall_progress_percent = 0;
    Siligen::Shared::Types::float32 elapsed_seconds = 0.0f;
    std::string error_message;
    bool dry_run = false;
};

class IWorkflowExecutionPort {
public:
    virtual ~IWorkflowExecutionPort() = default;

    virtual Siligen::Shared::Types::Result<WorkflowJobId> StartWorkflowExecution(
        const WorkflowExecutionStartRequest& request) = 0;

    virtual Siligen::Shared::Types::Result<WorkflowExecutionStatus> GetWorkflowExecutionStatus(
        const WorkflowJobId& job_id) const = 0;

    virtual Siligen::Shared::Types::Result<void> PauseWorkflowExecution(const WorkflowJobId& job_id) = 0;
    virtual Siligen::Shared::Types::Result<void> ResumeWorkflowExecution(const WorkflowJobId& job_id) = 0;
    virtual Siligen::Shared::Types::Result<void> StopWorkflowExecution(const WorkflowJobId& job_id) = 0;
};

}  // namespace Siligen::Workflow::Contracts
