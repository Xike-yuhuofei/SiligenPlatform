#pragma once

#include "domain/dispensing/contracts/ExecutionPackage.h"
#include "domain/dispensing/value-objects/DispensingExecutionTypes.h"
#include "domain/safety/domain-services/SafetyOutputGuard.h"
#include "shared/types/Result.h"

#include <memory>
#include <optional>
#include <string>

namespace Siligen::Application::Ports::Dispensing {

using Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeOverrides;
using Siligen::Domain::Dispensing::ValueObjects::JobExecutionMode;
using Siligen::Domain::Dispensing::ValueObjects::ProcessOutputPolicy;
using Siligen::Domain::Machine::ValueObjects::MachineMode;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::uint32;

using WorkflowJobId = std::string;

struct WorkflowExecutionRequest {
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

    [[nodiscard]] MachineMode ResolveMachineMode() const noexcept {
        if (machine_mode.has_value()) {
            return machine_mode.value();
        }
        return dry_run ? MachineMode::Test : MachineMode::Production;
    }

    [[nodiscard]] JobExecutionMode ResolveExecutionMode() const noexcept {
        if (execution_mode.has_value()) {
            return execution_mode.value();
        }
        return dry_run ? JobExecutionMode::ValidationDryCycle : JobExecutionMode::Production;
    }

    [[nodiscard]] ProcessOutputPolicy ResolveOutputPolicy() const noexcept {
        if (output_policy.has_value()) {
            return output_policy.value();
        }
        return dry_run ? ProcessOutputPolicy::Inhibited : ProcessOutputPolicy::Enabled;
    }

    [[nodiscard]] Result<void> Validate() const noexcept {
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

        DispensingRuntimeOverrides overrides;
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

        auto override_validation = overrides.Validate();
        if (override_validation.IsError()) {
            return Result<void>::Failure(override_validation.GetError());
        }

        auto guard_validation = Siligen::Domain::Safety::DomainServices::SafetyOutputGuard::Evaluate(
            ResolveMachineMode(),
            ResolveExecutionMode(),
            ResolveOutputPolicy());
        if (guard_validation.IsError()) {
            return Result<void>::Failure(guard_validation.GetError());
        }

        return Result<void>::Success();
    }
};

struct WorkflowRuntimeStartJobRequest {
    std::string plan_id;
    WorkflowExecutionRequest execution_request;
    std::string plan_fingerprint;
    uint32 target_count = 1;
};

class IWorkflowExecutionPort {
public:
    virtual ~IWorkflowExecutionPort() = default;

    virtual Result<WorkflowJobId> StartJob(const WorkflowRuntimeStartJobRequest& request) = 0;
};

}  // namespace Siligen::Application::Ports::Dispensing
