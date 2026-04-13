#pragma once

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
using PreparedPlanningInputPath = std::string;
using ProcessPathBuildRequest = Siligen::ProcessPath::Contracts::PathGenerationRequest;
using ProcessPathBuildResult = Siligen::ProcessPath::Contracts::PathGenerationResult;
using Siligen::Shared::Types::Result;

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

    virtual Result<PreparedPlanningInputPath> EnsurePreparedInput(const std::string& source_path) const = 0;
};

}  // namespace Siligen::Application::Ports::Dispensing
