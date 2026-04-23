#pragma once

#include "application/services/dispensing/WorkflowPlanningAssemblyTypes.h"
#include "runtime_execution/contracts/dispensing/ProfileCompareRuntimeContract.h"
#include "shared/types/Result.h"

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace Siligen::Application::Services::Dispensing::Internal {

using Siligen::Domain::Dispensing::Contracts::ExecutionPackageBuilt;
using Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated;
using Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout;
using Siligen::MotionPlanning::Contracts::InterpolationAlgorithm;
using Siligen::MotionPlanning::Contracts::MotionPlan;
using Siligen::MotionPlanning::Contracts::MotionTrajectory;
using Siligen::MotionPlanning::Contracts::MotionTrajectoryPoint;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::ProcessPath::Contracts::Segment;
using Siligen::ProcessPath::Contracts::SegmentType;
using Siligen::RuntimeExecution::Contracts::Motion::InterpolationData;
using Siligen::Shared::Types::Point2D;
using Siligen::Domain::Dispensing::Contracts::FormalCompareGateDiagnostic;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::DispensingExecutionGeometryKind;
using Siligen::Shared::Types::DispensingExecutionStrategy;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::uint32;
using Siligen::TrajectoryPoint;

inline constexpr float32 kEpsilon = 1e-6f;
inline constexpr float32 kGluePointDedupEpsilonMm = 1e-4f;

struct AuthorityTriggerPoint {
    Point2D position;
    float32 trigger_distance_mm = 0.0f;
    std::size_t segment_index = 0;
    bool short_segment_exception = false;
    bool shared_vertex = false;
};

struct SpacingValidationGroup {
    std::size_t segment_index = 0;
    std::vector<Point2D> points;
    float32 actual_spacing_mm = 0.0f;
    bool short_segment_exception = false;
    bool within_window = false;
};

struct AuthorityPreviewBuildInput {
    ProcessPath process_path;
    ProcessPath authority_process_path;
    std::string source_path;
    std::string dxf_filename;
    float32 dispensing_velocity = 0.0f;
    float32 acceleration = 0.0f;
    uint32 dispenser_interval_ms = 0;
    uint32 dispenser_duration_ms = 0;
    float32 trigger_spatial_interval_mm = 0.0f;
    float32 valve_response_ms = 0.0f;
    float32 safety_margin_ms = 0.0f;
    float32 min_interval_ms = 0.0f;
    float32 sample_dt = 0.01f;
    float32 sample_ds = 0.0f;
    float32 spline_max_step_mm = 0.0f;
    float32 spline_max_error_mm = 0.0f;
    Siligen::Shared::Types::DispensingStrategy dispensing_strategy =
        Siligen::Shared::Types::DispensingStrategy::BASELINE;
    int subsegment_count = 8;
    bool dispense_only_cruise = false;
    bool downgrade_on_violation = true;
    Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile compensation_profile{};
    float32 spacing_tol_ratio = 0.0f;
    float32 spacing_min_mm = 0.0f;
    float32 spacing_max_mm = 0.0f;
};

struct AuthorityPreviewBuildResult {
    int segment_count = 0;
    float32 total_length = 0.0f;
    float32 estimated_time = 0.0f;
    ProcessPath canonical_execution_process_path;
    std::vector<TrajectoryPoint> preview_trajectory_points;
    std::vector<Point2D> glue_points;
    int trigger_count = 0;
    std::string dxf_filename;
    int32 timestamp = 0;
    bool preview_authority_ready = false;
    bool preview_binding_ready = false;
    bool preview_spacing_valid = false;
    bool preview_has_short_segment_exceptions = false;
    std::string preview_validation_classification;
    std::string preview_exception_reason;
    std::string preview_failure_reason;
    AuthorityTriggerLayout authority_trigger_layout;
    std::vector<AuthorityTriggerPoint> authority_trigger_points;
    std::vector<SpacingValidationGroup> spacing_validation_groups;
};

struct ExecutionAssemblyBuildInput {
    ProcessPath authority_process_path;
    ProcessPath canonical_execution_process_path;
    MotionPlan motion_plan;
    Point2D planning_start_position{};
    std::string source_path;
    std::string dxf_filename;
    float32 dispensing_velocity = 0.0f;
    float32 acceleration = 0.0f;
    uint32 dispenser_interval_ms = 0;
    uint32 dispenser_duration_ms = 0;
    float32 trigger_spatial_interval_mm = 0.0f;
    float32 valve_response_ms = 0.0f;
    float32 safety_margin_ms = 0.0f;
    float32 min_interval_ms = 0.0f;
    float32 max_jerk = 0.0f;
    float32 sample_dt = 0.01f;
    float32 sample_ds = 0.0f;
    float32 spline_max_step_mm = 0.0f;
    float32 spline_max_error_mm = 0.0f;
    float32 execution_nominal_time_s = 0.0f;
    bool use_interpolation_planner = false;
    InterpolationAlgorithm interpolation_algorithm = InterpolationAlgorithm::LINEAR;
    DispensingExecutionStrategy requested_execution_strategy = DispensingExecutionStrategy::FLYING_SHOT;
    std::optional<Siligen::Shared::Types::PointFlyingCarrierPolicy> point_flying_carrier_policy;
    Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile compensation_profile{};
    Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareRuntimeContract
        profile_compare_runtime_contract{};
    AuthorityPreviewBuildResult authority_preview;
};

struct ExecutionGenerationArtifacts {
    std::vector<InterpolationData> interpolation_segments;
    std::vector<TrajectoryPoint> interpolation_points;
    MotionTrajectory motion_trajectory;
};

struct ExecutionAssemblyBuildResult {
    ExecutionPackageValidated execution_package;
    std::vector<TrajectoryPoint> execution_trajectory_points;
    std::vector<TrajectoryPoint> interpolation_trajectory_points;
    std::vector<TrajectoryPoint> motion_trajectory_points;
    bool preview_authority_shared_with_execution = false;
    bool execution_binding_ready = false;
    bool execution_contract_ready = false;
    std::string execution_failure_reason;
    std::string execution_diagnostic_code;
    FormalCompareGateDiagnostic formal_compare_gate;
    AuthorityTriggerLayout authority_trigger_layout;
    Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportRequest export_request;
};

struct PlanningArtifactsAssemblyInput {
    ProcessPath process_path;
    ProcessPath authority_process_path;
    MotionPlan motion_plan;
    std::string source_path;
    std::string dxf_filename;
    float32 dispensing_velocity = 0.0f;
    float32 acceleration = 0.0f;
    uint32 dispenser_interval_ms = 0;
    uint32 dispenser_duration_ms = 0;
    float32 trigger_spatial_interval_mm = 0.0f;
    float32 valve_response_ms = 0.0f;
    float32 safety_margin_ms = 0.0f;
    float32 min_interval_ms = 0.0f;
    float32 max_jerk = 0.0f;
    float32 sample_dt = 0.01f;
    float32 sample_ds = 0.0f;
    float32 spline_max_step_mm = 0.0f;
    float32 spline_max_error_mm = 0.0f;
    float32 execution_nominal_time_s = 0.0f;
    Siligen::Shared::Types::DispensingStrategy dispensing_strategy =
        Siligen::Shared::Types::DispensingStrategy::BASELINE;
    int subsegment_count = 8;
    bool dispense_only_cruise = false;
    bool downgrade_on_violation = true;
    bool use_interpolation_planner = false;
    InterpolationAlgorithm interpolation_algorithm = InterpolationAlgorithm::LINEAR;
    Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile compensation_profile{};
    float32 spacing_tol_ratio = 0.0f;
    float32 spacing_min_mm = 0.0f;
    float32 spacing_max_mm = 0.0f;
};

struct TriggerArtifacts {
    std::vector<float32> distances;
    std::vector<Point2D> positions;
    std::vector<AuthorityTriggerPoint> authority_trigger_points;
    std::vector<SpacingValidationGroup> spacing_validation_groups;
    AuthorityTriggerLayout authority_trigger_layout;
    uint32 interval_ms = 0;
    float32 interval_mm = 0.0f;
    bool downgraded = false;
    bool binding_ready = false;
    bool spacing_valid = false;
    bool has_short_segment_exceptions = false;
    std::string validation_classification;
    std::string exception_reason;
    std::string failure_reason;
};

template <typename ClockPoint>
long long ElapsedMs(const ClockPoint& start_time) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start_time)
        .count();
}

