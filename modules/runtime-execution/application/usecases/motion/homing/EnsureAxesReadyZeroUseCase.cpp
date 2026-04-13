#include "runtime_execution/application/usecases/motion/homing/EnsureAxesReadyZeroUseCase.h"

#include "process_planning/contracts/configuration/ReadyZeroSpeedResolver.h"
#include "shared/types/Error.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>
#include <thread>
#include <utility>

namespace Siligen::Application::UseCases::Motion::Homing {

using Siligen::Domain::Motion::DomainServices::ReadyZeroDecision;
using Siligen::Domain::Motion::DomainServices::ReadyZeroDecisionService;
using Siligen::Domain::Motion::DomainServices::ReadyZeroPlannedAction;
using Siligen::Domain::Motion::DomainServices::ReadyZeroSupervisorState;
using Siligen::Domain::Motion::Ports::MotionStatus;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::FromIndex;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;

namespace {

constexpr const char* kErrorSource = "EnsureAxesReadyZeroUseCase";
constexpr float32 kMinimumZeroToleranceMm = 0.01f;
constexpr float32 kSettledVelocityThresholdMmS = 0.01f;
constexpr auto kPollInterval = std::chrono::milliseconds(20);

struct AxisSnapshot {
    LogicalAxisId axis = LogicalAxisId::INVALID;
    MotionStatus status;
    ReadyZeroDecision decision;
};

struct GoHomeAxisPlan {
    LogicalAxisId axis = LogicalAxisId::INVALID;
    float32 speed_mm_s = 0.0f;
};

int32 ElapsedMs(const std::chrono::steady_clock::time_point& start_time) {
    return static_cast<int32>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count());
}

EnsureAxesReadyZeroResponse::AxisResult* FindAxisResult(
    std::vector<EnsureAxesReadyZeroResponse::AxisResult>& axis_results,
    LogicalAxisId axis) {
    auto it = std::find_if(axis_results.begin(), axis_results.end(), [axis](const auto& axis_result) {
        return axis_result.axis == axis;
    });
    return it == axis_results.end() ? nullptr : &(*it);
}

bool ContainsAxis(const std::vector<LogicalAxisId>& axes, LogicalAxisId axis) {
    return std::find(axes.begin(), axes.end(), axis) != axes.end();
}

std::string JoinMessages(const std::vector<std::string>& messages) {
    std::ostringstream stream;
    bool first = true;
    for (const auto& message : messages) {
        if (message.empty()) {
            continue;
        }
        if (!first) {
            stream << "; ";
        }
        stream << message;
        first = false;
    }
    return stream.str();
}

std::string AxisScopedMessage(LogicalAxisId axis, const std::string& message) {
    return std::string(Siligen::Shared::Types::AxisName(axis)) + ": " + message;
}

std::string SuccessMessageFor(const EnsureAxesReadyZeroResponse::AxisResult& axis_result, bool force_rehome) {
    if (axis_result.planned_action == "noop") {
        return "Already homed at zero";
    }
    if (axis_result.planned_action == "go_home") {
        return "Moved to zero";
    }
    if (axis_result.planned_action == "home") {
        return force_rehome ? "Rehomed" : "Homing completed";
    }
    return "Ready at zero";
}

Result<std::vector<LogicalAxisId>> ResolveAxes(
    const EnsureAxesReadyZeroRequest& request,
    const std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort>& config_port) {
    std::vector<LogicalAxisId> axes;
    if (!request.axes.empty()) {
        for (const auto axis : request.axes) {
            if (!Siligen::Shared::Types::IsValid(axis)) {
                return Result<std::vector<LogicalAxisId>>::Failure(
                    Error(ErrorCode::INVALID_PARAMETER, "Invalid axis in request", kErrorSource));
            }
            if (!ContainsAxis(axes, axis)) {
                axes.push_back(axis);
            }
        }
        return Result<std::vector<LogicalAxisId>>::Success(axes);
    }

    int axis_count = 2;
    if (config_port) {
        auto hardware_result = config_port->GetHardwareConfiguration();
        if (hardware_result.IsSuccess()) {
            axis_count = static_cast<int>(hardware_result.Value().EffectiveAxisCount());
        }
    }
    for (int axis_index = 0; axis_index < axis_count; ++axis_index) {
        const auto axis = FromIndex(static_cast<std::int16_t>(axis_index));
        if (Siligen::Shared::Types::IsValid(axis)) {
            axes.push_back(axis);
        }
    }
    return Result<std::vector<LogicalAxisId>>::Success(axes);
}

float32 ReadZeroToleranceMm(const std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort>& config_port) {
    if (!config_port) {
        return kMinimumZeroToleranceMm;
    }
    auto machine_result = config_port->GetMachineConfig();
    if (machine_result.IsError()) {
        return kMinimumZeroToleranceMm;
    }
    return std::max(machine_result.Value().positioning_tolerance, kMinimumZeroToleranceMm);
}

Result<GoHomeAxisPlan> ResolveGoHomePlan(
    LogicalAxisId axis,
    const std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort>& config_port) {
    auto speed_result = Siligen::Domain::Configuration::Services::ResolveReadyZeroSpeed(axis, config_port, kErrorSource);
    if (speed_result.IsError()) {
        return Result<GoHomeAxisPlan>::Failure(speed_result.GetError());
    }

    GoHomeAxisPlan plan;
    plan.axis = axis;
    plan.speed_mm_s = speed_result.Value().speed_mm_s;
    return Result<GoHomeAxisPlan>::Success(plan);
}

Result<std::vector<AxisSnapshot>> CaptureAxisSnapshots(
    const std::vector<LogicalAxisId>& axes,
    const std::shared_ptr<Monitoring::MotionMonitoringUseCase>& monitoring_use_case,
    const std::shared_ptr<ReadyZeroDecisionService>& decision_service,
    float32 zero_tolerance_mm) {
    std::vector<AxisSnapshot> snapshots;
    snapshots.reserve(axes.size());
    for (const auto axis : axes) {
        auto status_result = monitoring_use_case->GetAxisMotionStatus(axis);
        if (status_result.IsError()) {
            return Result<std::vector<AxisSnapshot>>::Failure(status_result.GetError());
        }
        AxisSnapshot snapshot;
        snapshot.axis = axis;
        snapshot.status = status_result.Value();
        snapshot.decision = decision_service->EvaluateAxis(
            axis,
            snapshot.status,
            zero_tolerance_mm,
            kSettledVelocityThresholdMmS);
        snapshots.push_back(std::move(snapshot));
    }
    return Result<std::vector<AxisSnapshot>>::Success(std::move(snapshots));
}

bool AllSnapshotsAtZero(const std::vector<AxisSnapshot>& snapshots) {
    return std::all_of(snapshots.begin(), snapshots.end(), [](const auto& snapshot) {
        return snapshot.decision.supervisor_state == ReadyZeroSupervisorState::HOMED_AT_ZERO;
    });
}

}  // namespace

