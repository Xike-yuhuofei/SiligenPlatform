#pragma once

#include "runtime_execution/contracts/system/IEventPublisherPort.h"
#include "domain/machine/ports/IHardwareConnectionPort.h"
#include "domain/motion/ports/IInterpolationPort.h"
#include "domain/motion/domain-services/interpolation/TrajectoryInterpolatorBase.h"
#include "domain/dispensing/ports/IValvePort.h"
#include "domain/configuration/ports/IConfigurationPort.h"
#include "runtime_execution/contracts/dispensing/ITaskSchedulerPort.h"
#include "domain/dispensing/value-objects/DispensingExecutionTypes.h"
#include "domain/dispensing/value-objects/QualityMetrics.h"
#include "application/services/dispensing/DispensePlanningFacade.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

namespace Siligen {
namespace Domain {
namespace Dispensing {
namespace DomainServices {
class DispensingProcessService;
}  // namespace DomainServices
}  // namespace Dispensing
}  // namespace Domain
}  // namespace Siligen

namespace Siligen::Application::Services::DXF {
class DxfPbPreparationService;
}

namespace Siligen::Application::UseCases::Dispensing {

namespace DomainServices = Siligen::Domain::Dispensing::DomainServices;

using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::int32;
using Siligen::Domain::Dispensing::DomainServices::DispensingPlan;
using Siligen::Domain::Dispensing::DomainServices::DispensingPlanRequest;
using Siligen::Domain::Dispensing::ValueObjects::JobExecutionMode;
using Siligen::Domain::Dispensing::ValueObjects::ProcessOutputPolicy;
using Siligen::Domain::Machine::ValueObjects::MachineMode;
using RuntimeEventPublisherPort = Siligen::RuntimeExecution::Contracts::System::IEventPublisherPort;
using RuntimeTaskSchedulerPort = Siligen::RuntimeExecution::Contracts::Dispensing::ITaskSchedulerPort;
using DispensingPlanner = Siligen::Application::Services::Dispensing::DispensePlanningFacade;

struct DispensingMVPRequest {
    std::string dxf_filepath;
    bool optimize_path = false;
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
    bool dry_run = false;
    std::optional<MachineMode> machine_mode;
    std::optional<JobExecutionMode> execution_mode;
    std::optional<ProcessOutputPolicy> output_policy;
    float32 dispensing_velocity = 0.0f;
    float32 dry_run_velocity = 0.0f;
    float32 max_jerk = 0.0f;
    float32 arc_tolerance_mm = 0.0f;

    bool use_interpolation_planner = false;
    Domain::Motion::InterpolationAlgorithm interpolation_algorithm = Domain::Motion::InterpolationAlgorithm::LINEAR;

    std::optional<float32> dispensing_speed_mm_s;
    std::optional<float32> dry_run_speed_mm_s;
    std::optional<float32> rapid_speed_mm_s;
    std::optional<float32> acceleration_mm_s2;
    bool velocity_trace_enabled = false;
    int32 velocity_trace_interval_ms = 0;
    std::string velocity_trace_path;
    bool velocity_guard_enabled = true;
    float32 velocity_guard_ratio = 0.3f;
    float32 velocity_guard_abs_mm_s = 5.0f;
    float32 velocity_guard_min_expected_mm_s = 5.0f;
    int32 velocity_guard_grace_ms = 800;
    int32 velocity_guard_interval_ms = 200;
    int32 velocity_guard_max_consecutive = 3;
    bool velocity_guard_stop_on_violation = false;

    static constexpr float32 ACCELERATION = 100.0f;
    static constexpr float32 PULSE_PER_MM = 200.0f;
    static constexpr float32 JERK = 5000.0f;
    static constexpr uint32 DISPENSER_INTERVAL_MS = 100;
    static constexpr uint32 DISPENSER_DURATION_MS = 100;
    static constexpr float32 TRIGGER_SPATIAL_INTERVAL_MM = 3.0f;
    static constexpr uint32 SUPPLY_VALVE_STABILIZATION_MS = 500;
    static constexpr uint32 MOTION_COMPLETION_TOLERANCE_MS = 100;
    static constexpr uint32 STATUS_POLL_INTERVAL_MS = 50;

    Shared::Types::Result<void> Validate() const noexcept;
    MachineMode ResolveMachineMode() const noexcept;
    JobExecutionMode ResolveExecutionMode() const noexcept;
    ProcessOutputPolicy ResolveOutputPolicy() const noexcept;
};

struct DispensingMVPResult {
    bool success = false;
    std::string message;

    uint32 total_segments = 0;
    uint32 executed_segments = 0;
    uint32 failed_segments = 0;
    float32 total_distance = 0.0f;
    float32 execution_time_seconds = 0.0f;

    std::string error_details;

    Domain::Dispensing::ValueObjects::QualityMetrics quality_metrics;
    bool quality_metrics_available = false;
};

using TaskID = std::string;

enum class TaskState {
    PENDING,
    RUNNING,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELLED
};

struct TaskExecutionContext {
    TaskID task_id;
    std::atomic<TaskState> state{TaskState::PENDING};
    std::atomic<TaskState> committed_terminal_state{TaskState::PENDING};
    DispensingMVPRequest request;
    DispensingMVPResult result;

