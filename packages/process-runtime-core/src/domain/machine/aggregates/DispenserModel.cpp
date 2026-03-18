#include "DispenserModel.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Siligen::Domain::Machine::Aggregates::Legacy {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::Point2D;

namespace {

bool IsTaskIdMatch(const DispensingTask& task, const std::string& task_id) {
    return task.task_id == task_id;
}

}  // namespace

DispenserModel::DispenserModel()
    : current_state_(DispenserState::UNINITIALIZED),
      current_task_(),
      pending_tasks_(),
      completed_tasks_(),
      motion_config_(),
      connection_config_(),
      total_completed_tasks_(0),
      total_running_time_(0.0),
      total_dispensing_length_(0.0),
      model_created_at_(std::chrono::system_clock::now()) {}

Result<void> DispenserModel::SetState(DispenserState state) {
    auto validation = ValidateStateTransition(current_state_, state);
    if (validation.IsError()) {
        return validation;
    }

    current_state_ = state;
    return Result<void>::Success();
}

DispenserState DispenserModel::GetState() const {
    return current_state_;
}

Result<bool> DispenserModel::CanDispense() const {
    bool can_dispense = (current_state_ == DispenserState::READY || current_state_ == DispenserState::PAUSED);
    return Result<bool>::Success(can_dispense);
}

bool DispenserModel::HasError() const {
    return current_state_ == DispenserState::ERROR_STATE;
}

Result<void> DispenserModel::ClearError() {
    if (current_state_ != DispenserState::ERROR_STATE) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "Dispenser is not in error state", "DispenserModel"));
    }
    current_state_ = DispenserState::READY;
    return Result<void>::Success();
}

Result<void> DispenserModel::AddTask(const DispensingTask& task) {
    if (!task.Validate()) {
        return Result<void>::Failure(
            Error(ErrorCode::ValidationFailed, "Dispensing task validation failed", "DispenserModel"));
    }
    if (static_cast<int32_t>(pending_tasks_.size()) >= MAX_PENDING_TASKS) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "Task queue is full", "DispenserModel"));
    }

    pending_tasks_.push_back(task);
    return Result<void>::Success();
}

Result<void> DispenserModel::StartTask(const std::string& task_id) {
    auto it = std::find_if(pending_tasks_.begin(), pending_tasks_.end(),
                           [&](const DispensingTask& task) { return IsTaskIdMatch(task, task_id); });
    if (it == pending_tasks_.end()) {
        return Result<void>::Failure(
            Error(ErrorCode::NOT_FOUND, "Task not found: " + task_id, "DispenserModel"));
    }

    auto validation = ValidateTaskStart(*it);
    if (validation.IsError()) {
        return validation;
    }

    current_task_ = *it;
    current_task_.started_at = std::chrono::system_clock::now();
    current_task_.is_completed = false;
    current_task_.current_point_index = 0;
    pending_tasks_.erase(it);

    current_state_ = DispenserState::DISPENSING;
    return Result<void>::Success();
}

Result<void> DispenserModel::PauseCurrentTask() {
    if (current_state_ != DispenserState::DISPENSING) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "Dispenser is not dispensing", "DispenserModel"));
    }
    current_state_ = DispenserState::PAUSED;
    return Result<void>::Success();
}

Result<void> DispenserModel::ResumeCurrentTask() {
    if (current_state_ != DispenserState::PAUSED) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "Dispenser is not paused", "DispenserModel"));
    }
    current_state_ = DispenserState::DISPENSING;
    return Result<void>::Success();
}

Result<void> DispenserModel::CancelTask(const std::string& task_id) {
    if (current_task_.task_id == task_id) {
        current_task_ = DispensingTask();
        current_state_ = DispenserState::READY;
        return Result<void>::Success();
    }

    auto it = std::find_if(pending_tasks_.begin(), pending_tasks_.end(),
                           [&](const DispensingTask& task) { return IsTaskIdMatch(task, task_id); });
    if (it == pending_tasks_.end()) {
        return Result<void>::Failure(
            Error(ErrorCode::NOT_FOUND, "Task not found: " + task_id, "DispenserModel"));
    }
    pending_tasks_.erase(it);
    return Result<void>::Success();
}

Result<void> DispenserModel::CompleteCurrentTask() {
    if (current_task_.task_id.empty()) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "No active task to complete", "DispenserModel"));
    }

    current_task_.is_completed = true;
    current_task_.completed_at = std::chrono::system_clock::now();

    UpdateStatistics(current_task_);
    completed_tasks_.push_back(current_task_);
    if (static_cast<int32_t>(completed_tasks_.size()) > MAX_COMPLETED_TASKS_HISTORY) {
        completed_tasks_.erase(completed_tasks_.begin());
    }

    current_task_ = DispensingTask();
    current_state_ = DispenserState::READY;
    return Result<void>::Success();
}

Result<DispensingTask> DispenserModel::GetCurrentTask() const {
    if (current_task_.task_id.empty()) {
        return Result<DispensingTask>::Failure(
            Error(ErrorCode::NOT_FOUND, "No active task", "DispenserModel"));
    }
    return Result<DispensingTask>::Success(current_task_);
}

Result<std::vector<DispensingTask>> DispenserModel::GetPendingTasks() const {
    return Result<std::vector<DispensingTask>>::Success(pending_tasks_);
}

Result<int32_t> DispenserModel::GetTaskQueueSize() const {
    return Result<int32_t>::Success(static_cast<int32_t>(pending_tasks_.size()));
}