AuthorityPreviewBuildInput BuildAuthorityPreviewBuildInput(const WorkflowAuthorityPreviewRequest& input);
WorkflowAuthorityPreviewArtifacts BuildWorkflowAuthorityPreviewArtifacts(const AuthorityPreviewBuildResult& input);
AuthorityPreviewBuildResult BuildAuthorityPreviewBuildResult(const WorkflowAuthorityPreviewArtifacts& input);
ExecutionAssemblyBuildInput BuildExecutionAssemblyBuildInput(const WorkflowExecutionAssemblyRequest& input);
WorkflowExecutionAssemblyResult BuildWorkflowExecutionAssemblyResult(const ExecutionAssemblyBuildResult& input);

float32 SegmentLength(const Segment& segment);
float32 ComputeProcessPathLength(const ProcessPath& process_path);
const ProcessPath& ResolveAuthorityProcessPath(const PlanningArtifactsAssemblyInput& input);
const ProcessPath& ResolveAuthorityProcessPath(const AuthorityPreviewBuildInput& input);
const ProcessPath& ResolveExecutionProcessPath(const ExecutionAssemblyBuildInput& input);
DispensingExecutionGeometryKind ResolveExecutionGeometryKind(const ProcessPath& execution_process_path);
DispensingExecutionStrategy ResolveExecutionStrategy(
    DispensingExecutionStrategy requested_strategy,
    DispensingExecutionGeometryKind geometry_kind,
    const ExecutionGenerationArtifacts& generation_artifacts,
    const MotionPlan& motion_plan);
