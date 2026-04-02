#pragma once

#include "shared/types/AxisStatus.h"
#include "shared/types/CMPTypes.h"
#include "shared/types/ConfigTypes.h"
#include "shared/types/DomainEvent.h"
#include "shared/types/Error.h"
#include "shared/types/Point2D.h"
#include "shared/types/Result.h"
#include "shared/util/ModuleDependencyCheck.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Siligen::RuntimeExecution::Application::System {

using ::Siligen::DispenserState;

inline const char* DispenserStateToString(DispenserState state) {
    switch (state) {
        case DispenserState::UNINITIALIZED:
            return "UNINITIALIZED";
        case DispenserState::INITIALIZING:
            return "INITIALIZING";
        case DispenserState::READY:
            return "READY";
        case DispenserState::DISPENSING:
            return "DISPENSING";
        case DispenserState::PAUSED:
            return "PAUSED";
        case DispenserState::ERROR_STATE:
            return "ERROR_STATE";
        case DispenserState::EMERGENCY_STOP:
            return "EMERGENCY_STOP";
        default:
            return "UNKNOWN";
    }
}

struct DispensingTask {
    std::string task_id;
    std::vector<Siligen::Shared::Types::Point2D> path;
    Siligen::Shared::Types::CMPConfiguration cmp_config;
    float movement_speed;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point completed_at;
    bool is_completed;
    int32_t current_point_index;

    DispensingTask()
        : task_id(""),
          path(),
          cmp_config(),
          movement_speed(0.0f),
          created_at(std::chrono::system_clock::now()),
          started_at(),
          completed_at(),
          is_completed(false),
          current_point_index(0) {}

    float GetProgress() const {
        if (path.empty()) return 0.0f;
        return (static_cast<float>(current_point_index) / path.size()) * 100.0f;
    }

    int32_t GetRemainingPoints() const {
        if (path.empty()) return 0;
        return static_cast<int32_t>(path.size()) - current_point_index;
    }

    bool Validate() const {
        if (path.empty()) return false;
        if (movement_speed <= 0.0f) return false;
        if (!cmp_config.Validate()) return false;
        return true;
    }
};

class LegacyDispenserModel : public Siligen::Shared::Util::DomainModuleTag {
   public:
    LegacyDispenserModel();

    ~LegacyDispenserModel() = default;

    LegacyDispenserModel(const LegacyDispenserModel&) = delete;
    LegacyDispenserModel& operator=(const LegacyDispenserModel&) = delete;
    LegacyDispenserModel(LegacyDispenserModel&&) = delete;
    LegacyDispenserModel& operator=(LegacyDispenserModel&&) = delete;

    Siligen::Shared::Types::Result<void> SetState(DispenserState state);
    DispenserState GetState() const;
    Siligen::Shared::Types::Result<bool> CanDispense() const;
    bool HasError() const;
    Siligen::Shared::Types::Result<void> ClearError();

    Siligen::Shared::Types::Result<void> AddTask(const DispensingTask& task);
    Siligen::Shared::Types::Result<void> StartTask(const std::string& task_id);
    Siligen::Shared::Types::Result<void> PauseCurrentTask();
    Siligen::Shared::Types::Result<void> ResumeCurrentTask();
    Siligen::Shared::Types::Result<void> CancelTask(const std::string& task_id);
    Siligen::Shared::Types::Result<void> CompleteCurrentTask();
    Siligen::Shared::Types::Result<DispensingTask> GetCurrentTask() const;
    Siligen::Shared::Types::Result<std::vector<DispensingTask>> GetPendingTasks() const;
    Siligen::Shared::Types::Result<int32_t> GetTaskQueueSize() const;
    Siligen::Shared::Types::Result<void> ClearAllTasks();

    Siligen::Shared::Types::Result<void> SetMotionConfig(const Siligen::Shared::Types::MotionConfig& config);
    const Siligen::Shared::Types::MotionConfig& GetMotionConfig() const;
    Siligen::Shared::Types::Result<void> SetConnectionConfig(
        const Siligen::Shared::Types::ConnectionConfig& config);
    const Siligen::Shared::Types::ConnectionConfig& GetConnectionConfig() const;

    int32_t GetCompletedTaskCount() const;
    double GetTotalRunningTime() const;
    double GetTotalDispensingLength() const;
    Siligen::Shared::Types::Result<void> ResetStatistics();

    static Siligen::Shared::Types::Result<void> ValidateStateTransition(
        DispenserState from_state,
        DispenserState to_state);
    Siligen::Shared::Types::Result<void> ValidateTaskStart(const DispensingTask& task) const;

    std::string ToJson() const;
    std::string ToString() const;

   private:
    DispenserState current_state_;
    DispensingTask current_task_;
    std::vector<DispensingTask> pending_tasks_;
    std::vector<DispensingTask> completed_tasks_;
    Siligen::Shared::Types::MotionConfig motion_config_;
    Siligen::Shared::Types::ConnectionConfig connection_config_;
    int32_t total_completed_tasks_;
    double total_running_time_;
    double total_dispensing_length_;
    std::chrono::system_clock::time_point model_created_at_;

    static constexpr int32_t MAX_PENDING_TASKS = 100;
    static constexpr int32_t MAX_COMPLETED_TASKS_HISTORY = 100;

    bool IsStateTransitionValid(DispenserState from_state, DispenserState to_state) const;
    void UpdateStatistics(const DispensingTask& completed_task);
    double CalculatePathLength(const std::vector<Siligen::Shared::Types::Point2D>& path) const;
};

}  // namespace Siligen::RuntimeExecution::Application::System
