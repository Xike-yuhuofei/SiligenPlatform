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
using ProcessPathBuildRequest = Siligen::ProcessPath::Contracts::PathGenerationRequest;
using ProcessPathBuildResult = Siligen::ProcessPath::Contracts::PathGenerationResult;
using Siligen::Shared::Types::Result;

struct PlanningInputPreparationRequest {
    std::string source_path;
    std::string source_ref;
    std::string source_hash;
};

struct PlanningInputQualityProjection {
    std::string report_id;
    std::string report_path;
    std::string schema_version;
    std::string dxf_hash;
    std::string source_drawing_ref;
    std::string gate_result;
    std::string classification;
    bool preview_ready = false;
    bool production_ready = false;
    std::string summary;
    std::string primary_code;
    std::vector<std::string> warning_codes;
    std::vector<std::string> error_codes;
    std::string resolved_units;
    double resolved_unit_scale = 1.0;
};

struct PreparedPlanningInput {
    std::string prepared_path;
    PlanningInputQualityProjection input_quality_projection;
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
