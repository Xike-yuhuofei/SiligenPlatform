#pragma once

#include "domain/dispensing/value-objects/DispensingExecutionPlan.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <string>
#include <utility>

namespace Siligen::Domain::Dispensing::Contracts {

using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionPlan;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;

struct ExecutionPackageBuilt {
    DispensingExecutionPlan execution_plan;
    float32 total_length_mm = 0.0f;
    float32 estimated_time_s = 0.0f;
    std::string source_path;
    std::string source_fingerprint;
};

struct ExecutionPackageValidated {
    DispensingExecutionPlan execution_plan;
    float32 total_length_mm = 0.0f;
    float32 estimated_time_s = 0.0f;
    std::string source_path;
    std::string source_fingerprint;

    ExecutionPackageValidated() = default;
    explicit ExecutionPackageValidated(const ExecutionPackageBuilt& built)
        : execution_plan(built.execution_plan),
          total_length_mm(built.total_length_mm),
          estimated_time_s(built.estimated_time_s),
          source_path(built.source_path),
          source_fingerprint(built.source_fingerprint) {}

    explicit ExecutionPackageValidated(ExecutionPackageBuilt&& built) noexcept
        : execution_plan(std::move(built.execution_plan)),
          total_length_mm(built.total_length_mm),
          estimated_time_s(built.estimated_time_s),
          source_path(std::move(built.source_path)),
          source_fingerprint(std::move(built.source_fingerprint)) {}

    [[nodiscard]] Result<void> Validate() const noexcept {
        const bool has_interpolation_points = execution_plan.interpolation_points.size() >= 2;
        const bool has_motion_points = execution_plan.motion_trajectory.points.size() >= 2;
        const bool has_interpolation_segments = !execution_plan.interpolation_segments.empty();

        if (!has_interpolation_segments && !has_interpolation_points && !has_motion_points) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "execution package missing executable trajectory"));
        }

        if (total_length_mm < 0.0f) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "execution package total_length_mm cannot be negative"));
        }

        if (estimated_time_s < 0.0f) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "execution package estimated_time_s cannot be negative"));
        }

        return Result<void>::Success();
    }
};

}  // namespace Siligen::Domain::Dispensing::Contracts
