#pragma once

#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "runtime_execution/contracts/motion/IMotionStatePort.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Siligen::Application::Services::Motion::Execution {

using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int16;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::Result;

enum class ExecutionTransitionState {
    UNSPECIFIED = 0,
    PENDING,
    RUNNING,
    STOPPING,
    CANCELING,
    PAUSING,
    PAUSED,
    COMPLETED,
    CANCELLED,
    FAILED,
    IDLE,
    UNKNOWN,
};

enum class MotionReadinessReason {
    NONE = 0,
    MOTION_NOT_READY,
};

enum class MotionReadinessBlockCause {
    NONE = 0,
    ACTIVE_JOB_STATE,
    COORDINATE_SYSTEM_ERROR,
    COORDINATE_SYSTEM_NOT_SETTLED,
    AXIS_NOT_SETTLED,
};

const char* ToString(ExecutionTransitionState state) noexcept;
ExecutionTransitionState ParseExecutionTransitionState(std::string_view state) noexcept;
const char* ToString(MotionReadinessReason reason) noexcept;
const char* ToString(MotionReadinessBlockCause cause) noexcept;

struct MotionReadinessQuery {
    int16 coord_sys = 1;
    std::string active_job_state;
    ExecutionTransitionState active_job_transition_state = ExecutionTransitionState::UNSPECIFIED;
    float32 velocity_tolerance_mm_s = 0.001f;
};

struct MotionReadinessResult {
    bool ready = false;
    MotionReadinessReason reason = MotionReadinessReason::NONE;
    MotionReadinessBlockCause block_cause = MotionReadinessBlockCause::NONE;
    ExecutionTransitionState active_job_transition_state = ExecutionTransitionState::UNSPECIFIED;
    std::string reason_code;
    std::string message;
    std::string diagnostic_message;
    Siligen::Domain::Motion::Ports::CoordinateSystemStatus coord_status;
    std::vector<Siligen::Domain::Motion::Ports::MotionStatus> axis_statuses;
};

class MotionReadinessService {
   public:
    MotionReadinessService(
        std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port,
        std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> interpolation_port);

    Result<MotionReadinessResult> Evaluate(const MotionReadinessQuery& query = {}) const;
    Result<MotionReadinessResult> WaitUntilReady(
        const MotionReadinessQuery& query,
        int32 timeout_ms = 2000,
        int32 poll_interval_ms = 20) const;

   private:
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port_;
    std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> interpolation_port_;
};

}  // namespace Siligen::Application::Services::Motion::Execution
