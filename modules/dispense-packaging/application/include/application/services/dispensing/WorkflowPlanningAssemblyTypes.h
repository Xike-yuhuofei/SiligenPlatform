#pragma once

#include "domain/dispensing/contracts/PlanningArtifactExportRequest.h"
#include "domain/dispensing/contracts/ExecutionPackage.h"
#include "domain/dispensing/value-objects/AuthorityTriggerLayout.h"
#include "domain/dispensing/value-objects/DispenseCompensationProfile.h"
#include "motion_planning/contracts/MotionPlan.h"
#include "motion_planning/contracts/MotionPlanningReport.h"
#include "motion_planning/contracts/InterpolationTypes.h"
#include "process_path/contracts/ProcessPath.h"
#include "shared/types/DispensingStrategy.h"
#include "shared/types/TrajectoryTypes.h"

#include <cstddef>
#include <string>
#include <vector>

namespace Siligen::Application::Services::Dispensing {

struct WorkflowAuthorityTriggerPoint {
    Siligen::Shared::Types::Point2D position;
    Siligen::Shared::Types::float32 trigger_distance_mm = 0.0f;
    std::size_t segment_index = 0;
    bool short_segment_exception = false;
    bool shared_vertex = false;
};

struct WorkflowSpacingValidationGroup {
    std::size_t segment_index = 0;
    std::vector<Siligen::Shared::Types::Point2D> points;
    Siligen::Shared::Types::float32 actual_spacing_mm = 0.0f;
    bool short_segment_exception = false;
    bool within_window = false;
};

struct WorkflowAssemblyRuntimeOptions {
    Siligen::Shared::Types::float32 dispensing_velocity = 0.0f;
    Siligen::Shared::Types::float32 acceleration = 0.0f;
    Siligen::Shared::Types::uint32 dispenser_interval_ms = 0;
    Siligen::Shared::Types::uint32 dispenser_duration_ms = 0;
    Siligen::Shared::Types::float32 trigger_spatial_interval_mm = 0.0f;
    Siligen::Shared::Types::float32 valve_response_ms = 0.0f;
    Siligen::Shared::Types::float32 safety_margin_ms = 0.0f;
    Siligen::Shared::Types::float32 min_interval_ms = 0.0f;
    Siligen::Shared::Types::float32 sample_dt = 0.01f;
    Siligen::Shared::Types::float32 sample_ds = 0.0f;
    Siligen::Shared::Types::float32 spline_max_step_mm = 0.0f;
    Siligen::Shared::Types::float32 spline_max_error_mm = 0.0f;
    Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile compensation_profile{};
};

struct WorkflowAuthorityPreviewRequest {
    Siligen::ProcessPath::Contracts::ProcessPath process_path;
    Siligen::ProcessPath::Contracts::ProcessPath authority_process_path;
    std::string source_path;
    std::string dxf_filename;
    WorkflowAssemblyRuntimeOptions runtime_options;
    Siligen::Shared::Types::DispensingStrategy dispensing_strategy =
        Siligen::Shared::Types::DispensingStrategy::BASELINE;
    int subsegment_count = 8;
    bool dispense_only_cruise = false;
    bool downgrade_on_violation = true;
    Siligen::Shared::Types::float32 spacing_tol_ratio = 0.0f;
    Siligen::Shared::Types::float32 spacing_min_mm = 0.0f;
    Siligen::Shared::Types::float32 spacing_max_mm = 0.0f;
};

struct WorkflowAuthorityPreviewArtifacts {
    int segment_count = 0;
    Siligen::Shared::Types::float32 total_length = 0.0f;
    Siligen::Shared::Types::float32 estimated_time = 0.0f;
    std::vector<Siligen::TrajectoryPoint> preview_trajectory_points;
    std::vector<Siligen::Shared::Types::Point2D> glue_points;
    int trigger_count = 0;
    std::string dxf_filename;
    Siligen::Shared::Types::int32 timestamp = 0;
    bool preview_authority_ready = false;
    bool preview_binding_ready = false;
    bool preview_spacing_valid = false;
    bool preview_has_short_segment_exceptions = false;
    std::string preview_validation_classification;
    std::string preview_exception_reason;
    std::string preview_failure_reason;
    Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout authority_trigger_layout;
    std::vector<WorkflowAuthorityTriggerPoint> authority_trigger_points;
    std::vector<WorkflowSpacingValidationGroup> spacing_validation_groups;
};

struct WorkflowExecutionAssemblyRequest {
    Siligen::ProcessPath::Contracts::ProcessPath process_path;
    Siligen::ProcessPath::Contracts::ProcessPath authority_process_path;
    Siligen::MotionPlanning::Contracts::MotionPlan motion_plan;
    std::string source_path;
    std::string dxf_filename;
    WorkflowAssemblyRuntimeOptions runtime_options;
    Siligen::Shared::Types::float32 max_jerk = 0.0f;
    Siligen::Shared::Types::float32 estimated_time_s = 0.0f;
    bool use_interpolation_planner = false;
    Siligen::MotionPlanning::Contracts::InterpolationAlgorithm interpolation_algorithm =
        Siligen::MotionPlanning::Contracts::InterpolationAlgorithm::LINEAR;
    WorkflowAuthorityPreviewArtifacts authority_preview;
};

struct WorkflowExecutionAssemblyResult {
    Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated execution_package;
    std::vector<Siligen::TrajectoryPoint> execution_trajectory_points;
    std::vector<Siligen::TrajectoryPoint> motion_trajectory_points;
    bool preview_authority_shared_with_execution = false;
    bool execution_binding_ready = false;
    std::string execution_failure_reason;
    Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout authority_trigger_layout;
    PlanningArtifactExportRequest export_request;
};

struct WorkflowPlanningAssemblyRequest {
    WorkflowAuthorityPreviewRequest authority_preview_request;
    Siligen::MotionPlanning::Contracts::MotionPlan motion_plan;
    Siligen::Shared::Types::float32 max_jerk = 0.0f;
    Siligen::Shared::Types::float32 estimated_time_s = 0.0f;
    bool use_interpolation_planner = false;
    Siligen::MotionPlanning::Contracts::InterpolationAlgorithm interpolation_algorithm =
        Siligen::MotionPlanning::Contracts::InterpolationAlgorithm::LINEAR;
};

struct WorkflowPlanningAssemblyResult {
    Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated execution_package;
    int segment_count = 0;
    Siligen::Shared::Types::float32 total_length = 0.0f;
    Siligen::Shared::Types::float32 estimated_time = 0.0f;
    std::vector<Siligen::TrajectoryPoint> trajectory_points;
    std::vector<Siligen::Shared::Types::Point2D> glue_points;
    int trigger_count = 0;
    std::string dxf_filename;
    Siligen::Shared::Types::int32 timestamp = 0;
    Siligen::MotionPlanning::Contracts::MotionPlanningReport planning_report;
    bool preview_authority_ready = false;
    bool preview_authority_shared_with_execution = false;
    bool preview_binding_ready = false;
    bool preview_spacing_valid = false;
    bool preview_has_short_segment_exceptions = false;
    std::string preview_validation_classification;
    std::string preview_exception_reason;
    std::string preview_failure_reason;
    Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout authority_trigger_layout;
    std::vector<WorkflowAuthorityTriggerPoint> authority_trigger_points;
    std::vector<WorkflowSpacingValidationGroup> spacing_validation_groups;
    PlanningArtifactExportRequest export_request;
};

}  // namespace Siligen::Application::Services::Dispensing
