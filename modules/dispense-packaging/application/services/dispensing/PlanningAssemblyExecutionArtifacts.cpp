#include "application/services/dispensing/PlanningAssemblyResidualInternals.h"

#include "runtime_execution/contracts/dispensing/ProfileCompareExecutionCompiler.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <sstream>

namespace Siligen::Application::Services::Dispensing::Internal {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

namespace {

Result<void> ValidateExecutionStrategyInput(
    DispensingExecutionStrategy requested_strategy,
    DispensingExecutionGeometryKind geometry_kind) {
    if (geometry_kind != DispensingExecutionGeometryKind::POINT &&
        requested_strategy == DispensingExecutionStrategy::STATIONARY_SHOT) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "requested_execution_strategy=stationary_shot only supports POINT geometry",
            "ExecutionAssemblyService"));
    }

    return Result<void>::Success();
}

Result<void> ValidateFormalProfileCompareContract(
    const ExecutionPackageValidated& execution_package,
    const Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareRuntimeContract& runtime_contract) {
    const auto& execution_plan = execution_package.execution_plan;
    if (execution_plan.production_trigger_mode !=
        Siligen::Domain::Dispensing::ValueObjects::ProductionTriggerMode::PROFILE_COMPARE) {
        return Result<void>::Success();
    }

    const auto compile_result =
        Siligen::RuntimeExecution::Contracts::Dispensing::CompileProfileCompareExecutionSchedule(
            {execution_plan, runtime_contract});
    if (compile_result.IsSuccess()) {
        return Result<void>::Success();
    }

    std::string detail = "owner execution package 不满足 formal runtime compare contract";
    if (!compile_result.GetError().GetMessage().empty()) {
        detail += ": " + compile_result.GetError().GetMessage();
    }
    return Result<void>::Failure(Error(
        compile_result.GetError().GetCode(),
        detail,
        "ExecutionAssemblyService"));
}

}  // namespace

