#include "runtime_execution/application/services/motion/ReadyZeroDecisionService.h"

#include <cmath>

namespace Siligen::RuntimeExecution::Application::Services::Motion {

namespace {

using MotionState = Siligen::Domain::Motion::Ports::MotionState;
using MotionStatus = Siligen::Domain::Motion::Ports::MotionStatus;

bool IsFaulted(const MotionStatus& status) {
    return status.has_error || status.servo_alarm || status.following_error ||
           status.state == MotionState::FAULT || status.state == MotionState::ESTOP ||
           status.state == MotionState::DISABLED;
}

bool IsMoving(const MotionStatus& status) {
    return status.state == MotionState::MOVING;
}

bool IsHoming(const MotionStatus& status) {
    return status.state == MotionState::HOMING || status.homing_state == "homing";
}

}  // namespace

ReadyZeroDecision ReadyZeroDecisionService::EvaluateAxis(LogicalAxisId axis,
                                                         const MotionStatus& status,
                                                         float32 zero_tolerance_mm,
                                                         float32 settled_velocity_threshold_mm_s) const {
    ReadyZeroDecision decision;
    decision.axis = axis;
    decision.position_mm = ExtractAxisPositionMm(axis, status);
    decision.home_signal = status.home_signal;
    decision.homed = status.homing_state == "homed";
    decision.at_zero = decision.homed &&
                       std::abs(decision.position_mm) <= std::max(0.0f, zero_tolerance_mm) &&
                       std::abs(status.velocity) <= std::max(0.0f, settled_velocity_threshold_mm_s) &&
                       !IsMoving(status);

    if (IsFaulted(status)) {
        decision.supervisor_state = ReadyZeroSupervisorState::BLOCKED;
        decision.planned_action = ReadyZeroPlannedAction::REJECT;
        decision.reason_code = "axis_fault";
        decision.message = "Axis faulted or unavailable";
        return decision;
    }

    if (IsMoving(status)) {
        decision.supervisor_state = ReadyZeroSupervisorState::BLOCKED;
        decision.planned_action = ReadyZeroPlannedAction::REJECT;
        decision.reason_code = "axis_moving";
        decision.message = "Axis is moving";
        return decision;
    }

    if (IsHoming(status)) {
        decision.supervisor_state = ReadyZeroSupervisorState::HOMING_IN_PROGRESS;
        decision.planned_action = ReadyZeroPlannedAction::REJECT;
        decision.reason_code = "homing_in_progress";
        decision.message = "Axis homing is already in progress";
        return decision;
    }

    if (!decision.homed) {
        decision.supervisor_state = ReadyZeroSupervisorState::NOT_HOMED;
        decision.planned_action = ReadyZeroPlannedAction::HOME;
        decision.reason_code = status.homing_state == "failed" ? "homing_failed_rehome_required" : "not_homed";
        decision.message = status.homing_state == "failed" ? "Axis homing failed previously; rehome required"
                                                           : "Axis is not homed";
        return decision;
    }

    if (decision.at_zero) {
        decision.supervisor_state = ReadyZeroSupervisorState::HOMED_AT_ZERO;
        decision.planned_action = ReadyZeroPlannedAction::NOOP;
        decision.reason_code = "already_at_zero";
        decision.message = "Axis already homed at zero";
        return decision;
    }

    decision.supervisor_state = ReadyZeroSupervisorState::HOMED_AWAY_FROM_ZERO;
    decision.planned_action = ReadyZeroPlannedAction::GO_HOME;
    decision.reason_code = "homed_away_from_zero";
    decision.message = "Axis homed but not at zero";
    return decision;
}

const char* ReadyZeroDecisionService::ToString(ReadyZeroSupervisorState state) {
    switch (state) {
        case ReadyZeroSupervisorState::BLOCKED:
            return "blocked";
        case ReadyZeroSupervisorState::HOMING_IN_PROGRESS:
            return "homing_in_progress";
        case ReadyZeroSupervisorState::NOT_HOMED:
            return "not_homed";
        case ReadyZeroSupervisorState::HOMED_AT_ZERO:
            return "homed_at_zero";
        case ReadyZeroSupervisorState::HOMED_AWAY_FROM_ZERO:
            return "homed_away_from_zero";
        default:
            return "unknown";
    }
}

const char* ReadyZeroDecisionService::ToString(ReadyZeroPlannedAction action) {
    switch (action) {
        case ReadyZeroPlannedAction::REJECT:
            return "reject";
        case ReadyZeroPlannedAction::NOOP:
            return "noop";
        case ReadyZeroPlannedAction::HOME:
            return "home";
        case ReadyZeroPlannedAction::GO_HOME:
            return "go_home";
        default:
            return "unknown";
    }
}

float32 ReadyZeroDecisionService::ExtractAxisPositionMm(
    LogicalAxisId axis,
    const MotionStatus& status) {
    const float32 coordinate = axis == LogicalAxisId::Y ? status.position.y : status.position.x;
    if (std::isfinite(status.axis_position_mm)) {
        if (std::abs(status.axis_position_mm) > 1e-6f || std::abs(coordinate) <= 1e-6f) {
            return status.axis_position_mm;
        }
    }
    return coordinate;
}

}  // namespace Siligen::RuntimeExecution::Application::Services::Motion