EnsureAxesReadyZeroUseCase::EnsureAxesReadyZeroUseCase(
    std::shared_ptr<HomeAxesUseCase> home_use_case,
    std::shared_ptr<Manual::ManualMotionControlUseCase> manual_motion_use_case,
    std::shared_ptr<Monitoring::MotionMonitoringUseCase> monitoring_use_case,
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
    std::shared_ptr<ReadyZeroDecisionService> decision_service,
    std::shared_ptr<Siligen::Application::Services::Motion::Execution::MotionReadinessService> readiness_service)
    : home_use_case_(std::move(home_use_case)),
      manual_motion_use_case_(std::move(manual_motion_use_case)),
      monitoring_use_case_(std::move(monitoring_use_case)),
      config_port_(std::move(config_port)),
      decision_service_(std::move(decision_service)),
      readiness_service_(std::move(readiness_service)) {}

Result<EnsureAxesReadyZeroResponse> EnsureAxesReadyZeroUseCase::Execute(const EnsureAxesReadyZeroRequest& request) {
    if (!request.Validate()) {
        return Result<EnsureAxesReadyZeroResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid ensure-ready-zero request", kErrorSource));
    }
    if (!home_use_case_ || !manual_motion_use_case_ || !monitoring_use_case_ || !decision_service_) {
        return Result<EnsureAxesReadyZeroResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Ready-zero dependencies not initialized", kErrorSource));
    }

    const auto start_time = std::chrono::steady_clock::now();
    EnsureAxesReadyZeroResponse response;

    auto axes_result = ResolveAxes(request, config_port_);
    if (axes_result.IsError()) {
        return Result<EnsureAxesReadyZeroResponse>::Failure(axes_result.GetError());
    }
    const auto axes = axes_result.Value();
    if (axes.empty()) {
        response.accepted = false;
        response.summary_state = "rejected";
        response.message = "No axes requested";
        response.total_time_ms = ElapsedMs(start_time);
        return Result<EnsureAxesReadyZeroResponse>::Success(response);
    }

    const float32 zero_tolerance_mm = ReadZeroToleranceMm(config_port_);

    auto initial_snapshots_result = CaptureAxisSnapshots(axes, monitoring_use_case_, decision_service_, zero_tolerance_mm);
    if (initial_snapshots_result.IsError()) {
        return Result<EnsureAxesReadyZeroResponse>::Failure(initial_snapshots_result.GetError());
    }

    const auto& initial_snapshots = initial_snapshots_result.Value();
    response.axis_results.reserve(initial_snapshots.size());
    std::vector<std::string> rejection_messages;
    std::vector<LogicalAxisId> axes_to_home;
    std::vector<LogicalAxisId> axes_to_go_home;
    bool has_transient_motion_rejection = false;
    bool has_non_transient_rejection = false;

    for (const auto& snapshot : initial_snapshots) {
        EnsureAxesReadyZeroResponse::AxisResult axis_result;
        axis_result.axis = snapshot.axis;
        axis_result.supervisor_state = ReadyZeroDecisionService::ToString(snapshot.decision.supervisor_state);
        axis_result.planned_action = ReadyZeroDecisionService::ToString(snapshot.decision.planned_action);
        axis_result.reason_code = snapshot.decision.reason_code;
        axis_result.message = snapshot.decision.message;
        axis_result.success = snapshot.decision.planned_action == ReadyZeroPlannedAction::NOOP;

        ReadyZeroPlannedAction effective_action = snapshot.decision.planned_action;
        if (request.force_rehome && effective_action != ReadyZeroPlannedAction::REJECT) {
            effective_action = ReadyZeroPlannedAction::HOME;
            axis_result.planned_action = ReadyZeroDecisionService::ToString(effective_action);
            axis_result.reason_code = "force_rehome";
            axis_result.message = "Force rehome requested";
            axis_result.success = false;
        }

        if (effective_action == ReadyZeroPlannedAction::REJECT) {
            if (snapshot.decision.reason_code == "axis_moving") {
                has_transient_motion_rejection = true;
            } else {
                has_non_transient_rejection = true;
            }
            rejection_messages.push_back(AxisScopedMessage(snapshot.axis, snapshot.decision.message));
        } else if (effective_action == ReadyZeroPlannedAction::HOME) {
            axes_to_home.push_back(snapshot.axis);
        } else if (effective_action == ReadyZeroPlannedAction::GO_HOME) {
            axes_to_go_home.push_back(snapshot.axis);
        } else {
            axis_result.message = "Already homed at zero";
        }

        response.axis_results.push_back(std::move(axis_result));
    }

    if (!rejection_messages.empty()) {
        if (has_transient_motion_rejection && !has_non_transient_rejection && readiness_service_) {
            auto readiness_result = readiness_service_->Evaluate();
            if (readiness_result.IsError()) {
                return Result<EnsureAxesReadyZeroResponse>::Failure(readiness_result.GetError());
            }
            if (!readiness_result.Value().ready) {
                for (auto& axis_result : response.axis_results) {
                    if (axis_result.reason_code == "axis_moving") {
                        axis_result.reason_code = "motion_not_ready";
                        axis_result.message = "motion_not_ready";
                    }
                }
                response.accepted = false;
                response.summary_state = "blocked";
                response.reason_code = "motion_not_ready";
                response.message = "motion_not_ready";
                response.total_time_ms = ElapsedMs(start_time);
                return Result<EnsureAxesReadyZeroResponse>::Success(response);
            }
        }

        response.accepted = false;
        response.summary_state = "rejected";
        response.message = JoinMessages(rejection_messages);
        response.total_time_ms = ElapsedMs(start_time);
        return Result<EnsureAxesReadyZeroResponse>::Success(response);
    }

    response.accepted = true;

    if (!axes_to_home.empty()) {
        HomeAxesRequest home_request;
        home_request.axes = axes_to_home;
        home_request.wait_for_completion = request.wait_for_completion;
        home_request.timeout_ms = request.timeout_ms;
        home_request.force_rehome = true;

        auto home_result = home_use_case_->Execute(home_request);
        if (home_result.IsError()) {
            return Result<EnsureAxesReadyZeroResponse>::Failure(home_result.GetError());
        }

        const auto& home_response = home_result.Value();
        bool phase1_failed = false;
        for (const auto axis : axes_to_home) {
            auto* axis_result = FindAxisResult(response.axis_results, axis);
            if (!axis_result) {
                continue;
            }
            axis_result->executed = true;
            const bool homing_success = ContainsAxis(home_response.successfully_homed_axes, axis);
            axis_result->success = homing_success;
            if (!homing_success) {
                phase1_failed = true;
                axis_result->reason_code = "home_failed";
                auto axis_it = std::find_if(
                    home_response.axis_results.begin(),
                    home_response.axis_results.end(),
                    [axis](const auto& item) { return item.axis == axis; });
                axis_result->message = axis_it != home_response.axis_results.end()
                    ? axis_it->message
                    : "Homing failed";
            }
        }

        if (phase1_failed) {
            std::vector<std::string> failure_messages;
            for (const auto& axis_result : response.axis_results) {
                if (!axis_result.success) {
                    failure_messages.push_back(AxisScopedMessage(axis_result.axis, axis_result.message));
                }
            }
            response.summary_state = "failed";
            response.message = JoinMessages(failure_messages);
            response.total_time_ms = ElapsedMs(start_time);
            return Result<EnsureAxesReadyZeroResponse>::Success(response);
        }

        auto post_home_snapshots_result =
            CaptureAxisSnapshots(axes, monitoring_use_case_, decision_service_, zero_tolerance_mm);
        if (post_home_snapshots_result.IsError()) {
            return Result<EnsureAxesReadyZeroResponse>::Failure(post_home_snapshots_result.GetError());
        }

        for (const auto& snapshot : post_home_snapshots_result.Value()) {
            auto* axis_result = FindAxisResult(response.axis_results, snapshot.axis);
            if (!axis_result) {
                continue;
            }
            if (axis_result->planned_action == "home") {
                axis_result->supervisor_state = ReadyZeroDecisionService::ToString(snapshot.decision.supervisor_state);
                axis_result->reason_code = snapshot.decision.reason_code;
                if (snapshot.decision.supervisor_state == ReadyZeroSupervisorState::HOMED_AT_ZERO) {
                    axis_result->success = true;
                    axis_result->message = SuccessMessageFor(*axis_result, request.force_rehome);
                } else if (snapshot.decision.supervisor_state == ReadyZeroSupervisorState::HOMED_AWAY_FROM_ZERO) {
                    if (!ContainsAxis(axes_to_go_home, snapshot.axis)) {
                        axes_to_go_home.push_back(snapshot.axis);
                    }
                } else {
                    axis_result->success = false;
                    axis_result->reason_code = "not_homed_after_home";
                    axis_result->message = snapshot.decision.message;
                }
            }
        }
    }

    bool post_home_failure = false;
    for (const auto& axis_result : response.axis_results) {
        if (axis_result.planned_action == "home" && !axis_result.success &&
            !ContainsAxis(axes_to_go_home, axis_result.axis)) {
            post_home_failure = true;
            break;
        }
    }
    if (post_home_failure) {
        std::vector<std::string> failure_messages;
        for (const auto& axis_result : response.axis_results) {
            if (!axis_result.success) {
                failure_messages.push_back(AxisScopedMessage(axis_result.axis, axis_result.message));
            }
        }
        response.summary_state = "failed";
        response.message = JoinMessages(failure_messages);
        response.total_time_ms = ElapsedMs(start_time);
        return Result<EnsureAxesReadyZeroResponse>::Success(response);
    }

    if (!axes_to_go_home.empty()) {
        if (readiness_service_) {
            auto readiness_result = readiness_service_->Evaluate();
            if (readiness_result.IsError()) {
                return Result<EnsureAxesReadyZeroResponse>::Failure(readiness_result.GetError());
            }
            if (!readiness_result.Value().ready) {
                for (auto& axis_result : response.axis_results) {
                    if (ContainsAxis(axes_to_go_home, axis_result.axis)) {
                        axis_result.success = false;
                        axis_result.reason_code = "motion_not_ready";
                        axis_result.message = "motion_not_ready";
                    }
                }
                response.accepted = false;
                response.summary_state = "blocked";
                response.reason_code = "motion_not_ready";
                response.message = "motion_not_ready";
                response.total_time_ms = ElapsedMs(start_time);
                return Result<EnsureAxesReadyZeroResponse>::Success(response);
            }
        }

        std::vector<GoHomeAxisPlan> go_home_plans;
        go_home_plans.reserve(axes_to_go_home.size());
        for (const auto axis : axes_to_go_home) {
            auto plan_result = ResolveGoHomePlan(axis, config_port_);
            if (plan_result.IsError()) {
                return Result<EnsureAxesReadyZeroResponse>::Failure(plan_result.GetError());
            }
            go_home_plans.push_back(plan_result.Value());
        }

        for (const auto& plan : go_home_plans) {
            const auto axis = plan.axis;
            auto* axis_result = FindAxisResult(response.axis_results, axis);
            if (axis_result) {
                axis_result->executed = true;
            }

            Manual::ManualMotionCommand command;
            command.axis = axis;
            command.position = 0.0f;
            command.velocity = plan.speed_mm_s;

            auto move_result = manual_motion_use_case_->ExecutePointToPointMotion(command, false);
            if (move_result.IsError()) {
                if (axis_result) {
                    axis_result->success = false;
                    axis_result->reason_code = "go_home_dispatch_failed";
                    axis_result->message = move_result.GetError().GetMessage();
                }
                response.summary_state = "failed";
                response.message = AxisScopedMessage(axis, move_result.GetError().GetMessage());
                response.total_time_ms = ElapsedMs(start_time);
                return Result<EnsureAxesReadyZeroResponse>::Success(response);
            }
        }

        if (!request.wait_for_completion) {
            for (const auto axis : axes_to_go_home) {
                auto* axis_result = FindAxisResult(response.axis_results, axis);
                if (!axis_result) {
                    continue;
                }
                axis_result->success = true;
                axis_result->message = axis_result->planned_action == "home"
                    ? (request.force_rehome ? "Rehome completed; moving to zero" : "Homed; moving to zero")
                    : "Moving to zero";
            }
            response.summary_state = "in_progress";
            response.message = "Go-home started";
            response.total_time_ms = ElapsedMs(start_time);
            return Result<EnsureAxesReadyZeroResponse>::Success(response);
        }

        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(request.timeout_ms);
        auto final_snapshots_result =
            CaptureAxisSnapshots(axes_to_go_home, monitoring_use_case_, decision_service_, zero_tolerance_mm);
        if (final_snapshots_result.IsError()) {
            return Result<EnsureAxesReadyZeroResponse>::Failure(final_snapshots_result.GetError());
        }
        auto final_snapshots = final_snapshots_result.Value();

        while (!AllSnapshotsAtZero(final_snapshots) && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(kPollInterval);
            final_snapshots_result =
                CaptureAxisSnapshots(axes_to_go_home, monitoring_use_case_, decision_service_, zero_tolerance_mm);
            if (final_snapshots_result.IsError()) {
                return Result<EnsureAxesReadyZeroResponse>::Failure(final_snapshots_result.GetError());
            }
            final_snapshots = final_snapshots_result.Value();
        }

        std::vector<std::string> phase2_failures;
        for (const auto& snapshot : final_snapshots) {
            auto* axis_result = FindAxisResult(response.axis_results, snapshot.axis);
            if (!axis_result) {
                continue;
            }
            axis_result->supervisor_state = ReadyZeroDecisionService::ToString(snapshot.decision.supervisor_state);
            if (snapshot.decision.supervisor_state == ReadyZeroSupervisorState::HOMED_AT_ZERO) {
                axis_result->success = true;
                axis_result->message = axis_result->planned_action == "home"
                    ? (request.force_rehome ? "Rehomed then moved to zero" : "Homed then moved to zero")
                    : "Moved to zero";
                continue;
            }

            axis_result->success = false;
            axis_result->reason_code = snapshot.decision.reason_code.empty()
                ? "zero_position_not_reached"
                : snapshot.decision.reason_code;
            axis_result->message = snapshot.decision.message.empty()
                ? "Axis did not reach zero"
                : snapshot.decision.message;
            phase2_failures.push_back(AxisScopedMessage(snapshot.axis, axis_result->message));
        }

        if (!phase2_failures.empty()) {
            response.summary_state = "failed";
            response.message = JoinMessages(phase2_failures);
            response.total_time_ms = ElapsedMs(start_time);
            return Result<EnsureAxesReadyZeroResponse>::Success(response);
        }
    }

    const bool any_executed = std::any_of(response.axis_results.begin(), response.axis_results.end(), [](const auto& axis_result) {
        return axis_result.executed;
    });
    response.summary_state = any_executed ? "completed" : "noop";
    response.message = any_executed ? "Axes ready at zero" : "Axes already homed at zero";
    response.total_time_ms = ElapsedMs(start_time);
    return Result<EnsureAxesReadyZeroResponse>::Success(response);
}

}  // namespace Siligen::Application::UseCases::Motion::Homing
