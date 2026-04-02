#include "PlanningUseCase.h"

#include "application/services/dxf/DxfPbPreparationService.h"
#include "application/services/dispensing/PlanningArtifactExportAssemblyService.h"
#include "domain/trajectory/value-objects/GeometryUtils.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "workflow/contracts/WorkflowContracts.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "PlanningUseCase"

namespace Siligen::Application::UseCases::Dispensing {

using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
using Siligen::Application::Services::ProcessPath::ProcessPathBuildResult;
using Siligen::Application::Services::Dispensing::AuthorityPreviewBuildInput;
using Siligen::Application::Services::Dispensing::ExecutionAssemblyBuildInput;
using Siligen::Domain::Motion::ValueObjects::TimePlanningConfig;
using Siligen::Domain::Trajectory::Ports::PathSourceResult;
using Siligen::Domain::Trajectory::ValueObjects::ContourElement;
using Siligen::Domain::Trajectory::ValueObjects::ContourElementType;
using Siligen::Domain::Trajectory::ValueObjects::Primitive;
using Siligen::Domain::Trajectory::ValueObjects::PrimitiveType;
using Siligen::Domain::Trajectory::ValueObjects::ProcessTag;
using Siligen::Domain::Trajectory::ValueObjects::Segment;
using Siligen::MotionPlanning::Contracts::MotionPlan;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Workflow::Contracts::WorkflowCommand;
using Siligen::Workflow::Contracts::WorkflowFailureCategory;
using Siligen::Workflow::Contracts::WorkflowPlanningTriggerRequest;
using Siligen::Workflow::Contracts::WorkflowPlanningTriggerResponse;
using Siligen::Workflow::Contracts::WorkflowRecoveryAction;
using Siligen::Workflow::Contracts::WorkflowStageLifecycle;

namespace {

constexpr float32 kEpsilon = 1e-6f;
constexpr float32 kBoundsToleranceMm = 1e-4f;

struct PreviewRuntimeParams {
    float32 dispensing_velocity = 0.0f;
    float32 acceleration = 0.0f;
    float32 pulse_per_mm = 200.0f;
    uint32 dispenser_interval_ms = 0;
    uint32 dispenser_duration_ms = 0;
    float32 trigger_spatial_interval_mm = 0.0f;
    float32 valve_response_ms = 0.0f;
    float32 safety_margin_ms = 0.0f;
    float32 min_interval_ms = 0.0f;
    float32 sample_dt = 0.01f;
    float32 sample_ds = 0.0f;
    Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile compensation_profile{};
};

struct MachineBounds {
    bool enabled = false;
    float32 x_min = 0.0f;
    float32 x_max = 0.0f;
    float32 y_min = 0.0f;
    float32 y_max = 0.0f;
};

struct PrimitiveBounds {
    bool has = false;
    float32 min_x = 0.0f;
    float32 max_x = 0.0f;
    float32 min_y = 0.0f;
    float32 max_y = 0.0f;
};

WorkflowPlanningTriggerRequest BuildWorkflowPlanningTriggerRequest(const PlanningRequest& request) {
    WorkflowPlanningTriggerRequest trigger_request;
    trigger_request.workflow_run_id = request.dxf_filepath;
    trigger_request.source_artifact = "WorkflowRun";
    trigger_request.source_path = request.dxf_filepath;
    trigger_request.optimize_path = request.optimize_path;
    trigger_request.use_interpolation_planner = request.use_interpolation_planner;
    return trigger_request;
}

WorkflowPlanningTriggerResponse BuildWorkflowPlanningTriggerResponse(
    const WorkflowPlanningTriggerRequest& trigger_request,
    WorkflowStageLifecycle lifecycle,
    WorkflowFailureCategory failure_category,
    const std::string& prepared_input_path,
    const std::string& reason) {
    WorkflowPlanningTriggerResponse response;
    response.accepted = (lifecycle == WorkflowStageLifecycle::Completed);
    response.prepared_input_path = prepared_input_path;
    response.failure_category = failure_category;
    response.stage_state.workflow_run_id = trigger_request.workflow_run_id;
    response.stage_state.source_artifact = trigger_request.source_artifact;
    response.stage_state.stage_name = "M0->M6/M7/M8-planning-trigger";
    response.stage_state.last_command = WorkflowCommand::PreparePlanning;
    response.stage_state.lifecycle = lifecycle;
    response.stage_state.failure_category = failure_category;
    response.stage_state.recovery_required = (failure_category != WorkflowFailureCategory::None);
    if (failure_category != WorkflowFailureCategory::None) {
        response.recovery_directive.action = WorkflowRecoveryAction::EscalateToHost;
        response.recovery_directive.retryable = false;
        response.recovery_directive.reason = reason;
    }
    return response;
}

PreviewRuntimeParams BuildPreviewRuntimeParams(
    const PlanningRequest& request,
    const std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort>& config_port) {
    PreviewRuntimeParams params;
    params.dispensing_velocity = request.trajectory_config.max_velocity;
    params.acceleration = request.trajectory_config.max_acceleration;
    params.sample_dt = request.trajectory_config.time_step;
    params.dispenser_duration_ms = static_cast<uint32>(std::max(
        1.0f,
        static_cast<float32>(request.trajectory_config.trigger_pulse_us) / 1000.0f));

    float32 safety_limit_velocity = 0.0f;
    if (config_port) {
        auto config_result = config_port->LoadConfiguration();
        if (config_result.IsSuccess()) {
            const auto& config = config_result.Value();
            if (config.machine.max_speed > 0.0f) {
                safety_limit_velocity = config.machine.max_speed;
            }
            if (config.machine.max_acceleration > 0.0f) {
                params.acceleration = config.machine.max_acceleration;
            }
            if (config.machine.pulse_per_mm > 0.0f) {
                params.pulse_per_mm = config.machine.pulse_per_mm;
            }
            if (config.dispensing.dot_diameter_target_mm > 0.0f) {
                float32 interval = config.dispensing.dot_diameter_target_mm;
                if (config.dispensing.dot_edge_gap_mm > 0.0f) {
                    interval += config.dispensing.dot_edge_gap_mm;
                }
                params.trigger_spatial_interval_mm = interval;
            }
            params.compensation_profile.open_comp_ms = config.dispensing.open_comp_ms;
            params.compensation_profile.close_comp_ms = config.dispensing.close_comp_ms;
            params.compensation_profile.retract_enabled = config.dispensing.retract_enabled;
            params.compensation_profile.corner_pulse_scale = config.dispensing.corner_pulse_scale;
            params.compensation_profile.curvature_speed_factor = config.dispensing.curvature_speed_factor;
            params.sample_dt = config.dispensing.trajectory_sample_dt;
            params.sample_ds = config.dispensing.trajectory_sample_ds;
        } else {
            SILIGEN_LOG_WARNING("读取系统配置失败: " + config_result.GetError().GetMessage());
        }

        auto valve_coord_result = config_port->GetValveCoordinationConfig();
        if (valve_coord_result.IsSuccess()) {
            const auto& valve_cfg = valve_coord_result.Value();
            if (valve_cfg.dispensing_interval_ms > 0) {
                params.dispenser_interval_ms = static_cast<uint32>(valve_cfg.dispensing_interval_ms);
            }
            if (valve_cfg.dispensing_duration_ms > 0) {
                params.dispenser_duration_ms = static_cast<uint32>(valve_cfg.dispensing_duration_ms);
            }
            if (valve_cfg.valve_response_ms >= 0) {
                params.valve_response_ms = static_cast<float32>(valve_cfg.valve_response_ms);
            }
            if (valve_cfg.visual_margin_ms >= 0) {
                params.safety_margin_ms = static_cast<float32>(valve_cfg.visual_margin_ms);
            }
            if (valve_cfg.min_interval_ms >= 0) {
                params.min_interval_ms = static_cast<float32>(valve_cfg.min_interval_ms);
            }
        }
    }

    if (request.trajectory_config.max_velocity > 0.0f) {
        params.dispensing_velocity = request.trajectory_config.max_velocity;
    }
    if (request.trajectory_config.max_acceleration > 0.0f) {
        params.acceleration = request.trajectory_config.max_acceleration;
    }
    if (request.trajectory_config.dispensing_interval > 0.0f) {
        params.trigger_spatial_interval_mm = request.trajectory_config.dispensing_interval;
    }
    if (safety_limit_velocity > 0.0f && params.dispensing_velocity > 0.0f) {
        params.dispensing_velocity = std::min(params.dispensing_velocity, safety_limit_velocity);
    }

    if (params.pulse_per_mm <= 0.0f) {
        params.pulse_per_mm = 200.0f;
    }
    if (params.dispenser_interval_ms == 0) {
        params.dispenser_interval_ms = 100;
    }
    if (params.dispenser_duration_ms == 0) {
        params.dispenser_duration_ms = 100;
    }
    if (params.trigger_spatial_interval_mm <= 0.0f) {
        params.trigger_spatial_interval_mm = 3.0f;
    }
    if (params.trigger_spatial_interval_mm <= 0.0f && params.dispensing_velocity > 0.0f) {
        params.trigger_spatial_interval_mm =
            params.dispensing_velocity * (static_cast<float32>(params.dispenser_interval_ms) / 1000.0f);
    }

    return params;
}

Siligen::Shared::Types::Point2D ResolveDxfOffset(
    const std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort>& config_port) {
    if (!config_port) {
        return {};
    }

    auto config_result = config_port->LoadConfiguration();
    if (config_result.IsError()) {
        SILIGEN_LOG_WARNING("读取系统配置失败: " + config_result.GetError().GetMessage());
        return {};
    }

    const auto& config = config_result.Value();
    return Siligen::Shared::Types::Point2D(config.dxf.offset_x, config.dxf.offset_y);
}

MachineBounds ResolveMachineBounds(
    const std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort>& config_port) {
    if (!config_port) {
        return {};
    }

    auto machine_result = config_port->GetMachineConfig();
    if (machine_result.IsError()) {
        SILIGEN_LOG_WARNING("读取机器配置失败: " + machine_result.GetError().GetMessage());
        return {};
    }

    const auto& machine = machine_result.Value();
    if (machine.soft_limits.x_max <= machine.soft_limits.x_min ||
        machine.soft_limits.y_max <= machine.soft_limits.y_min) {
        SILIGEN_LOG_WARNING("Machine soft limits 无效，跳过DXF范围归位");
        return {};
    }

    MachineBounds bounds;
    bounds.enabled = true;
    bounds.x_min = machine.soft_limits.x_min;
    bounds.x_max = machine.soft_limits.x_max;
    bounds.y_min = machine.soft_limits.y_min;
    bounds.y_max = machine.soft_limits.y_max;
    return bounds;
}

void ApplyOffsetToPrimitives(
    std::vector<Primitive>& primitives,
    const Siligen::Shared::Types::Point2D& offset);

void UpdateBounds(PrimitiveBounds& bounds, const Point2D& point) {
    if (!bounds.has) {
        bounds.has = true;
        bounds.min_x = point.x;
        bounds.max_x = point.x;
        bounds.min_y = point.y;
        bounds.max_y = point.y;
        return;
    }

    bounds.min_x = std::min(bounds.min_x, point.x);
    bounds.max_x = std::max(bounds.max_x, point.x);
    bounds.min_y = std::min(bounds.min_y, point.y);
    bounds.max_y = std::max(bounds.max_y, point.y);
}

bool IsAngleOnArc(float32 start_angle, float32 end_angle, bool clockwise, float32 angle) {
    const float32 total = Siligen::Domain::Trajectory::ValueObjects::ComputeArcSweep(start_angle, end_angle, clockwise);
    const float32 sweep = Siligen::Domain::Trajectory::ValueObjects::ComputeArcSweep(start_angle, angle, clockwise);
    return sweep <= total + 1e-3f;
}

void UpdateBoundsForArc(
    const Siligen::Domain::Trajectory::ValueObjects::ArcPrimitive& arc,
    PrimitiveBounds& bounds) {
    const float32 start_angle = Siligen::Domain::Trajectory::ValueObjects::NormalizeAngle(arc.start_angle_deg);
    const float32 end_angle = Siligen::Domain::Trajectory::ValueObjects::NormalizeAngle(arc.end_angle_deg);
    UpdateBounds(bounds, Siligen::Domain::Trajectory::ValueObjects::ArcPoint(arc, start_angle));
    UpdateBounds(bounds, Siligen::Domain::Trajectory::ValueObjects::ArcPoint(arc, end_angle));

    const float32 cardinal_angles[] = {0.0f, 90.0f, 180.0f, 270.0f};
    for (float32 angle : cardinal_angles) {
        const float32 normalized = Siligen::Domain::Trajectory::ValueObjects::NormalizeAngle(angle);
        if (IsAngleOnArc(start_angle, end_angle, arc.clockwise, normalized)) {
            UpdateBounds(bounds, Siligen::Domain::Trajectory::ValueObjects::ArcPoint(arc, normalized));
        }
    }
}

void UpdateBoundsForEllipse(
    const Siligen::Domain::Trajectory::ValueObjects::EllipsePrimitive& ellipse,
    PrimitiveBounds& bounds) {
    const Point2D major = ellipse.major_axis;
    const float32 a = major.Length();
    const float32 b = a * ellipse.ratio;
    if (a <= kEpsilon || b <= kEpsilon) {
        UpdateBounds(bounds, ellipse.center);
        return;
    }

    const float32 theta = std::atan2(major.y, major.x);
    const float32 cos_t = std::cos(theta);
    const float32 sin_t = std::sin(theta);
    const Point2D ex(cos_t, sin_t);
    const Point2D ey(-sin_t, cos_t);
    UpdateBounds(bounds, ellipse.center + ex * a);
    UpdateBounds(bounds, ellipse.center - ex * a);
    UpdateBounds(bounds, ellipse.center + ey * b);
    UpdateBounds(bounds, ellipse.center - ey * b);
}

void UpdateBoundsForSpline(const std::vector<Point2D>& control_points, PrimitiveBounds& bounds) {
    for (const auto& point : control_points) {
        UpdateBounds(bounds, point);
    }
}

PrimitiveBounds ComputePrimitiveBounds(const std::vector<Primitive>& primitives) {
    PrimitiveBounds bounds;
    for (const auto& primitive : primitives) {
        switch (primitive.type) {
            case PrimitiveType::Line:
                UpdateBounds(bounds, primitive.line.start);
                UpdateBounds(bounds, primitive.line.end);
                break;
            case PrimitiveType::Arc:
                UpdateBoundsForArc(primitive.arc, bounds);
                break;
            case PrimitiveType::Spline:
                UpdateBoundsForSpline(primitive.spline.control_points, bounds);
                break;
            case PrimitiveType::Circle:
                UpdateBounds(bounds, Point2D(primitive.circle.center.x + primitive.circle.radius, primitive.circle.center.y));
                UpdateBounds(bounds, Point2D(primitive.circle.center.x - primitive.circle.radius, primitive.circle.center.y));
                UpdateBounds(bounds, Point2D(primitive.circle.center.x, primitive.circle.center.y + primitive.circle.radius));
                UpdateBounds(bounds, Point2D(primitive.circle.center.x, primitive.circle.center.y - primitive.circle.radius));
                break;
            case PrimitiveType::Ellipse:
                UpdateBoundsForEllipse(primitive.ellipse, bounds);
                break;
            case PrimitiveType::Point:
                UpdateBounds(bounds, primitive.point.position);
                break;
            case PrimitiveType::Contour:
                for (const auto& element : primitive.contour.elements) {
                    switch (element.type) {
                        case ContourElementType::Line:
                            UpdateBounds(bounds, element.line.start);
                            UpdateBounds(bounds, element.line.end);
                            break;
                        case ContourElementType::Arc:
                            UpdateBoundsForArc(element.arc, bounds);
                            break;
                        case ContourElementType::Spline:
                            UpdateBoundsForSpline(element.spline.control_points, bounds);
                            break;
                    }
                }
                break;
        }
    }
    return bounds;
}

Result<void> AutoFitPrimitivesIntoMachineBounds(
    std::vector<Primitive>& primitives,
    const MachineBounds& bounds) {
    if (!bounds.enabled) {
        return Result<void>::Success();
    }

    PrimitiveBounds primitive_bounds = ComputePrimitiveBounds(primitives);
    if (!primitive_bounds.has) {
        return Result<void>::Success();
    }

    const float32 bounds_width = bounds.x_max - bounds.x_min;
    const float32 bounds_height = bounds.y_max - bounds.y_min;
    const float32 shape_width = primitive_bounds.max_x - primitive_bounds.min_x;
    const float32 shape_height = primitive_bounds.max_y - primitive_bounds.min_y;
    const bool fits_x = (shape_width <= bounds_width + kBoundsToleranceMm);
    const bool fits_y = (shape_height <= bounds_height + kBoundsToleranceMm);

    if (fits_x && fits_y) {
        const float32 tx_min = bounds.x_min - primitive_bounds.min_x;
        const float32 tx_max = bounds.x_max - primitive_bounds.max_x;
        const float32 ty_min = bounds.y_min - primitive_bounds.min_y;
        const float32 ty_max = bounds.y_max - primitive_bounds.max_y;
        const float32 tx = std::clamp(0.0f, std::min(tx_min, tx_max), std::max(tx_min, tx_max));
        const float32 ty = std::clamp(0.0f, std::min(ty_min, ty_max), std::max(ty_min, ty_max));
        if (std::abs(tx) > kEpsilon || std::abs(ty) > kEpsilon) {
            ApplyOffsetToPrimitives(primitives, Point2D(tx, ty));
            SILIGEN_LOG_WARNING_FMT_HELPER("DXF坐标自动平移至行程内: offset=(%.6f, %.6f)", tx, ty);
            primitive_bounds = ComputePrimitiveBounds(primitives);
        }
    }

    const bool out_x = (primitive_bounds.min_x < bounds.x_min - kBoundsToleranceMm) ||
                       (primitive_bounds.max_x > bounds.x_max + kBoundsToleranceMm);
    const bool out_y = (primitive_bounds.min_y < bounds.y_min - kBoundsToleranceMm) ||
                       (primitive_bounds.max_y > bounds.y_max + kBoundsToleranceMm);
    if (!out_x && !out_y) {
        return Result<void>::Success();
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "DXF超出行程范围: "
        << "x=[" << primitive_bounds.min_x << "," << primitive_bounds.max_x << "] "
        << "y=[" << primitive_bounds.min_y << "," << primitive_bounds.max_y << "] "
        << "limits x=[" << bounds.x_min << "," << bounds.x_max << "] "
        << "y=[" << bounds.y_min << "," << bounds.y_max << "]";
    return Result<void>::Failure(Error(ErrorCode::POSITION_OUT_OF_RANGE, oss.str(), "PlanningUseCase"));
}

void ApplyOffsetPoint(Siligen::Shared::Types::Point2D& point, const Siligen::Shared::Types::Point2D& offset) {
    point = point + offset;
}

void ApplyOffsetToContourElement(ContourElement& element, const Siligen::Shared::Types::Point2D& offset) {
    switch (element.type) {
        case ContourElementType::Line:
            ApplyOffsetPoint(element.line.start, offset);
            ApplyOffsetPoint(element.line.end, offset);
            break;
        case ContourElementType::Arc:
            ApplyOffsetPoint(element.arc.center, offset);
            break;
        case ContourElementType::Spline:
            for (auto& point : element.spline.control_points) {
                ApplyOffsetPoint(point, offset);
            }
            break;
    }
}

void ApplyOffsetToPrimitive(Primitive& primitive, const Siligen::Shared::Types::Point2D& offset) {
    switch (primitive.type) {
        case PrimitiveType::Line:
            ApplyOffsetPoint(primitive.line.start, offset);
            ApplyOffsetPoint(primitive.line.end, offset);
            break;
        case PrimitiveType::Arc:
            ApplyOffsetPoint(primitive.arc.center, offset);
            break;
        case PrimitiveType::Spline:
            for (auto& point : primitive.spline.control_points) {
                ApplyOffsetPoint(point, offset);
            }
            break;
        case PrimitiveType::Circle:
            ApplyOffsetPoint(primitive.circle.center, offset);
            break;
        case PrimitiveType::Ellipse:
            ApplyOffsetPoint(primitive.ellipse.center, offset);
            break;
        case PrimitiveType::Point:
            ApplyOffsetPoint(primitive.point.position, offset);
            break;
        case PrimitiveType::Contour:
            for (auto& element : primitive.contour.elements) {
                ApplyOffsetToContourElement(element, offset);
            }
            break;
    }
}

void ApplyOffsetToPrimitives(
    std::vector<Primitive>& primitives,
    const Siligen::Shared::Types::Point2D& offset) {
    if (std::abs(offset.x) <= kEpsilon && std::abs(offset.y) <= kEpsilon) {
        return;
    }
    for (auto& primitive : primitives) {
        ApplyOffsetToPrimitive(primitive, offset);
    }
    SILIGEN_LOG_INFO_FMT_HELPER("DXF坐标平移: offset=(%.6f, %.6f)", offset.x, offset.y);
}

ProcessPathBuildRequest BuildProcessPathRequest(
    const PlanningRequest& request,
    std::vector<Primitive> primitives,
    const std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort>& config_port) {
    ProcessPathBuildRequest process_path_request;
    process_path_request.primitives = std::move(primitives);
    process_path_request.normalization.approximate_splines = request.approximate_splines;
    process_path_request.normalization.spline_max_step_mm = request.spline_max_step_mm;
    process_path_request.normalization.spline_max_error_mm = request.spline_max_error_mm;
    if (request.continuity_tolerance_mm > kEpsilon) {
        process_path_request.normalization.continuity_tolerance = request.continuity_tolerance_mm;
    }

    process_path_request.process.default_flow = 1.0f;
    process_path_request.process.lead_on_dist = 0.0f;
    process_path_request.process.lead_off_dist = 0.0f;
    process_path_request.process.corner_slowdown = true;
    process_path_request.process.start_speed_factor = 0.5f;
    process_path_request.process.end_speed_factor = 0.5f;
    process_path_request.process.corner_speed_factor = 0.6f;
    process_path_request.process.rapid_speed_factor = 1.0f;
    process_path_request.shaping.corner_smoothing_radius = 0.0f;
    process_path_request.shaping.corner_max_deviation_mm = 0.0f;
    if (request.curve_chain_angle_deg > kEpsilon) {
        process_path_request.process.curve_chain_angle_deg = request.curve_chain_angle_deg;
    }
    if (request.curve_chain_max_segment_mm > kEpsilon) {
        process_path_request.process.curve_chain_max_segment_mm = request.curve_chain_max_segment_mm;
    }

    if (config_port) {
        auto dispensing_result = config_port->GetDispensingConfig();
        if (dispensing_result.IsSuccess()) {
            const auto& dispensing = dispensing_result.Value();
            process_path_request.process.start_speed_factor = dispensing.start_speed_factor;
            process_path_request.process.end_speed_factor = dispensing.end_speed_factor;
            process_path_request.process.corner_speed_factor = dispensing.corner_speed_factor;
            process_path_request.process.rapid_speed_factor = dispensing.rapid_speed_factor;
        }
    }

    return process_path_request;
}

TimePlanningConfig BuildMotionPlanningConfig(
    const PlanningRequest& request,
    const PreviewRuntimeParams& runtime_params,
    const std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort>& config_port) {
    TimePlanningConfig config;
    config.vmax = runtime_params.dispensing_velocity;
    config.amax = runtime_params.acceleration;
    config.jmax = request.trajectory_config.max_jerk;
    config.sample_dt = runtime_params.sample_dt;
    config.sample_ds = runtime_params.sample_ds;
    config.arc_tolerance_mm = request.trajectory_config.arc_tolerance;
    config.curvature_speed_factor = runtime_params.compensation_profile.curvature_speed_factor;
    config.start_speed_factor = 0.5f;
    config.end_speed_factor = 0.5f;
    config.corner_speed_factor = 0.6f;
    config.rapid_speed_factor = 1.0f;
    config.enforce_jerk_limit = true;

    if (config_port) {
        auto dispensing_result = config_port->GetDispensingConfig();
        if (dispensing_result.IsSuccess()) {
            const auto& dispensing = dispensing_result.Value();
            config.start_speed_factor = dispensing.start_speed_factor;
            config.end_speed_factor = dispensing.end_speed_factor;
            config.corner_speed_factor = dispensing.corner_speed_factor;
            config.rapid_speed_factor = dispensing.rapid_speed_factor;
        }
    }

    return config;
}

float32 SegmentLength(const Segment& segment) {
    if (segment.length > kEpsilon) {
        return segment.length;
    }
    switch (segment.type) {
        case Siligen::Domain::Trajectory::ValueObjects::SegmentType::Line:
            return segment.line.start.DistanceTo(segment.line.end);
        case Siligen::Domain::Trajectory::ValueObjects::SegmentType::Arc:
            return Siligen::Domain::Trajectory::ValueObjects::ComputeArcLength(segment.arc);
        case Siligen::Domain::Trajectory::ValueObjects::SegmentType::Spline: {
            float32 total = 0.0f;
            for (size_t i = 1; i < segment.spline.control_points.size(); ++i) {
                total += segment.spline.control_points[i - 1].DistanceTo(segment.spline.control_points[i]);
            }
            return total;
        }
        default:
            return 0.0f;
    }
}

void PopulatePlanningReport(
    const Siligen::ProcessPath::Contracts::ProcessPath& shaped_path,
    int32 discontinuity_count,
    MotionPlan& motion_plan) {
    auto& report = motion_plan.planning_report;
    report.segment_count = static_cast<int32>(shaped_path.segments.size());
    report.discontinuity_count = discontinuity_count;

    int32 rapid_count = 0;
    int32 corner_count = 0;
    float32 rapid_length = 0.0f;
    for (const auto& segment : shaped_path.segments) {
        if (segment.tag == ProcessTag::Rapid) {
            ++rapid_count;
            rapid_length += SegmentLength(segment.geometry);
        } else if (segment.tag == ProcessTag::Corner) {
            ++corner_count;
        }
    }

    report.rapid_segment_count = rapid_count;
    report.rapid_length_mm = rapid_length;
    report.corner_segment_count = corner_count;
}

std::uint32_t ToElapsedMs(const std::chrono::steady_clock::time_point start_time) {
    return static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count());
}

void PopulatePlanningReport(const ProcessPathBuildResult& path_result, MotionPlan& motion_plan) {
    PopulatePlanningReport(
        path_result.shaped_path,
        path_result.normalized.report.discontinuity_count,
        motion_plan);
}

AuthorityPreviewBuildInput BuildAuthorityPreviewInput(
    const PlanningRequest& request,
    const ProcessPathBuildResult& path_result,
    const PreviewRuntimeParams& runtime_params,
    const std::string& source_path,
    const std::string& dxf_filename,
    const std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort>& config_port) {
    AuthorityPreviewBuildInput input;
    input.process_path = path_result.shaped_path;
    input.authority_process_path = path_result.process_path;
    input.source_path = source_path;
    input.dxf_filename = dxf_filename;
    input.dispensing_velocity = runtime_params.dispensing_velocity;
    input.acceleration = runtime_params.acceleration;
    input.dispenser_interval_ms = runtime_params.dispenser_interval_ms;
    input.dispenser_duration_ms = runtime_params.dispenser_duration_ms;
    input.trigger_spatial_interval_mm = runtime_params.trigger_spatial_interval_mm;
    input.valve_response_ms = runtime_params.valve_response_ms;
    input.safety_margin_ms = runtime_params.safety_margin_ms;
    input.min_interval_ms = runtime_params.min_interval_ms;
    input.sample_dt = runtime_params.sample_dt;
    input.sample_ds = runtime_params.sample_ds;
    input.spline_max_step_mm = request.spline_max_step_mm;
    input.spline_max_error_mm = request.spline_max_error_mm;
    input.compensation_profile = runtime_params.compensation_profile;
    input.spacing_tol_ratio = request.spacing_tol_ratio;
    input.spacing_min_mm = request.spacing_min_mm;
    input.spacing_max_mm = request.spacing_max_mm;

    if (!config_port) {
        return input;
    }

    auto dispensing_result = config_port->GetDispensingConfig();
    if (dispensing_result.IsSuccess()) {
        const auto& dispensing = dispensing_result.Value();
        input.dispensing_strategy = dispensing.strategy;
        if (dispensing.subsegment_count > 0) {
            input.subsegment_count = dispensing.subsegment_count;
        }
        input.dispense_only_cruise = dispensing.dispense_only_cruise;
    }

    return input;
}

ExecutionAssemblyBuildInput BuildExecutionAssemblyInput(
    const PlanningRequest& request,
    const PreparedAuthorityPreview& authority_preview,
    MotionPlan motion_plan,
    const PreviewRuntimeParams& runtime_params) {
    ExecutionAssemblyBuildInput input;
    input.process_path = authority_preview.process_path;
    input.motion_plan = std::move(motion_plan);
    input.source_path = authority_preview.source_path;
    input.dxf_filename = authority_preview.dxf_filename;
    input.dispensing_velocity = runtime_params.dispensing_velocity;
    input.acceleration = runtime_params.acceleration;
    input.dispenser_interval_ms = runtime_params.dispenser_interval_ms;
    input.dispenser_duration_ms = runtime_params.dispenser_duration_ms;
    input.trigger_spatial_interval_mm = runtime_params.trigger_spatial_interval_mm;
    input.valve_response_ms = runtime_params.valve_response_ms;
    input.safety_margin_ms = runtime_params.safety_margin_ms;
    input.min_interval_ms = runtime_params.min_interval_ms;
    input.max_jerk = request.trajectory_config.max_jerk;
    input.sample_dt = runtime_params.sample_dt;
    input.sample_ds = runtime_params.sample_ds;
    input.spline_max_step_mm = request.spline_max_step_mm;
    input.spline_max_error_mm = request.spline_max_error_mm;
    input.estimated_time_s = authority_preview.estimated_time;
    input.use_interpolation_planner = request.use_interpolation_planner;
    input.interpolation_algorithm = request.interpolation_algorithm;
    input.compensation_profile = runtime_params.compensation_profile;

    input.authority_preview.segment_count = authority_preview.segment_count;
    input.authority_preview.total_length = authority_preview.total_length;
    input.authority_preview.estimated_time = authority_preview.estimated_time;
    input.authority_preview.preview_trajectory_points = authority_preview.preview_trajectory_points;
    input.authority_preview.glue_points = authority_preview.glue_points;
    input.authority_preview.trigger_count = authority_preview.trigger_count;
    input.authority_preview.dxf_filename = authority_preview.dxf_filename;
    input.authority_preview.timestamp = authority_preview.timestamp;
    input.authority_preview.preview_authority_ready = authority_preview.preview_authority_ready;
    input.authority_preview.preview_binding_ready = authority_preview.preview_binding_ready;
    input.authority_preview.preview_spacing_valid = authority_preview.preview_spacing_valid;
    input.authority_preview.preview_has_short_segment_exceptions =
        authority_preview.preview_has_short_segment_exceptions;
    input.authority_preview.preview_validation_classification =
        authority_preview.preview_validation_classification;
    input.authority_preview.preview_exception_reason = authority_preview.preview_exception_reason;
    input.authority_preview.preview_failure_reason = authority_preview.preview_failure_reason;
    input.authority_preview.authority_trigger_layout = authority_preview.authority_trigger_layout;
    input.authority_preview.authority_trigger_points = authority_preview.authority_trigger_points;
    input.authority_preview.spacing_validation_groups = authority_preview.spacing_validation_groups;
    return input;
}

}  // namespace

PlanningUseCase::PlanningUseCase(
    std::shared_ptr<Siligen::Domain::Trajectory::Ports::IPathSourcePort> path_source,
    std::shared_ptr<Siligen::Application::Services::ProcessPath::ProcessPathFacade> process_path_facade,
    std::shared_ptr<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>
        motion_planning_facade,
    std::shared_ptr<Siligen::Application::Services::Dispensing::AuthorityPreviewAssemblyService>
        authority_preview_assembly_service,
    std::shared_ptr<Siligen::Application::Services::Dispensing::ExecutionAssemblyService>
        execution_assembly_service,
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port,
    std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService> pb_preparation_service,
    std::shared_ptr<Siligen::Application::Services::Dispensing::IPlanningArtifactExportPort> artifact_export_port)
    : path_source_(std::move(path_source)),
      process_path_facade_(process_path_facade
                               ? std::move(process_path_facade)
                               : std::make_shared<Siligen::Application::Services::ProcessPath::ProcessPathFacade>()),
      motion_planning_facade_(motion_planning_facade
                                  ? std::move(motion_planning_facade)
                                  : std::make_shared<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>()),
      authority_preview_assembly_service_(authority_preview_assembly_service
                                              ? std::move(authority_preview_assembly_service)
                                              : std::make_shared<Siligen::Application::Services::Dispensing::AuthorityPreviewAssemblyService>()),
      execution_assembly_service_(execution_assembly_service
                                      ? std::move(execution_assembly_service)
                                      : std::make_shared<Siligen::Application::Services::Dispensing::ExecutionAssemblyService>()),
      config_port_(std::move(config_port)),
      pb_preparation_service_(pb_preparation_service
                                  ? std::move(pb_preparation_service)
                                  : std::make_shared<Siligen::Application::Services::DXF::DxfPbPreparationService>(
                                        config_port_)),
      artifact_export_port_(std::move(artifact_export_port)) {
    if (!path_source_) {
        throw std::invalid_argument("PlanningUseCase: path_source cannot be null");
    }
}

Result<PlanningResponse> PlanningUseCase::Execute(const PlanningRequest& request) {
    SILIGEN_LOG_INFO("Starting DXF planning...");
    const auto total_start = std::chrono::steady_clock::now();

    auto authority_result = PrepareAuthorityPreview(request);
    if (authority_result.IsError()) {
        return Result<PlanningResponse>::Failure(authority_result.GetError());
    }

    auto execution_result = AssembleExecutionFromAuthority(request, authority_result.Value());
    if (execution_result.IsError()) {
        return Result<PlanningResponse>::Failure(execution_result.GetError());
    }

    const auto& authority = authority_result.Value();
    const auto& execution = execution_result.Value();

    PlanningResponse response;
    response.success = true;
    response.segment_count = authority.segment_count;
    response.total_length = execution.execution_package ? execution.execution_package->total_length_mm : authority.total_length;
    response.estimated_time =
        execution.execution_package ? execution.execution_package->estimated_time_s : authority.estimated_time;
    response.execution_trajectory_points = execution.execution_trajectory_points;
    response.glue_points = authority.glue_points;
    response.process_tags.assign(response.execution_trajectory_points.size(), 0);
    response.trigger_count = authority.trigger_count;
    response.dxf_filename = authority.dxf_filename;
    response.timestamp = authority.timestamp;
    response.planning_report = execution.planning_report;
    response.preview_authority_ready = authority.preview_authority_ready;
    response.preview_authority_shared_with_execution = execution.preview_authority_shared_with_execution;
    response.preview_binding_ready = execution.execution_binding_ready;
    response.preview_spacing_valid = authority.preview_spacing_valid;
    response.preview_has_short_segment_exceptions = authority.preview_has_short_segment_exceptions;
    response.preview_validation_classification = authority.preview_validation_classification;
    response.preview_exception_reason = authority.preview_exception_reason;
    response.preview_failure_reason = execution.execution_failure_reason.empty()
        ? authority.preview_failure_reason
        : execution.execution_failure_reason;
    response.authority_trigger_layout = execution.authority_trigger_layout;
    response.authority_trigger_points = authority.authority_trigger_points;
    response.spacing_validation_groups = authority.spacing_validation_groups;
    response.authority_profile = authority.authority_profile;
    response.execution_profile = execution.execution_profile;
    response.execution_package = execution.execution_package;

    SILIGEN_LOG_INFO_FMT_HELPER(
        "DXF预览数据准备完成: execution_points=%zu, glue_points=%zu, triggers=%d",
        response.execution_trajectory_points.size(),
        response.glue_points.size(),
        response.trigger_count);

    const auto total_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - total_start).count();
    SILIGEN_LOG_INFO_FMT_HELPER("DXF规划流程完成: total_time=%lldms", static_cast<long long>(total_elapsed_ms));