Result<ExecutionAssemblyBuildResult> AssembleExecutionArtifacts(const ExecutionAssemblyBuildInput& input) {
    const auto& execution_process_path = ResolveExecutionProcessPath(input);
    const auto geometry_kind = ResolveExecutionGeometryKind(execution_process_path);
    auto log_stage = [&](const char* stage, const std::string& detail = std::string()) {
        std::ostringstream oss;
        oss << "planning_artifacts_stage=" << stage
            << " dxf=" << input.dxf_filename
            << " authority_segments=" << input.authority_process_path.segments.size()
            << " canonical_execution_segments=" << input.canonical_execution_process_path.segments.size()
            << " execution_segments=" << execution_process_path.segments.size()
            << " motion_points=" << input.motion_plan.points.size()
            << " preview_layout=" << input.authority_preview.authority_trigger_layout.layout_id;
        if (!detail.empty()) {
            oss << ' ' << detail;
        }
        SILIGEN_LOG_INFO(oss.str());
    };

    log_stage("execution_assembly_start");

    if (execution_process_path.segments.empty()) {
        return Result<ExecutionAssemblyBuildResult>::Failure(
            Error(
                ErrorCode::INVALID_PARAMETER,
                "canonical execution process path为空",
                "DispensePackagingAssembly"));
    }
    if (input.motion_plan.points.empty()) {
        return Result<ExecutionAssemblyBuildResult>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "motion plan为空", "DispensePackagingAssembly"));
    }

    auto strategy_validation =
        ValidateExecutionStrategyInput(input.requested_execution_strategy, geometry_kind);
    if (strategy_validation.IsError()) {
        return Result<ExecutionAssemblyBuildResult>::Failure(strategy_validation.GetError());
    }

    auto trigger_artifacts = BuildTriggerArtifactsFromAuthorityPreview(input.authority_preview);
    {
        std::ostringstream oss;
        oss << "authority_points=" << trigger_artifacts.authority_trigger_points.size()
            << " spacing_valid=" << (trigger_artifacts.spacing_valid ? 1 : 0)
            << " failure_reason=" << trigger_artifacts.failure_reason;
        log_stage("authority_preview_loaded", oss.str());
    }

    auto generation_result =
        BuildExecutionGenerationArtifacts(input, execution_process_path, trigger_artifacts);
    if (generation_result.IsError()) {
        return Result<ExecutionAssemblyBuildResult>::Failure(generation_result.GetError());
    }
    auto generation_artifacts = std::move(generation_result.Value());
    {
        std::ostringstream oss;
        oss << "interpolation_points=" << generation_artifacts.interpolation_points.size()
            << " interpolation_segments=" << generation_artifacts.interpolation_segments.size();
        log_stage("execution_generation_ready", oss.str());
    }

    auto execution_package_result = BuildValidatedExecutionPackage(
        input,
        execution_process_path,
        trigger_artifacts,
        std::move(generation_artifacts));
    if (execution_package_result.IsError()) {
        return Result<ExecutionAssemblyBuildResult>::Failure(execution_package_result.GetError());
    }
    auto execution_package = std::move(execution_package_result.Value());

    ExecutionAssemblyBuildResult result;
    bool authority_shared = false;
    PopulateExecutionTrajectorySelection(execution_package, result, authority_shared);
    {
        std::ostringstream oss;
        oss << "authority_shared=" << (authority_shared ? 1 : 0)
            << " execution_points=" << result.execution_trajectory_points.size();
        log_stage("execution_trajectory_selected", oss.str());
    }

    const auto* execution_trajectory =
        result.execution_trajectory_points.empty() ? nullptr : &result.execution_trajectory_points;
    BindAuthorityLayoutToExecutionTrajectory(trigger_artifacts, execution_trajectory);
    {
        std::ostringstream oss;
        oss << "binding_ready=" << (trigger_artifacts.binding_ready ? 1 : 0)
            << " bindings=" << trigger_artifacts.authority_trigger_layout.bindings.size()
            << " failure_reason=" << trigger_artifacts.failure_reason;
        log_stage("execution_binding_resolved", oss.str());
    }

    if (!trigger_artifacts.binding_ready &&
        input.authority_preview.preview_authority_ready &&
        input.authority_preview.authority_trigger_layout.authority_ready) {
        const std::string failure_reason = trigger_artifacts.failure_reason.empty()
            ? "authority trigger binding unavailable"
            : trigger_artifacts.failure_reason;
        return Result<ExecutionAssemblyBuildResult>::Failure(
            Error(ErrorCode::INVALID_STATE, failure_reason, "DispensePackagingAssembly"));
    }

    result.execution_package = std::move(execution_package);
    result.preview_authority_shared_with_execution = trigger_artifacts.binding_ready;
    result.execution_binding_ready = trigger_artifacts.binding_ready;
    result.execution_contract_ready = trigger_artifacts.binding_ready;
    result.execution_failure_reason = trigger_artifacts.failure_reason;
    result.authority_trigger_layout = trigger_artifacts.authority_trigger_layout;

    if (result.execution_binding_ready) {
        Siligen::Domain::Dispensing::Contracts::FormalCompareGateDiagnostic formal_compare_gate;
        auto profiled_package_result = BuildFormalProfileCompareExecutionPackage(
            result.execution_package,
            trigger_artifacts,
            input.profile_compare_runtime_contract,
            &formal_compare_gate);
        if (profiled_package_result.IsError()) {
            result.execution_contract_ready = false;
            result.execution_failure_reason =
                "owner execution package 不满足 formal runtime compare contract: " +
                profiled_package_result.GetError().GetMessage();
            result.execution_diagnostic_code = "formal_runtime_compare_contract_unsatisfied";
            result.formal_compare_gate = std::move(formal_compare_gate);
        } else {
            result.execution_package = std::move(profiled_package_result.Value());
        }
    }

    if (result.execution_contract_ready) {
        auto contract_result =
            ValidateFormalProfileCompareContract(result.execution_package, input.profile_compare_runtime_contract);
        if (contract_result.IsError()) {
            result.execution_contract_ready = false;
            result.execution_failure_reason = contract_result.GetError().GetMessage();
            result.execution_diagnostic_code = "formal_runtime_compare_contract_unsatisfied";
        }
    }

    result.export_request = BuildExecutionExportRequest(input, execution_process_path, result);

    {
        std::ostringstream oss;
        oss << "execution_binding_ready=" << (result.execution_binding_ready ? 1 : 0)
            << " execution_contract_ready=" << (result.execution_contract_ready ? 1 : 0)
            << " execution_diagnostic_code=" << result.execution_diagnostic_code
            << " execution_failure_reason=" << result.execution_failure_reason;
        log_stage("execution_assembly_complete", oss.str());
    }
    return Result<ExecutionAssemblyBuildResult>::Success(std::move(result));
}

}  // namespace Siligen::Application::Services::Dispensing::Internal
