#pragma once

#include "shared/types/CMPTypes.h"
#include "shared/types/Error.h"
#include "shared/types/Point2D.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Siligen::Runtime::Service::System {

struct DispensingTask {
    std::string task_id;
    std::vector<Siligen::Shared::Types::Point2D> path;
    Siligen::Shared::Types::CMPConfiguration cmp_config;
    float movement_speed = 0.0f;

    bool Validate() const {
        return !path.empty() && movement_speed > 0.0f && cmp_config.Validate();
    }
};

class DispenserModel {
   public:
    using ResultVoid = Siligen::Shared::Types::Result<void>;
    using ResultInt = Siligen::Shared::Types::Result<int32_t>;

    DispenserModel() = default;

    ResultVoid SetState(Siligen::DispenserState state) {
        auto validation = ValidateStateTransition(current_state_, state);
        if (validation.IsError()) {
            return validation;
        }

        current_state_ = state;
        return ResultVoid::Success();
    }

    Siligen::DispenserState GetState() const {
        return current_state_;
    }

    ResultVoid AddTask(const DispensingTask& task) {
        if (!task.Validate()) {
            return ResultVoid::Failure(MakeError(
                Siligen::Shared::Types::ErrorCode::ValidationFailed,
                "machine execution task validation failed"));
        }
        if (static_cast<int32_t>(pending_tasks_.size()) >= kMaxPendingTasks) {
            return ResultVoid::Failure(MakeError(
                Siligen::Shared::Types::ErrorCode::INVALID_STATE,
                "machine execution task queue is full"));
        }

        pending_tasks_.push_back(task);
        return ResultVoid::Success();
    }

    ResultInt GetTaskQueueSize() const {
        return ResultInt::Success(static_cast<int32_t>(pending_tasks_.size()));
    }

    ResultVoid ClearAllTasks() {
        pending_tasks_.clear();
        return ResultVoid::Success();
    }

   private:
    static constexpr int32_t kMaxPendingTasks = 100;

    static Siligen::Shared::Types::Error MakeError(
        Siligen::Shared::Types::ErrorCode code,
        std::string message) {
        return Siligen::Shared::Types::Error(std::move(code), std::move(message), "RuntimeDispenserModel");
    }

    static ResultVoid ValidateStateTransition(
        Siligen::DispenserState from_state,
        Siligen::DispenserState to_state) {
        using State = Siligen::DispenserState;

        switch (from_state) {
            case State::UNINITIALIZED:
                if (to_state == State::INITIALIZING || to_state == State::EMERGENCY_STOP) {
                    return ResultVoid::Success();
                }
                break;
            case State::INITIALIZING:
                if (to_state == State::READY || to_state == State::ERROR_STATE || to_state == State::EMERGENCY_STOP) {
                    return ResultVoid::Success();
                }
                break;
            case State::READY:
                if (to_state == State::DISPENSING || to_state == State::ERROR_STATE || to_state == State::EMERGENCY_STOP) {
                    return ResultVoid::Success();
                }
                break;
            case State::DISPENSING:
                if (to_state == State::PAUSED || to_state == State::READY || to_state == State::ERROR_STATE ||
                    to_state == State::EMERGENCY_STOP) {
                    return ResultVoid::Success();
                }
                break;
            case State::PAUSED:
                if (to_state == State::DISPENSING || to_state == State::READY || to_state == State::ERROR_STATE ||
                    to_state == State::EMERGENCY_STOP) {
                    return ResultVoid::Success();
                }
                break;
            case State::ERROR_STATE:
                if (to_state == State::READY || to_state == State::EMERGENCY_STOP) {
                    return ResultVoid::Success();
                }
                break;
            case State::EMERGENCY_STOP:
                if (to_state == State::UNINITIALIZED) {
                    return ResultVoid::Success();
                }
                break;
            default:
                break;
        }

        return ResultVoid::Failure(MakeError(
            Siligen::Shared::Types::ErrorCode::INVALID_STATE,
            "invalid runtime machine execution state transition"));
    }

    Siligen::DispenserState current_state_ = Siligen::DispenserState::UNINITIALIZED;
    std::vector<DispensingTask> pending_tasks_;
};

}  // namespace Siligen::Runtime::Service::System