Result<void> DispenserModel::ClearAllTasks() {
    pending_tasks_.clear();
    current_task_ = DispensingTask();
    return Result<void>::Success();
}

Result<void> DispenserModel::SetMotionConfig(const Siligen::Shared::Types::MotionConfig& config) {
    motion_config_ = config;
    return Result<void>::Success();
}

const Siligen::Shared::Types::MotionConfig& DispenserModel::GetMotionConfig() const {
    return motion_config_;
}

Result<void> DispenserModel::SetConnectionConfig(const Siligen::Shared::Types::ConnectionConfig& config) {
    connection_config_ = config;
    return Result<void>::Success();
}

const Siligen::Shared::Types::ConnectionConfig& DispenserModel::GetConnectionConfig() const {
    return connection_config_;
}

int32_t DispenserModel::GetCompletedTaskCount() const {
    return total_completed_tasks_;
}

double DispenserModel::GetTotalRunningTime() const {
    return total_running_time_;
}

double DispenserModel::GetTotalDispensingLength() const {
    return total_dispensing_length_;
}

Result<void> DispenserModel::ResetStatistics() {
    total_completed_tasks_ = 0;
    total_running_time_ = 0.0;
    total_dispensing_length_ = 0.0;
    return Result<void>::Success();
}

Result<void> DispenserModel::ValidateStateTransition(DispenserState from_state, DispenserState to_state) {
    switch (from_state) {
        case DispenserState::UNINITIALIZED:
            if (to_state == DispenserState::INITIALIZING ||
                to_state == DispenserState::EMERGENCY_STOP) {
                return Result<void>::Success();
            }
            break;
        case DispenserState::INITIALIZING:
            if (to_state == DispenserState::READY ||
                to_state == DispenserState::ERROR_STATE ||
                to_state == DispenserState::EMERGENCY_STOP) {
                return Result<void>::Success();
            }
            break;
        case DispenserState::READY:
            if (to_state == DispenserState::DISPENSING ||
                to_state == DispenserState::ERROR_STATE ||
                to_state == DispenserState::EMERGENCY_STOP) {
                return Result<void>::Success();
            }
            break;
        case DispenserState::DISPENSING:
            if (to_state == DispenserState::PAUSED ||
                to_state == DispenserState::READY ||
                to_state == DispenserState::ERROR_STATE ||
                to_state == DispenserState::EMERGENCY_STOP) {
                return Result<void>::Success();
            }
            break;
        case DispenserState::PAUSED:
            if (to_state == DispenserState::DISPENSING ||
                to_state == DispenserState::READY ||
                to_state == DispenserState::ERROR_STATE ||
                to_state == DispenserState::EMERGENCY_STOP) {
                return Result<void>::Success();
            }
            break;
        case DispenserState::ERROR_STATE:
            if (to_state == DispenserState::READY ||
                to_state == DispenserState::EMERGENCY_STOP) {
                return Result<void>::Success();
            }
            break;
        case DispenserState::EMERGENCY_STOP:
            if (to_state == DispenserState::UNINITIALIZED) {
                return Result<void>::Success();
            }
            break;
        default:
            break;
    }

    return Result<void>::Failure(
        Error(ErrorCode::INVALID_STATE, "Invalid state transition", "DispenserModel"));
}

Result<void> DispenserModel::ValidateTaskStart(const DispensingTask& task) const {
    if (current_state_ != DispenserState::READY) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "Dispenser is not ready", "DispenserModel"));
    }
    if (!task.Validate()) {
        return Result<void>::Failure(
            Error(ErrorCode::ValidationFailed, "Dispensing task validation failed", "DispenserModel"));
    }
    return Result<void>::Success();
}

std::string DispenserModel::ToJson() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"state\":\"" << DispenserStateToString(current_state_) << "\",";
    oss << "\"pending_tasks\":" << pending_tasks_.size() << ",";
    oss << "\"completed_tasks\":" << total_completed_tasks_;
    oss << "}";
    return oss.str();
}

std::string DispenserModel::ToString() const {
    std::ostringstream oss;
    oss << "DispenserModel(state=" << DispenserStateToString(current_state_)
        << ", pending=" << pending_tasks_.size()
        << ", completed=" << total_completed_tasks_
        << ")";
    return oss.str();
}

bool DispenserModel::IsStateTransitionValid(DispenserState from_state, DispenserState to_state) const {
    return ValidateStateTransition(from_state, to_state).IsSuccess();
}

void DispenserModel::UpdateStatistics(const DispensingTask& completed_task) {
    ++total_completed_tasks_;

    if (completed_task.started_at.time_since_epoch().count() > 0 &&
        completed_task.completed_at.time_since_epoch().count() > 0) {
        auto duration = completed_task.completed_at - completed_task.started_at;
        total_running_time_ += std::chrono::duration_cast<std::chrono::duration<double>>(duration).count();
    }

    total_dispensing_length_ += CalculatePathLength(completed_task.path);
}

double DispenserModel::CalculatePathLength(const std::vector<Point2D>& path) const {
    if (path.size() < 2) {
        return 0.0;
    }

    double length = 0.0;
    for (size_t i = 1; i < path.size(); ++i) {
        length += static_cast<double>(path[i - 1].DistanceTo(path[i]));
    }
    return length;
}

}  // namespace Siligen::Domain::Machine::Aggregates::Legacy