float32 EstimateExecutionTime(const PlanningArtifactsAssemblyInput& input, const ExecutionPackageBuilt& built);
float32 ResolveInterpolationStep(const PlanningArtifactsAssemblyInput& input);
std::vector<TrajectoryPoint> ConvertMotionTrajectoryToTrajectoryPoints(const MotionTrajectory& trajectory);
Siligen::Shared::Types::Result<std::vector<Point2D>> BuildInterpolationSeedPoints(
    const ProcessPath& path,
    float32 spline_max_error_mm,
    float32 step_mm);
std::vector<TrajectoryPoint> ConvertPreviewPointsToTrajectory(const std::vector<Point2D>& preview_points);
Siligen::Shared::Types::Result<std::vector<TrajectoryPoint>> BuildTimedExecutionTrajectoryFromMotionPlan(
    const std::vector<Point2D>& preview_points,
    const MotionPlan& motion_plan);
Siligen::Shared::Types::Result<void> ValidatePathInterpolationTiming(
    const std::vector<TrajectoryPoint>& points,
    float32 expected_total_time_s,
    const char* error_source);
float32 EstimatePreviewTime(const AuthorityPreviewBuildInput& input, float32 total_length_mm);
std::vector<Point2D> CollectAuthorityPositions(const AuthorityTriggerLayout& layout);
Siligen::Shared::Types::Result<ProcessPath> BuildCanonicalExecutionProcessPath(
    const ProcessPath& authority_process_path,
    AuthorityTriggerLayout& authority_trigger_layout);
TriggerArtifacts BuildTriggerArtifactsFromAuthorityPreview(const AuthorityPreviewBuildResult& authority_preview);
Siligen::Shared::Types::Result<std::vector<TrajectoryPoint>> BuildInterpolationPoints(
    const PlanningArtifactsAssemblyInput& input,
    const ProcessPath& path,
    const TriggerArtifacts& trigger_artifacts);
Siligen::Shared::Types::Result<ExecutionGenerationArtifacts> BuildExecutionGenerationArtifacts(
    const ExecutionAssemblyBuildInput& input,
    const ProcessPath& execution_process_path,
    const TriggerArtifacts& trigger_artifacts);
void BindAuthorityLayoutToExecutionTrajectory(
    TriggerArtifacts& artifacts,
    const std::vector<TrajectoryPoint>* execution_trajectory);
Siligen::Shared::Types::Result<ExecutionPackageValidated> BuildValidatedExecutionPackage(
    const ExecutionAssemblyBuildInput& input,
    const ProcessPath& execution_process_path,
    const TriggerArtifacts& trigger_artifacts,
    ExecutionGenerationArtifacts generation_artifacts);
Siligen::Shared::Types::Result<ExecutionPackageValidated> BuildFormalProfileCompareExecutionPackage(
    const ExecutionPackageValidated& base_package,
    const TriggerArtifacts& trigger_artifacts,
    const Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareRuntimeContract& runtime_contract,
    FormalCompareGateDiagnostic* formal_compare_gate = nullptr);
void PopulateExecutionTrajectorySelection(
    const ExecutionPackageValidated& execution_package,
    ExecutionAssemblyBuildResult& result,
    bool& authority_shared);
Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportRequest BuildExecutionExportRequest(
    const ExecutionAssemblyBuildInput& input,
    const ProcessPath& execution_process_path,
    const ExecutionAssemblyBuildResult& result);

Siligen::Shared::Types::Result<AuthorityPreviewBuildResult> AssembleAuthorityPreviewArtifacts(
    const AuthorityPreviewBuildInput& input);
Siligen::Shared::Types::Result<ExecutionAssemblyBuildResult> AssembleExecutionArtifacts(
    const ExecutionAssemblyBuildInput& input);

}  // namespace Siligen::Application::Services::Dispensing::Internal
