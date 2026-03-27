#include "PlanningUseCase.h"

#include "application/services/dispensing/PlanningPreviewAssemblyService.h"
#include "application/services/dxf/DxfPbPreparationService.h"
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
    response.stage_state.stage_name = "M0->M4-planning-trigger";
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

DispensingPlanRequest BuildPlanRequest(
    const PlanningRequest& request,
    const PreviewRuntimeParams& runtime_params,
    const std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort>& config_port) {
    DispensingPlanRequest plan_request;
    plan_request.dxf_filepath = request.dxf_filepath;
    plan_request.optimize_path = request.optimize_path;
    plan_request.start_x = request.start_x;
    plan_request.start_y = request.start_y;
    plan_request.approximate_splines = request.approximate_splines;
    plan_request.two_opt_iterations = request.two_opt_iterations;
    plan_request.spline_max_step_mm = request.spline_max_step_mm;
    plan_request.spline_max_error_mm = request.spline_max_error_mm;
    plan_request.continuity_tolerance_mm = request.continuity_tolerance_mm;
    plan_request.curve_chain_angle_deg = request.curve_chain_angle_deg;
    plan_request.curve_chain_max_segment_mm = request.curve_chain_max_segment_mm;
    plan_request.use_hardware_trigger = request.use_hardware_trigger;
    plan_request.dispensing_velocity = runtime_params.dispensing_velocity;
    plan_request.acceleration = runtime_params.acceleration;
    plan_request.dispenser_interval_ms = runtime_params.dispenser_interval_ms;
    plan_request.dispenser_duration_ms = runtime_params.dispenser_duration_ms;
    plan_request.trigger_spatial_interval_mm = runtime_params.trigger_spatial_interval_mm;
    plan_request.pulse_per_mm = runtime_params.pulse_per_mm;
    plan_request.valve_response_ms = runtime_params.valve_response_ms;
    plan_request.safety_margin_ms = runtime_params.safety_margin_ms;
    plan_request.min_interval_ms = runtime_params.min_interval_ms;
    plan_request.compensation_profile = runtime_params.compensation_profile;
    plan_request.sample_dt = runtime_params.sample_dt;
    plan_request.sample_ds = runtime_params.sample_ds;
    plan_request.arc_tolerance_mm = request.trajectory_config.arc_tolerance;
    plan_request.max_jerk = request.trajectory_config.max_jerk;
    plan_request.use_interpolation_planner = request.use_interpolation_planner;
    plan_request.interpolation_algorithm = request.interpolation_algorithm;

    if (!config_port) {
        return plan_request;
    }

    auto system_result = config_port->LoadConfiguration();
    if (system_result.IsSuccess()) {
        const auto& config = system_result.Value();
        plan_request.dxf_offset = Point2D(config.dxf.offset_x, config.dxf.offset_y);
    } else {
        SILIGEN_LOG_WARNING("读取系统配置失败: " + system_result.GetError().GetMessage());
    }

    auto dispensing_result = config_port->GetDispensingConfig();
    if (dispensing_result.IsSuccess()) {
        const auto& disp = dispensing_result.Value();
        plan_request.dispensing_strategy = disp.strategy;
        if (disp.subsegment_count > 0) {
            plan_request.subsegment_count = disp.subsegment_count;
        }
        plan_request.dispense_only_cruise = disp.dispense_only_cruise;
        plan_request.start_speed_factor = disp.start_speed_factor;
        plan_request.end_speed_factor = disp.end_speed_factor;
        plan_request.corner_speed_factor = disp.corner_speed_factor;
        plan_request.rapid_speed_factor = disp.rapid_speed_factor;
    }

    auto traj_result = config_port->GetDxfTrajectoryConfig();
    if (traj_result.IsSuccess()) {
        const auto& traj = traj_result.Value();
        plan_request.python_ruckig_python = traj.python;
        plan_request.python_ruckig_script = traj.script;
    }

    return plan_request;
}

