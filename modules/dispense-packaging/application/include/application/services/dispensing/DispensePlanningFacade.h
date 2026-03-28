#pragma once

#include "application/services/dispensing/PlanningArtifactExportPort.h"
#include "domain/dispensing/contracts/ExecutionPackage.h"
#include "domain/dispensing/value-objects/DispenseCompensationProfile.h"
#include "domain/motion/domain-services/interpolation/TrajectoryInterpolatorBase.h"
#include "domain/motion/value-objects/MotionPlanningReport.h"
#include "motion_planning/contracts/MotionPlan.h"
#include "process_path/contracts/ProcessPath.h"
#include "shared/types/DispensingStrategy.h"
#include "shared/types/Result.h"
#include "shared/types/TrajectoryTypes.h"

#include <string>
#include <vector>

namespace Siligen::Application::Services::Dispensing {

struct PlanningArtifactsBuildInput {
    Siligen::ProcessPath::Contracts::ProcessPath process_path;
    Siligen::MotionPlanning::Contracts::MotionPlan motion_plan;
    std::string source_path;
    std::string dxf_filename;
    Siligen::Shared::Types::float32 dispensing_velocity = 0.0f;
    Siligen::Shared::Types::float32 acceleration = 0.0f;
    Siligen::Shared::Types::uint32 dispenser_interval_ms = 0;
    Siligen::Shared::Types::uint32 dispenser_duration_ms = 0;
    Siligen::Shared::Types::float32 trigger_spatial_interval_mm = 0.0f;
    Siligen::Shared::Types::float32 valve_response_ms = 0.0f;
    Siligen::Shared::Types::float32 safety_margin_ms = 0.0f;
    Siligen::Shared::Types::float32 min_interval_ms = 0.0f;
    Siligen::Shared::Types::float32 max_jerk = 0.0f;
    Siligen::Shared::Types::float32 sample_dt = 0.01f;
    Siligen::Shared::Types::float32 sample_ds = 0.0f;
    Siligen::Shared::Types::float32 spline_max_step_mm = 0.0f;
    Siligen::Shared::Types::float32 spline_max_error_mm = 0.0f;
    Siligen::Shared::Types::float32 estimated_time_s = 0.0f;
    Siligen::Shared::Types::DispensingStrategy dispensing_strategy =
        Siligen::Shared::Types::DispensingStrategy::BASELINE;
    int subsegment_count = 8;
    bool dispense_only_cruise = false;
    bool downgrade_on_violation = true;
    bool use_interpolation_planner = false;
    Siligen::Domain::Motion::InterpolationAlgorithm interpolation_algorithm =
        Siligen::Domain::Motion::InterpolationAlgorithm::LINEAR;
    Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile compensation_profile{};
    Siligen::Shared::Types::float32 spacing_tol_ratio = 0.0f;
    Siligen::Shared::Types::float32 spacing_min_mm = 0.0f;
    Siligen::Shared::Types::float32 spacing_max_mm = 0.0f;
};

struct PlanningArtifactsBuildResult {
    Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated execution_package;
    int segment_count = 0;
    Siligen::Shared::Types::float32 total_length = 0.0f;
    Siligen::Shared::Types::float32 estimated_time = 0.0f;
    std::vector<Siligen::TrajectoryPoint> trajectory_points;
    std::vector<Siligen::Shared::Types::Point2D> glue_points;
    int trigger_count = 0;
    std::string dxf_filename;
    Siligen::Shared::Types::int32 timestamp = 0;
    Siligen::Domain::Motion::ValueObjects::MotionPlanningReport planning_report;
    PlanningArtifactExportRequest export_request;
};

class DispensePlanningFacade {
   public:
    Siligen::Shared::Types::Result<PlanningArtifactsBuildResult> AssemblePlanningArtifacts(
        const PlanningArtifactsBuildInput& input) const;
};

}  // namespace Siligen::Application::Services::Dispensing