    return Result<PlanningResponse>::Success(std::move(response));
}

std::string PlanningUseCase::BuildAuthorityCacheKey(const PlanningRequest& request) const {
    const auto runtime_params = BuildPreviewRuntimeParams(request, config_port_);
    const auto offset = ResolveDxfOffset(config_port_);
    const auto bounds = ResolveMachineBounds(config_port_);

    Siligen::Shared::Types::DispensingStrategy strategy = Siligen::Shared::Types::DispensingStrategy::BASELINE;
    int subsegment_count = 8;
    bool dispense_only_cruise = false;
    if (config_port_) {
        auto dispensing_result = config_port_->GetDispensingConfig();
        if (dispensing_result.IsSuccess()) {
            const auto& dispensing = dispensing_result.Value();
            strategy = dispensing.strategy;
            if (dispensing.subsegment_count > 0) {
                subsegment_count = dispensing.subsegment_count;
            }
            dispense_only_cruise = dispensing.dispense_only_cruise;
        }
    }

    std::ostringstream oss;
    oss << request.dxf_filepath << '|'
        << request.optimize_path << '|'
        << request.start_x << '|'
        << request.start_y << '|'
        << request.approximate_splines << '|'
        << request.two_opt_iterations << '|'
        << request.spline_max_step_mm << '|'
        << request.spline_max_error_mm << '|'
        << request.continuity_tolerance_mm << '|'
        << request.curve_chain_angle_deg << '|'
        << request.curve_chain_max_segment_mm << '|'
        << request.trajectory_config.max_velocity << '|'
        << request.trajectory_config.max_acceleration << '|'
        << request.trajectory_config.time_step << '|'
        << request.trajectory_config.dispensing_interval << '|'
        << request.trajectory_config.trigger_pulse_us << '|'
        << runtime_params.dispensing_velocity << '|'
        << runtime_params.acceleration << '|'
        << runtime_params.dispenser_interval_ms << '|'
        << runtime_params.dispenser_duration_ms << '|'
        << runtime_params.trigger_spatial_interval_mm << '|'
        << runtime_params.valve_response_ms << '|'
        << runtime_params.safety_margin_ms << '|'
        << runtime_params.min_interval_ms << '|'
        << runtime_params.sample_dt << '|'
        << runtime_params.sample_ds << '|'
        << runtime_params.compensation_profile.open_comp_ms << '|'
        << runtime_params.compensation_profile.close_comp_ms << '|'
        << runtime_params.compensation_profile.retract_enabled << '|'
        << runtime_params.compensation_profile.corner_pulse_scale << '|'
        << runtime_params.compensation_profile.curvature_speed_factor << '|'
        << request.spacing_tol_ratio << '|'
        << request.spacing_min_mm << '|'
        << request.spacing_max_mm << '|'
        << static_cast<int>(strategy) << '|'
        << subsegment_count << '|'
        << dispense_only_cruise << '|'
        << offset.x << '|'
        << offset.y << '|'
        << bounds.enabled << '|'
        << bounds.x_min << '|'
        << bounds.x_max << '|'
        << bounds.y_min << '|'
        << bounds.y_max;
    return oss.str();
}