float32 ResolveEstimatedTime(
    const Siligen::Domain::Dispensing::DomainServices::DispensingPlan& plan,
    float32 planned_estimated_time_s) {
    if (planned_estimated_time_s > kEpsilon) {
        return planned_estimated_time_s;
    }
    if (plan.estimated_time_s > kEpsilon) {
        return plan.estimated_time_s;
    }
    return plan.motion_trajectory.total_time;
}

Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated BuildExecutionPackage(
    const Siligen::Domain::Dispensing::DomainServices::DispensingPlan& plan,
    const std::string& source_path,
    float32 planned_estimated_time_s) {
    using Siligen::Domain::Dispensing::Contracts::ExecutionPackageBuilt;
    using Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated;

    ExecutionPackageBuilt built;
    built.execution_plan.interpolation_segments = plan.interpolation_segments;
    built.execution_plan.interpolation_points = plan.interpolation_points;
    built.execution_plan.motion_trajectory = plan.motion_trajectory;
    built.execution_plan.trigger_distances_mm = plan.trigger_distances_mm;
    built.execution_plan.trigger_interval_ms = plan.trigger_interval_ms;
    built.execution_plan.trigger_interval_mm = plan.trigger_interval_mm;
    built.execution_plan.total_length_mm = plan.total_length_mm;
    built.total_length_mm = plan.total_length_mm > kEpsilon ? plan.total_length_mm : plan.motion_trajectory.total_length;
    built.estimated_time_s = ResolveEstimatedTime(plan, planned_estimated_time_s);
    built.source_path = source_path;
    return ExecutionPackageValidated(built);
}

}  // namespace

PlanningUseCase::PlanningUseCase(
    std::shared_ptr<DispensingPlanner> planner,
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port,
    std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService> pb_preparation_service,
    std::shared_ptr<Siligen::Application::Services::Dispensing::IPlanningArtifactExportPort> artifact_export_port)
    : planner_(std::move(planner)),
      config_port_(std::move(config_port)),
      pb_preparation_service_(pb_preparation_service
                                  ? std::move(pb_preparation_service)
                                  : std::make_shared<Siligen::Application::Services::DXF::DxfPbPreparationService>(
                                        config_port_)),
      artifact_export_port_(std::move(artifact_export_port)) {
    if (!planner_) {
        throw std::invalid_argument("PlanningUseCase: planner cannot be null");
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

    const auto runtime_params = BuildPreviewRuntimeParams(request, config_port_);
    auto plan_request = BuildPlanRequest(request, runtime_params, config_port_);
    plan_request.dxf_filepath = prepared_pb_path;

    const auto plan_start = std::chrono::steady_clock::now();
    auto plan_result = planner_->Plan(plan_request);
    if (plan_result.IsError()) {
        const auto trigger_response = BuildWorkflowPlanningTriggerResponse(
            trigger_request,
            WorkflowStageLifecycle::Failed,
            WorkflowFailureCategory::PlanningUnavailable,
            prepared_pb_path,
            plan_result.GetError().GetMessage());
        SILIGEN_LOG_WARNING(
            "workflow planning trigger failed during plan assembly: " + trigger_response.recovery_directive.reason);
        return Result<PlanningResponse>::Failure(plan_result.GetError());
    }
    const auto plan_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - plan_start).count();

    const auto& plan = plan_result.Value();
    SILIGEN_LOG_INFO_FMT_HELPER(
        "DXF规划完成: segments=%zu, time=%lldms",
        plan.process_path.segments.size(),
        static_cast<long long>(plan_elapsed_ms));

    Siligen::Application::Services::Dispensing::PlanningPreviewAssemblyService preview_assembly_service;
    auto response_result = preview_assembly_service.BuildResponse(
        request,
        plan,
        plan_request,
        ExtractFilename(request.dxf_filepath));
    if (response_result.IsError()) {
        const auto trigger_response = BuildWorkflowPlanningTriggerResponse(
            trigger_request,
            WorkflowStageLifecycle::Failed,
            WorkflowFailureCategory::BoundaryViolation,
            prepared_pb_path,
            response_result.GetError().GetMessage());
        SILIGEN_LOG_WARNING(
            "workflow planning trigger failed during preview assembly: " + trigger_response.recovery_directive.reason);
        return Result<PlanningResponse>::Failure(response_result.GetError());
    }

    auto assembled = response_result.Value();
    auto response = std::move(assembled.response);
    response.execution_package =
        std::make_shared<Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated>(
            BuildExecutionPackage(plan, request.dxf_filepath, response.estimated_time));
    response.execution_plan = std::make_shared<DispensingPlan>(plan);

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
        "DXF预览数据准备完成: points=%zu, triggers=%d",
        response.trajectory_points.size(),
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