    std::atomic<uint32> total_segments{0};
    std::atomic<uint32> executed_segments{0};
    std::atomic<uint32> reported_progress_percent{0};
    std::atomic<uint32> reported_executed_segments{0};
    std::atomic<uint32> estimated_execution_ms{0};
    std::atomic<bool> cancel_requested{false};
    std::atomic<bool> pause_requested{false};
    std::atomic<bool> pause_applied{false};
    std::atomic<bool> terminal_committed{false};
    std::atomic<bool> execution_started{false};
    std::atomic<bool> inflight_registered{false};
    std::atomic<bool> inflight_released{false};

    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    mutable std::mutex mutex_;
    std::string scheduler_task_id;
    std::string error_message;
};

struct TaskStatusResponse {
    TaskID task_id;
    std::string state;
    uint32 progress_percent = 0;
    uint32 executed_segments = 0;
    uint32 total_segments = 0;
    float32 elapsed_seconds = 0.0f;
    std::string error_message;
};

class DispensingExecutionUseCase {
   public:
    explicit DispensingExecutionUseCase(
        std::shared_ptr<DispensingPlanner> planner,
        std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port,
        std::shared_ptr<Domain::Motion::Ports::IInterpolationPort> interpolation_port,
        std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port,
        std::shared_ptr<Domain::Machine::Ports::IHardwareConnectionPort> connection_port,
        std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
        std::shared_ptr<RuntimeEventPublisherPort> event_port = nullptr,
        std::shared_ptr<RuntimeTaskSchedulerPort> task_scheduler_port = nullptr,
        std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService>
            pb_preparation_service = nullptr);

    ~DispensingExecutionUseCase();

    DispensingExecutionUseCase(const DispensingExecutionUseCase&) = delete;
    DispensingExecutionUseCase& operator=(const DispensingExecutionUseCase&) = delete;

    Shared::Types::Result<DispensingMVPResult> Execute(const DispensingMVPRequest& request);
    void StopExecution();

    Shared::Types::Result<TaskID> ExecuteAsync(const DispensingMVPRequest& request);
    Shared::Types::Result<TaskStatusResponse> GetTaskStatus(const TaskID& task_id) const;
    Shared::Types::Result<void> PauseTask(const TaskID& task_id);
    Shared::Types::Result<void> ResumeTask(const TaskID& task_id);
    Shared::Types::Result<void> CancelTask(const TaskID& task_id);
    void CleanupExpiredTasks();

   private:
    std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port_;
    std::shared_ptr<Domain::Motion::Ports::IInterpolationPort> interpolation_port_;
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port_;
    std::shared_ptr<Domain::Machine::Ports::IHardwareConnectionPort> connection_port_;
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port_;
    std::shared_ptr<RuntimeEventPublisherPort> event_port_;
    std::shared_ptr<RuntimeTaskSchedulerPort> task_scheduler_port_;
    std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService> pb_preparation_service_;

    std::shared_ptr<DispensingPlanner> planner_;
    std::shared_ptr<::Siligen::Domain::Dispensing::DomainServices::DispensingProcessService> process_service_;

    struct VelocityTraceSettings {
        bool enabled = false;
        int32 interval_ms = 0;
        std::string output_path;
    };

    struct ResolvedExecutionProfile {
        MachineMode machine_mode = MachineMode::Production;
        JobExecutionMode execution_mode = JobExecutionMode::Production;
        ProcessOutputPolicy output_policy = ProcessOutputPolicy::Enabled;
        Domain::Dispensing::ValueObjects::GuardDecision guard_decision{};
    };

    Domain::Dispensing::ValueObjects::DispensingRuntimeParams runtime_params_;
    ResolvedExecutionProfile resolved_execution_;

    std::atomic<bool> stop_requested_{false};
    std::mutex execution_mutex_;
    mutable std::mutex worker_mutex_;
    std::thread worker_thread_;
    std::atomic<std::uint64_t> task_sequence_{0};
    std::atomic<std::uint32_t> inflight_tasks_{0};
    mutable std::mutex inflight_mutex_;
    std::condition_variable inflight_cv_;

    std::unordered_map<TaskID, std::shared_ptr<TaskExecutionContext>> tasks_;
    mutable std::mutex tasks_mutex_;
    TaskID active_task_id_;

    Shared::Types::Result<void> ValidateHardwareConnection() noexcept;
    Shared::Types::Result<void> RefreshRuntimeParameters(const DispensingMVPRequest& request) noexcept;
    DispensingPlanRequest BuildPlanRequest(const DispensingMVPRequest& request) const noexcept;
    Shared::Types::Result<DispensingMVPResult> ExecuteInternal(
        const DispensingMVPRequest& request,
        const std::shared_ptr<TaskExecutionContext>& context);
    static bool IsTerminalState(TaskState state);
    static bool TryCommitTerminalState(
        const std::shared_ptr<TaskExecutionContext>& context,
        TaskState terminal_state,
        const std::string& error_message);
    static TaskState ResolveVisibleState(const std::shared_ptr<TaskExecutionContext>& context);
    std::shared_ptr<TaskExecutionContext> ResolveActiveContextLocked() const;
    void JoinWorkerThread();
    void RegisterTaskInflight(const std::shared_ptr<TaskExecutionContext>& context);
    void ReleaseTaskInflight(const std::shared_ptr<TaskExecutionContext>& context);
    bool WaitForTaskTerminalState(
        const std::shared_ptr<TaskExecutionContext>& context,
        std::chrono::milliseconds timeout,
        TaskState* terminal_state_out = nullptr) const;
    bool WaitForAllInflightTasks(
        std::chrono::milliseconds timeout,
        std::string* diagnostics_out = nullptr);
    void ReconcileStalledInflightTasks();
    std::string BuildInflightDiagnostics() const;
    std::string ReadTaskErrorMessage(const std::shared_ptr<TaskExecutionContext>& context) const;

    TaskID GenerateTaskID();
    std::string TaskStateToString(TaskState state) const;
};

}  // namespace Siligen::Application::UseCases::Dispensing