Result<PreparedAuthorityPreview> PlanningUseCase::PrepareAuthorityPreview(const PlanningRequest& request) {
    const auto total_start = std::chrono::steady_clock::now();
    if (!request.Validate()) {
        return Result<PreparedAuthorityPreview>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid request parameters", "PlanningUseCase"));
    }

    auto file_validation = ValidateFileExists(request.dxf_filepath);
    if (file_validation.IsError()) {
        return Result<PreparedAuthorityPreview>::Failure(file_validation.GetError());
    }

    const auto pb_start = std::chrono::steady_clock::now();
    auto pb_result = pb_preparation_service_->EnsurePbReady(request.dxf_filepath);
    if (pb_result.IsError()) {
        return Result<PreparedAuthorityPreview>::Failure(pb_result.GetError());
    }
    const auto pb_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - pb_start).count();
    const std::string prepared_pb_path = pb_result.Value();

    const auto load_start = std::chrono::steady_clock::now();
    auto path_result = path_source_->LoadFromFile(prepared_pb_path);
    if (path_result.IsError()) {
        return Result<PreparedAuthorityPreview>::Failure(path_result.GetError());
    }
    const auto load_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - load_start).count();

    auto primitives = path_result.Value().primitives;
    if (primitives.empty()) {
        return Result<PreparedAuthorityPreview>::Failure(
            Error(ErrorCode::FILE_FORMAT_INVALID, "DXF无可用路径", "PlanningUseCase"));
    }

    ApplyOffsetToPrimitives(primitives, ResolveDxfOffset(config_port_));
    auto fit_bounds_result = AutoFitPrimitivesIntoMachineBounds(primitives, ResolveMachineBounds(config_port_));
    if (fit_bounds_result.IsError()) {
        return Result<PreparedAuthorityPreview>::Failure(fit_bounds_result.GetError());
    }

    const auto runtime_params = BuildPreviewRuntimeParams(request, config_port_);
    const auto process_path_start = std::chrono::steady_clock::now();
    const auto process_path_request = BuildProcessPathRequest(request, std::move(primitives), config_port_);
    const auto process_path_result = process_path_facade_->Build(process_path_request);
    const auto process_path_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - process_path_start).count();

    const auto authority_start = std::chrono::steady_clock::now();
    auto authority_result = authority_preview_assembly_service_->BuildAuthorityPreviewArtifacts(
        BuildAuthorityPreviewInput(
            request,
            process_path_result,
            runtime_params,
            request.dxf_filepath,
            ExtractFilename(request.dxf_filepath),
            config_port_));
    if (authority_result.IsError()) {
        return Result<PreparedAuthorityPreview>::Failure(authority_result.GetError());
    }
    const auto authority_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - authority_start).count();

    const auto& authority = authority_result.Value();
    PreparedAuthorityPreview prepared;
    prepared.success = true;
    prepared.source_path = request.dxf_filepath;
    prepared.prepared_pb_path = prepared_pb_path;
    prepared.dxf_filename = authority.dxf_filename;
    prepared.process_path = process_path_result.shaped_path;
    prepared.authority_process_path = process_path_result.process_path;
    prepared.discontinuity_count = process_path_result.normalized.report.discontinuity_count;
    prepared.segment_count = authority.segment_count;
    prepared.total_length = authority.total_length;
    prepared.estimated_time = authority.estimated_time;
    prepared.preview_trajectory_points = authority.preview_trajectory_points;
    prepared.glue_points = authority.glue_points;
    prepared.trigger_count = authority.trigger_count;
    prepared.timestamp = authority.timestamp;
    prepared.preview_authority_ready = authority.preview_authority_ready;
    prepared.preview_binding_ready = authority.preview_binding_ready;
    prepared.preview_spacing_valid = authority.preview_spacing_valid;
    prepared.preview_has_short_segment_exceptions = authority.preview_has_short_segment_exceptions;
    prepared.preview_validation_classification = authority.preview_validation_classification;
    prepared.preview_exception_reason = authority.preview_exception_reason;
    prepared.preview_failure_reason = authority.preview_failure_reason;
    prepared.authority_trigger_layout = authority.authority_trigger_layout;
    prepared.authority_trigger_points = authority.authority_trigger_points;
    prepared.spacing_validation_groups = authority.spacing_validation_groups;
    prepared.authority_profile.pb_prepare_ms = static_cast<std::uint32_t>(pb_elapsed_ms);
    prepared.authority_profile.path_load_ms = static_cast<std::uint32_t>(load_elapsed_ms);
    prepared.authority_profile.process_path_ms = static_cast<std::uint32_t>(process_path_elapsed_ms);
    prepared.authority_profile.authority_build_ms = static_cast<std::uint32_t>(authority_elapsed_ms);
    prepared.authority_profile.total_ms = ToElapsedMs(total_start);

    {
        std::ostringstream oss;
        oss << "planning_prepare_authority"
            << " dxf=" << request.dxf_filepath
            << " pb_ms=" << pb_elapsed_ms
            << " load_ms=" << load_elapsed_ms
            << " process_path_ms=" << process_path_elapsed_ms
            << " authority_ms=" << authority_elapsed_ms
            << " glue_points=" << prepared.glue_points.size()
            << " preview_points=" << prepared.preview_trajectory_points.size()
            << " total_ms=" << prepared.authority_profile.total_ms;
        SILIGEN_LOG_INFO(oss.str());
    }

    return Result<PreparedAuthorityPreview>::Success(std::move(prepared));
}

