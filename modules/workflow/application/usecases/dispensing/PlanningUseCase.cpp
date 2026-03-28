#include "PlanningUseCase.h"

#include "application/services/dxf/DxfPbPreparationService.h"
#include "domain/trajectory/value-objects/GeometryUtils.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "workflow/contracts/WorkflowContracts.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <utility>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "PlanningUseCase"

namespace Siligen::Application::UseCases::Dispensing {

using Siligen::Application::Services::Dispensing::PlanningArtifactsBuildInput;
using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
using Siligen::Application::Services::ProcessPath::ProcessPathBuildResult;
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

void PopulatePlanningReport(const ProcessPathBuildResult& path_result, MotionPlan& motion_plan) {
    auto& report = motion_plan.planning_report;
    report.segment_count = static_cast<int32>(path_result.shaped_path.segments.size());
    report.discontinuity_count = path_result.normalized.report.discontinuity_count;

    int32 rapid_count = 0;
    int32 corner_count = 0;
    float32 rapid_length = 0.0f;
    for (const auto& segment : path_result.shaped_path.segments) {
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

PlanningArtifactsBuildInput BuildPlanningArtifactsInput(
    const PlanningRequest& request,
    const ProcessPathBuildResult& path_result,
    MotionPlan motion_plan,
    const PreviewRuntimeParams& runtime_params,
    const std::string& source_path,
    const std::string& dxf_filename,
    const std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort>& config_port) {
    PlanningArtifactsBuildInput input;
    input.process_path = path_result.shaped_path;
    input.motion_plan = std::move(motion_plan);
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
    input.max_jerk = request.trajectory_config.max_jerk;
    input.sample_dt = runtime_params.sample_dt;
    input.sample_ds = runtime_params.sample_ds;
    input.spline_max_step_mm = request.spline_max_step_mm;
    input.spline_max_error_mm = request.spline_max_error_mm;
    input.estimated_time_s = motion_plan.total_time;
    input.use_interpolation_planner = request.use_interpolation_planner;
    input.interpolation_algorithm = request.interpolation_algorithm;
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

}  // namespace

PlanningUseCase::PlanningUseCase(
    std::shared_ptr<Siligen::Domain::Trajectory::Ports::IPathSourcePort> path_source,
    std::shared_ptr<Siligen::Application::Services::ProcessPath::ProcessPathFacade> process_path_facade,
    std::shared_ptr<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>
        motion_planning_facade,
    std::shared_ptr<Siligen::Application::Services::Dispensing::DispensePlanningFacade>
        dispense_planning_facade,
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
      dispense_planning_facade_(dispense_planning_facade
                                    ? std::move(dispense_planning_facade)
                                    : std::make_shared<Siligen::Application::Services::Dispensing::DispensePlanningFacade>()),
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
    const auto trigger_request = BuildWorkflowPlanningTriggerRequest(request);

    if (!request.Validate()) {
        return Result<PlanningResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid request parameters", "PlanningUseCase"));
    }

    auto file_validation = ValidateFileExists(request.dxf_filepath);
    if (file_validation.IsError()) {
        return Result<PlanningResponse>::Failure(file_validation.GetError());
    }

    auto pb_result = pb_preparation_service_->EnsurePbReady(request.dxf_filepath);
    if (pb_result.IsError()) {
        const auto trigger_response = BuildWorkflowPlanningTriggerResponse(
            trigger_request,
            WorkflowStageLifecycle::Failed,
            WorkflowFailureCategory::InputPreparation,
            "",
            pb_result.GetError().GetMessage());
        SILIGEN_LOG_WARNING(
            "workflow planning trigger failed during input preparation: " + trigger_response.recovery_directive.reason);
        return Result<PlanningResponse>::Failure(pb_result.GetError());
    }
    const std::string prepared_pb_path = pb_result.Value();

    auto path_result = path_source_->LoadFromFile(prepared_pb_path);
    if (path_result.IsError()) {
        const auto trigger_response = BuildWorkflowPlanningTriggerResponse(
            trigger_request,
            WorkflowStageLifecycle::Failed,
            WorkflowFailureCategory::PlanningUnavailable,
            prepared_pb_path,
            path_result.GetError().GetMessage());
        SILIGEN_LOG_WARNING(
            "workflow planning trigger failed during path load: " + trigger_response.recovery_directive.reason);
        return Result<PlanningResponse>::Failure(path_result.GetError());
    }

    auto primitives = path_result.Value().primitives;
    if (primitives.empty()) {
        return Result<PlanningResponse>::Failure(
            Error(ErrorCode::FILE_FORMAT_INVALID, "DXF无可用路径", "PlanningUseCase"));
    }

    ApplyOffsetToPrimitives(primitives, ResolveDxfOffset(config_port_));

    const auto runtime_params = BuildPreviewRuntimeParams(request, config_port_);
    const auto process_path_request = BuildProcessPathRequest(request, std::move(primitives), config_port_);
    const auto process_path_result = process_path_facade_->Build(process_path_request);

    const auto plan_start = std::chrono::steady_clock::now();
    auto motion_plan = motion_planning_facade_->Plan(
        process_path_result.shaped_path,
        BuildMotionPlanningConfig(request, runtime_params, config_port_));
    PopulatePlanningReport(process_path_result, motion_plan);
    if (motion_plan.points.empty() || motion_plan.planning_report.jerk_plan_failed) {
        const auto trigger_response = BuildWorkflowPlanningTriggerResponse(
            trigger_request,
            WorkflowStageLifecycle::Failed,
            WorkflowFailureCategory::PlanningUnavailable,
            prepared_pb_path,
            "motion planning failed");
        SILIGEN_LOG_WARNING(
            "workflow planning trigger failed during motion planning: " + trigger_response.recovery_directive.reason);
        return Result<PlanningResponse>::Failure(
            Error(ErrorCode::INVALID_STATE, "轨迹规划失败", "PlanningUseCase"));
    }
    const auto plan_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - plan_start).count();
    SILIGEN_LOG_INFO_FMT_HELPER(
        "DXF规划完成: segments=%zu, time=%lldms",
        process_path_result.shaped_path.segments.size(),
        static_cast<long long>(plan_elapsed_ms));

    auto assembly_result = dispense_planning_facade_->AssemblePlanningArtifacts(
        BuildPlanningArtifactsInput(
            request,
            process_path_result,
            std::move(motion_plan),
            runtime_params,
            request.dxf_filepath,
            ExtractFilename(request.dxf_filepath),
            config_port_));
    if (assembly_result.IsError()) {
        const auto trigger_response = BuildWorkflowPlanningTriggerResponse(
            trigger_request,
            WorkflowStageLifecycle::Failed,
            WorkflowFailureCategory::BoundaryViolation,
            prepared_pb_path,
            assembly_result.GetError().GetMessage());
        SILIGEN_LOG_WARNING(
            "workflow planning trigger failed during package assembly: " + trigger_response.recovery_directive.reason);
        return Result<PlanningResponse>::Failure(assembly_result.GetError());
    }

    auto assembled = assembly_result.Value();
    PlanningResponse response;
    response.success = true;
    response.segment_count = assembled.segment_count;
    response.total_length = assembled.total_length;
    response.estimated_time = assembled.estimated_time;
    response.execution_trajectory_points = assembled.trajectory_points;
    response.glue_points = assembled.glue_points;
    response.process_tags.assign(response.execution_trajectory_points.size(), 0);
    response.trigger_count = assembled.trigger_count;
    response.dxf_filename = assembled.dxf_filename;
    response.timestamp = assembled.timestamp;
    response.planning_report = assembled.planning_report;
    response.execution_package =
        std::make_shared<Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated>(
            assembled.execution_package);

    if (artifact_export_port_) {
        auto export_result = artifact_export_port_->Export(assembled.export_request);
        if (export_result.IsError()) {
            SILIGEN_LOG_WARNING("planning artifact export failed: " + export_result.GetError().GetMessage());
        } else if (export_result.Value().export_requested && !export_result.Value().success) {
            SILIGEN_LOG_WARNING("planning artifact export reported failure: " + export_result.Value().message);
        }
    }

    const auto trigger_response = BuildWorkflowPlanningTriggerResponse(
        trigger_request,
        WorkflowStageLifecycle::Completed,
        WorkflowFailureCategory::None,
        prepared_pb_path,
        "");
    SILIGEN_LOG_INFO("workflow planning trigger accepted: prepared_input=" + trigger_response.prepared_input_path);
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
