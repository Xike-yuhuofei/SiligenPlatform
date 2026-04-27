#pragma once

#include "engineering/contracts/DxfValidationReport.h"
#include "motion_planning/contracts/MotionPlan.h"
#include "motion_planning/contracts/TimePlanningConfig.h"
#include "process_path/contracts/PathGenerationRequest.h"
#include "process_path/contracts/PathGenerationResult.h"
#include "process_path/contracts/ProcessPath.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>

namespace Siligen::Application::Ports::Dispensing {

using MotionPlan = Siligen::MotionPlanning::Contracts::MotionPlan;
using MotionPlanningConfig = Siligen::MotionPlanning::Contracts::TimePlanningConfig;
using Siligen::Engineering::Contracts::DxfValidationReport;
using ProcessPathBuildRequest = Siligen::ProcessPath::Contracts::PathGenerationRequest;
using ProcessPathBuildResult = Siligen::ProcessPath::Contracts::PathGenerationResult;
using Siligen::Shared::Types::Result;

struct PlanningInputPreparationRequest {
    std::string source_path;
    std::string source_ref;
    std::string source_hash;
};

struct PreparedPlanningInput {
    std::string prepared_path;
    std::string canonical_geometry_ref;
    DxfValidationReport validation_report;
};

class IProcessPathBuildPort {
public:
    virtual ~IProcessPathBuildPort() = default;

    virtual ProcessPathBuildResult Build(const ProcessPathBuildRequest& request) const = 0;
};

class IMotionPlanningPort {
public:
    virtual ~IMotionPlanningPort() = default;

    virtual MotionPlan Plan(
        const Siligen::ProcessPath::Contracts::ProcessPath& path,
        const MotionPlanningConfig& config) const = 0;
};

class IPlanningInputPreparationPort {
public:
    virtual ~IPlanningInputPreparationPort() = default;

    virtual Result<PreparedPlanningInput> EnsurePreparedInput(const PlanningInputPreparationRequest& request) const = 0;
};

}  // namespace Siligen::Application::Ports::Dispensing