Result<ExecutionAssemblyResponse> PlanningUseCase::AssembleExecutionFromAuthority(
    const PlanningRequest& request,
    const PreparedAuthorityPreview& authority_preview) {
    const auto total_start = std::chrono::steady_clock::now();
    if (!authority_preview.success) {
        return Result<ExecutionAssemblyResponse>::Failure(
            Error(ErrorCode::INVALID_STATE, "authority preview unavailable", "PlanningUseCase"));
    }

    const auto runtime_params = BuildPreviewRuntimeParams(request, config_port_);
    const auto motion_plan_start = std::chrono::steady_clock::now();
    auto motion_plan = motion_planning_facade_->Plan(
        authority_preview.process_path,
        BuildMotionPlanningConfig(request, runtime_params, config_port_));
    PopulatePlanningReport(authority_preview.process_path, authority_preview.discontinuity_count, motion_plan);
    if (motion_plan.points.empty() || motion_plan.planning_report.jerk_plan_failed) {
        return Result<ExecutionAssemblyResponse>::Failure(
            Error(ErrorCode::INVALID_STATE, "轨迹规划失败", "PlanningUseCase"));
    }
    const auto motion_plan_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - motion_plan_start).count();

    auto assembly_input = BuildExecutionAssemblyInput(
        request,
        authority_preview,
        std::move(motion_plan),
        runtime_params);

    const auto assembly_start = std::chrono::steady_clock::now();
    auto assembly_result = execution_assembly_service_->BuildExecutionArtifactsFromAuthority(assembly_input);
    if (assembly_result.IsError()) {
        return Result<ExecutionAssemblyResponse>::Failure(assembly_result.GetError());
    }
    const auto assembly_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - assembly_start).count();

    ExecutionAssemblyResponse response;
    response.success = true;
    response.execution_trajectory_points = assembly_result.Value().execution_trajectory_points;
    response.planning_report = assembly_input.motion_plan.planning_report;
    response.preview_authority_shared_with_execution =
        assembly_result.Value().preview_authority_shared_with_execution;
    response.execution_binding_ready = assembly_result.Value().execution_binding_ready;
    response.execution_failure_reason = assembly_result.Value().execution_failure_reason;
    response.execution_package =
        std::make_shared<Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated>(
            assembly_result.Value().execution_package);
    response.authority_trigger_layout = assembly_result.Value().authority_trigger_layout;
    Siligen::Application::Services::Dispensing::PlanningArtifactExportAssemblyService export_assembly_service;
    Siligen::Application::Services::Dispensing::PlanningArtifactExportAssemblyInput export_input;
    export_input.source_path = authority_preview.source_path;
    export_input.dxf_filename = authority_preview.dxf_filename;
    export_input.process_path = authority_preview.process_path;
    export_input.glue_points = authority_preview.glue_points;
    export_input.execution_trajectory_points = response.execution_trajectory_points;
    export_input.interpolation_trajectory_points = assembly_result.Value().interpolation_trajectory_points;
    export_input.motion_trajectory_points = assembly_result.Value().motion_trajectory_points;
    response.export_request = export_assembly_service.BuildRequest(export_input);
    response.execution_profile.motion_plan_ms = static_cast<std::uint32_t>(motion_plan_elapsed_ms);
    response.execution_profile.assembly_ms = static_cast<std::uint32_t>(assembly_elapsed_ms);

    std::uint32_t export_elapsed_ms = 0;
    if (artifact_export_port_) {
        const auto export_start = std::chrono::steady_clock::now();
        auto export_result = artifact_export_port_->Export(response.export_request);
        export_elapsed_ms = ToElapsedMs(export_start);
        if (export_result.IsError()) {
            SILIGEN_LOG_WARNING("planning artifact export failed: " + export_result.GetError().GetMessage());
        } else if (export_result.Value().export_requested && !export_result.Value().success) {
            SILIGEN_LOG_WARNING("planning artifact export reported failure: " + export_result.Value().message);
        }
    }
    response.execution_profile.export_ms = export_elapsed_ms;
    response.execution_profile.total_ms = ToElapsedMs(total_start);

    {
        std::ostringstream oss;
        oss << "planning_assemble_execution"
            << " dxf=" << authority_preview.source_path
            << " motion_plan_ms=" << motion_plan_elapsed_ms
            << " assembly_ms=" << assembly_elapsed_ms
            << " export_ms=" << export_elapsed_ms
            << " execution_points=" << response.execution_trajectory_points.size()
            << " binding_ready=" << (response.execution_binding_ready ? 1 : 0)
            << " total_ms=" << response.execution_profile.total_ms;
        SILIGEN_LOG_INFO(oss.str());
    }

    return Result<ExecutionAssemblyResponse>::Success(std::move(response));
}

Result<void> PlanningUseCase::ValidateFileExists(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.good()) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_NOT_FOUND, "File not found: " + filepath, "PlanningUseCase"));
    }
    return Result<void>::Success();
}

std::string PlanningUseCase::ExtractFilename(const std::string& filepath) {
    const auto slash_pos = filepath.find_last_of("/\\");
    if (slash_pos == std::string::npos || slash_pos + 1 >= filepath.size()) {
        return filepath;
    }
    return filepath.substr(slash_pos + 1);
}

}  // namespace Siligen::Application::UseCases::Dispensing

