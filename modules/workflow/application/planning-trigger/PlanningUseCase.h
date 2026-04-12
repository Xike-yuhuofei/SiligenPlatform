#pragma once

#include "../ports/dispensing/PlanningPorts.h"
#include "../services/dispensing/WorkflowPlanningAssemblyTypes.h"
#include "workflow/contracts/WorkflowContracts.h"
#include "runtime/contracts/system/IEventPublisherPort.h"
#include "motion_planning/contracts/MotionPlanningReport.h"
#include "motion_planning/contracts/InterpolationTypes.h"
#include "domain/dispensing/value-objects/AuthorityTriggerLayout.h"
#include "domain/dispensing/value-objects/DispenseCompensationProfile.h"
#include "domain/dispensing/contracts/ExecutionPackage.h"
#include "domain/dispensing/contracts/PlanningArtifactExportRequest.h"
#include "process_path/contracts/ProcessPath.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "shared/types/TrajectoryTypes.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Siligen::ProcessPath::Contracts {
class IPathSourcePort;
}

namespace Siligen::Domain::Configuration::Ports {
class IConfigurationPort;
}

namespace Siligen::Domain::Diagnostics::Ports {
class IDiagnosticsPort;
}

namespace Siligen::Application::Services::Dispensing {
class IWorkflowPlanningAssemblyOperations;
class IPlanningArtifactExportPort;
}

namespace Siligen::Application::UseCases::Dispensing {

using Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated;
using Siligen::MotionPlanning::Contracts::MotionPlanningReport;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::TrajectoryConfig;
using Siligen::TrajectoryPoint;

struct AuthorityProfile {
    std::uint32_t pb_prepare_ms = 0;
    std::uint32_t path_load_ms = 0;
    std::uint32_t process_path_ms = 0;
    std::uint32_t authority_build_ms = 0;
    std::uint32_t total_ms = 0;
};

struct ExecutionProfile {
    std::uint32_t motion_plan_ms = 0;
    std::uint32_t assembly_ms = 0;
    std::uint32_t export_ms = 0;
    std::uint32_t total_ms = 0;
};

struct PreparedAuthorityPreview {
    bool success = false;
    std::string source_path;
    std::string prepared_pb_path;
    Siligen::ProcessPath::Contracts::ProcessPath process_path;
    Siligen::ProcessPath::Contracts::ProcessPath authority_process_path;
    int discontinuity_count = 0;
    std::string preview_diagnostic_code;
    Siligen::Application::Services::Dispensing::WorkflowAuthorityPreviewArtifacts artifacts;
    AuthorityProfile authority_profile;
};

struct ExecutionAssemblyResponse {
    bool success = false;
    std::vector<TrajectoryPoint> execution_trajectory_points;
    std::vector<TrajectoryPoint> motion_trajectory_points;
    MotionPlanningReport planning_report;
    bool preview_authority_shared_with_execution = false;
    bool execution_binding_ready = false;
    std::string execution_failure_reason;
    std::shared_ptr<ExecutionPackageValidated> execution_package;
    Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout authority_trigger_layout;
    Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportRequest export_request;
    ExecutionProfile execution_profile;
    int diagnostic_error_code = 0;
    std::string diagnostic_message;
    std::int64_t observed_at_ms = 0;
    bool export_attempted = false;
    bool export_succeeded = false;
};

struct PlanningRuntimeParams {
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

struct ExecutionAssemblyArtifacts {
    bool success = false;
    MotionPlanningReport planning_report;
    std::shared_ptr<ExecutionPackageValidated> execution_package;
    Siligen::Application::Services::Dispensing::WorkflowExecutionAssemblyResult artifacts;
};

struct PlanningRequest {
    std::string dxf_filepath;
    TrajectoryConfig trajectory_config;
    bool optimize_path = true;
    float32 start_x = 0.0f;
    float32 start_y = 0.0f;
    bool approximate_splines = false;
    int two_opt_iterations = 0;
    float32 spline_max_step_mm = 0.0f;
    float32 spline_max_error_mm = 0.0f;
    float32 continuity_tolerance_mm = 0.0f;
    float32 curve_chain_angle_deg = 0.0f;
    float32 curve_chain_max_segment_mm = 0.0f;
    bool use_hardware_trigger = true;
    bool use_interpolation_planner = false;
    MotionPlanning::Contracts::InterpolationAlgorithm interpolation_algorithm =
        MotionPlanning::Contracts::InterpolationAlgorithm::LINEAR;
    float32 spacing_tol_ratio = 0.0f;
    float32 spacing_min_mm = 0.0f;
    float32 spacing_max_mm = 0.0f;

    bool Validate() const {
        if (dxf_filepath.empty()) {
            return false;
        }

        if (trajectory_config.max_velocity <= 0.0f ||
            trajectory_config.max_velocity > 1000.0f) {
            return false;
        }

        if (trajectory_config.max_acceleration <= 0.0f ||
            trajectory_config.max_acceleration > 10000.0f) {
            return false;
        }

        if (trajectory_config.time_step <= 0.0f ||
            trajectory_config.time_step > 0.1f) {
            return false;
        }
        if (trajectory_config.arc_tolerance < 0.0f) {
            return false;
        }
        if (use_interpolation_planner &&
            (interpolation_algorithm == MotionPlanning::Contracts::InterpolationAlgorithm::LINEAR ||
             interpolation_algorithm == MotionPlanning::Contracts::InterpolationAlgorithm::CMP_COORDINATED) &&
            trajectory_config.max_jerk <= 0.0f) {
            return false;
        }
        if (spacing_tol_ratio < 0.0f || spacing_tol_ratio > 1.0f) {
            return false;
        }
        if (spacing_min_mm < 0.0f || spacing_max_mm < 0.0f) {
            return false;
        }
        if (spacing_min_mm > 0.0f && spacing_max_mm > 0.0f && spacing_min_mm > spacing_max_mm) {
            return false;
        }
        if (continuity_tolerance_mm < 0.0f) {
            return false;
        }
        if (curve_chain_angle_deg < 0.0f || curve_chain_angle_deg > 180.0f) {
            return false;
        }
        if (curve_chain_max_segment_mm < 0.0f) {
            return false;
        }

        return true;
    }
};

struct PlanningResponse {
    bool success = false;
    std::string error_message;

