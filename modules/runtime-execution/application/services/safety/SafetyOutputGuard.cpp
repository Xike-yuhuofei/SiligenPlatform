#include "domain/safety/domain-services/SafetyOutputGuard.h"

#include "shared/types/Error.h"

namespace Siligen::Domain::Safety::DomainServices {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

Result<GuardDecision> SafetyOutputGuard::Evaluate(MachineMode machine_mode,
                                                  JobExecutionMode execution_mode,
                                                  ProcessOutputPolicy output_policy) noexcept {
    GuardDecision decision;
    decision.machine_mode = machine_mode;
    decision.execution_mode = execution_mode;
    decision.output_policy = output_policy;
    decision.allow_motion = true;
    decision.allow_valve = (output_policy == ProcessOutputPolicy::Enabled);
    decision.allow_supply = decision.allow_valve;
    decision.allow_cmp = decision.allow_valve;

    if (execution_mode == JobExecutionMode::ValidationDryCycle &&
        machine_mode != MachineMode::Test) {
        return Result<GuardDecision>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "ValidationDryCycle要求机台处于Test模式",
                  "SafetyOutputGuard"));
    }

    if (output_policy == ProcessOutputPolicy::Enabled &&
        machine_mode != MachineMode::Production) {
        return Result<GuardDecision>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "启用工艺输出要求机台处于Production模式",
                  "SafetyOutputGuard"));
    }

    if (output_policy == ProcessOutputPolicy::Enabled &&
        execution_mode != JobExecutionMode::Production) {
        return Result<GuardDecision>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "ValidationDryCycle禁止启用真实工艺输出",
                  "SafetyOutputGuard"));
    }

    return Result<GuardDecision>::Success(decision);
}

}  // namespace Siligen::Domain::Safety::DomainServices
