#pragma once

#include "domain/system/ports/IEventPublisherPort.h"
#include "domain/machine/ports/IHardwareConnectionPort.h"
#include "domain/motion/ports/IInterpolationPort.h"
#include "domain/motion/domain-services/interpolation/TrajectoryInterpolatorBase.h"
#include "domain/dispensing/ports/IValvePort.h"
#include "domain/configuration/ports/IConfigurationPort.h"
#include "domain/dispensing/ports/ITaskSchedulerPort.h"
#include "domain/dispensing/value-objects/DispensingExecutionTypes.h"
#include "domain/dispensing/value-objects/QualityMetrics.h"
#include "domain/dispensing/planning/domain-services/DispensingPlannerService.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"

#include <atomic>
#include <chrono>
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
using Siligen::Domain::Dispensing::DomainServices::DispensingPlanner;
using Siligen::Domain::Dispensing::DomainServices::DispensingPlanRequest;

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

    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    mutable std::mutex mutex_;
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
        std::shared_ptr<Domain::System::Ports::IEventPublisherPort> event_port = nullptr,
        std::shared_ptr<Domain::Dispensing::Ports::ITaskSchedulerPort> task_scheduler_port = nullptr,
        std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService>
            pb_preparation_service = nullptr);

    ~DispensingExecutionUseCase() = default;

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
    std::shared_ptr<Domain::System::Ports::IEventPublisherPort> event_port_;
    std::shared_ptr<Domain::Dispensing::Ports::ITaskSchedulerPort> task_scheduler_port_;
    std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService> pb_preparation_service_;

    std::shared_ptr<DispensingPlanner> planner_;
    std::shared_ptr<::Siligen::Domain::Dispensing::DomainServices::DispensingProcessService> process_service_;

    struct VelocityTraceSettings {
        bool enabled = false;
        int32 interval_ms = 0;
        std::string output_path;
    };

    Domain::Dispensing::ValueObjects::DispensingRuntimeParams runtime_params_;

    std::atomic<bool> stop_requested_{false};
    std::mutex execution_mutex_;

    std::unordered_map<TaskID, std::shared_ptr<TaskExecutionContext>> tasks_;
    mutable std::mutex tasks_mutex_;

    Shared::Types::Result<void> ValidateHardwareConnection() noexcept;
    Shared::Types::Result<void> RefreshRuntimeParameters(const DispensingMVPRequest& request) noexcept;
    DispensingPlanRequest BuildPlanRequest(const DispensingMVPRequest& request) const noexcept;
    Shared::Types::Result<DispensingMVPResult> ExecuteInternal(
        const DispensingMVPRequest& request,
        const std::shared_ptr<TaskExecutionContext>& context);

    TaskID GenerateTaskID() const;
    std::string TaskStateToString(TaskState state) const;
};

}  // namespace Siligen::Application::UseCases::Dispensing