    int segment_count = 0;
    float32 total_length = 0.0f;
    float32 estimated_time = 0.0f;

    std::vector<TrajectoryPoint> execution_trajectory_points;
    std::vector<Siligen::Shared::Types::Point2D> glue_points;
    std::vector<int32> process_tags;

    int trigger_count = 0;

    std::string dxf_filename;
    int32 timestamp = 0;

    MotionPlanningReport planning_report;

    bool preview_authority_ready = false;
    bool preview_authority_shared_with_execution = false;
    bool preview_binding_ready = false;
    bool preview_spacing_valid = false;
    bool preview_has_short_segment_exceptions = false;
    std::string preview_validation_classification;
    std::string preview_exception_reason;
    std::string preview_failure_reason;
    std::string preview_diagnostic_code;
    Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout authority_trigger_layout;
    std::vector<Siligen::Application::Services::Dispensing::WorkflowAuthorityTriggerPoint> authority_trigger_points;
    std::vector<Siligen::Application::Services::Dispensing::WorkflowSpacingValidationGroup> spacing_validation_groups;
    AuthorityProfile authority_profile;
    ExecutionProfile execution_profile;

    std::shared_ptr<ExecutionPackageValidated> execution_package;
    int diagnostic_error_code = 0;
    std::string diagnostic_message;
    std::int64_t observed_at_ms = 0;
    bool export_attempted = false;
    bool export_succeeded = false;
};

struct PlanningUseCaseInternalAccess;

class PlanningUseCase {
public:
    PlanningUseCase(
        std::shared_ptr<Siligen::ProcessPath::Contracts::IPathSourcePort> path_source,
        std::shared_ptr<Siligen::Application::Ports::Dispensing::IProcessPathBuildPort> process_path_port,
        std::shared_ptr<Siligen::Application::Ports::Dispensing::IMotionPlanningPort> motion_planning_port,
        std::shared_ptr<Siligen::Application::Services::Dispensing::IWorkflowPlanningAssemblyOperations> planning_operations,
        std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port = nullptr,
        std::shared_ptr<Siligen::Application::Ports::Dispensing::IPlanningInputPreparationPort>
            planning_input_preparation_port = nullptr,
        std::shared_ptr<Siligen::Application::Services::Dispensing::IPlanningArtifactExportPort>
            artifact_export_port = nullptr,
        std::shared_ptr<Siligen::Domain::Diagnostics::Ports::IDiagnosticsPort> diagnostics_port = nullptr,
        std::shared_ptr<Siligen::Domain::System::Ports::IEventPublisherPort> event_publisher_port = nullptr);

    ~PlanningUseCase() = default;

    PlanningUseCase(const PlanningUseCase&) = delete;
    PlanningUseCase& operator=(const PlanningUseCase&) = delete;
    PlanningUseCase(PlanningUseCase&&) = delete;
    PlanningUseCase& operator=(PlanningUseCase&&) = delete;

    Result<PlanningResponse> Execute(const PlanningRequest& request);
    std::string BuildAuthorityCacheKey(const PlanningRequest& request) const;

private:
    friend struct PlanningUseCaseInternalAccess;

    Result<PreparedAuthorityPreview> PrepareAuthorityPreview(const PlanningRequest& request);
    Result<ExecutionAssemblyResponse> AssembleExecutionFromAuthority(
        const PlanningRequest& request,
        const PreparedAuthorityPreview& authority_preview);

    std::shared_ptr<Siligen::ProcessPath::Contracts::IPathSourcePort> path_source_;
    std::shared_ptr<Siligen::Application::Ports::Dispensing::IProcessPathBuildPort> process_path_port_;
    std::shared_ptr<Siligen::Application::Ports::Dispensing::IMotionPlanningPort> motion_planning_port_;
    std::shared_ptr<Siligen::Application::Services::Dispensing::IWorkflowPlanningAssemblyOperations> planning_operations_;
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port_;
    std::shared_ptr<Siligen::Application::Ports::Dispensing::IPlanningInputPreparationPort>
        planning_input_preparation_port_;
    std::shared_ptr<Siligen::Application::Services::Dispensing::IPlanningArtifactExportPort> artifact_export_port_;
    std::shared_ptr<Siligen::Domain::Diagnostics::Ports::IDiagnosticsPort> diagnostics_port_;
    std::shared_ptr<Siligen::Domain::System::Ports::IEventPublisherPort> event_publisher_port_;

    std::string ExtractFilename(const std::string& filepath);
    void RecordPlanningDiagnostic(
        Siligen::Workflow::Contracts::WorkflowFailureCategory failure_category,
        int error_code,
        const std::string& message,
        const std::string& workflow_run_id,
        const std::string& source_path,
        bool export_attempted,
        bool export_succeeded,
        std::uint32_t stage_duration_ms) const;
};

}  // namespace Siligen::Application::UseCases::Dispensing
