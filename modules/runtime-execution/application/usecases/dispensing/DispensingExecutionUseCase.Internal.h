#pragma once

#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace Siligen::Application::UseCases::Dispensing {

using Siligen::Shared::Types::LogicalAxisId;

using TaskID = std::string;
using SharedExecutionRequest = std::shared_ptr<const DispensingExecutionRequest>;

enum class TaskState {
    PENDING,
    RUNNING,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELLED
};

enum class JobState {
    PENDING,
    RUNNING,
    WAITING_CONTINUE,
    STOPPING,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELLED
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

struct TaskExecutionContext {
    TaskID task_id;
    std::atomic<TaskState> state{TaskState::PENDING};
    std::atomic<TaskState> committed_terminal_state{TaskState::PENDING};
    SharedExecutionRequest request;
    DispensingExecutionResult result;

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

    std::chrono::steady_clock::time_point start_time{};
    std::chrono::steady_clock::time_point end_time{};
    mutable std::mutex mutex_;
    std::string scheduler_task_id;
    std::string error_message;
};

struct JobExecutionContext {
    JobID job_id;
    std::string plan_id;
    std::string plan_fingerprint;
    SharedExecutionRequest execution_request;
    std::atomic<JobState> state{JobState::PENDING};
    std::atomic<ExecutionTransitionState> requested_transition_state{ExecutionTransitionState::PENDING};
    std::atomic<uint32> target_count{0};
    std::atomic<uint32> completed_count{0};
    std::atomic<uint32> current_cycle{0};
    std::atomic<uint32> current_segment{0};
    std::atomic<uint32> total_segments{0};
    std::atomic<uint32> cycle_progress_percent{0};
    ExecutionBudgetBreakdown execution_budget_breakdown{};
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> pause_requested{false};
    std::atomic<bool> continue_requested{false};
    std::atomic<bool> final_state_committed{false};
    JobCycleAdvanceMode cycle_advance_mode = JobCycleAdvanceMode::WAIT_FOR_CONTINUE;
    bool dry_run = false;
    std::chrono::steady_clock::time_point start_time{};
    std::chrono::steady_clock::time_point end_time{};
    mutable std::mutex mutex_;
    std::string active_task_id;
    std::string error_message;
};

struct DispensingExecutionUseCase::Impl {
    explicit Impl(
        std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port,
        std::shared_ptr<Domain::Motion::Ports::IInterpolationPort> interpolation_port,
        std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port,
        std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
        std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
        std::shared_ptr<RuntimeDispensingProcessPort> process_port,
        std::shared_ptr<RuntimeEventPublisherPort> event_port,
        std::shared_ptr<RuntimeTaskSchedulerPort> task_scheduler_port,
        std::shared_ptr<RuntimeHomingPort> homing_port,
        std::shared_ptr<RuntimeInterlockSignalPort> interlock_signal_port,
        std::shared_ptr<Siligen::Application::Services::Motion::Execution::MotionReadinessService>
            readiness_service);

    ~Impl();

    Shared::Types::Result<DispensingExecutionResult> Execute(const DispensingExecutionRequest& request);
    Shared::Types::Result<JobID> StartJob(const RuntimeStartJobRequest& request);
    Shared::Types::Result<RuntimeJobStatusResponse> GetJobStatus(const JobID& job_id) const;
    Shared::Types::Result<ExecutionTransitionSnapshot> RequestJobTransition(
        const JobID& job_id,
        ExecutionTransitionState requested_transition_state);
    JobID GetActiveJobId() const;
    Shared::Types::Result<void> PauseJob(const JobID& job_id);
    Shared::Types::Result<void> ResumeJob(const JobID& job_id);
    Shared::Types::Result<void> ContinueJob(const JobID& job_id);
    Shared::Types::Result<void> StopJob(const JobID& job_id);

#ifdef SILIGEN_TEST_HOOKS
    void SeedJobStateForTesting(const RuntimeJobStatusResponse& status, bool pause_requested = false);
    void SetActiveJobForTesting(const JobID& job_id);
#endif

    std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port_;
    std::shared_ptr<Domain::Motion::Ports::IInterpolationPort> interpolation_port_;
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port_;
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port_;
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port_;
    std::shared_ptr<RuntimeDispensingProcessPort> process_port_;
    std::shared_ptr<RuntimeEventPublisherPort> event_port_;
    std::shared_ptr<RuntimeTaskSchedulerPort> task_scheduler_port_;
    std::shared_ptr<RuntimeHomingPort> homing_port_;
    std::shared_ptr<RuntimeInterlockSignalPort> interlock_signal_port_;
    std::shared_ptr<Siligen::Application::Services::Motion::Execution::MotionReadinessService>
        readiness_service_;

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
    mutable std::mutex job_worker_mutex_;
    std::thread job_worker_thread_;
    std::atomic<std::uint64_t> task_sequence_{0};
    std::atomic<std::uint64_t> job_sequence_{0};
    std::atomic<std::uint32_t> inflight_tasks_{0};
    mutable std::mutex inflight_mutex_;
    std::condition_variable inflight_cv_;

    mutable std::unordered_map<TaskID, std::shared_ptr<TaskExecutionContext>> tasks_;
    mutable std::mutex tasks_mutex_;
    mutable TaskID active_task_id_;
    mutable std::unordered_map<JobID, std::shared_ptr<JobExecutionContext>> jobs_;
    mutable std::mutex jobs_mutex_;
    mutable JobID active_job_id_;

    Shared::Types::Result<void> ValidateHardwareConnection(bool allow_disconnected = false) noexcept;
    Shared::Types::Result<void> ValidateExecutionPreconditions(bool allow_disconnected = false) const noexcept;
    Shared::Types::Result<void> RefreshRuntimeParameters(const DispensingExecutionRequest& request) noexcept;
    Shared::Types::Result<DispensingExecutionResult> ExecuteInternal(
        const DispensingExecutionRequest& request,
        const std::shared_ptr<TaskExecutionContext>& context);
    static bool IsTerminalState(TaskState state);
    static bool IsTerminalJobState(JobState state);
    static bool TryCommitTerminalState(
        const std::shared_ptr<TaskExecutionContext>& context,
        TaskState terminal_state,
        const std::string& error_message);
    static bool TryCommitJobTerminalState(
        const std::shared_ptr<JobExecutionContext>& context,
        JobState terminal_state,
        const std::string& error_message);
    static TaskState ResolveVisibleState(const std::shared_ptr<TaskExecutionContext>& context);
    std::shared_ptr<TaskExecutionContext> ResolveActiveContextLocked() const;
    void CleanupTerminalTasksLocked();
    void CleanupTerminalJobsLocked();
    void JoinWorkerThread();
    void JoinJobWorkerThread();
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

    Shared::Types::Result<TaskID> ExecuteAsync(const DispensingExecutionRequest& request);
    Shared::Types::Result<TaskID> ExecuteAsync(SharedExecutionRequest request);
    Shared::Types::Result<TaskStatusResponse> GetTaskStatus(const TaskID& task_id) const;
    Shared::Types::Result<void> PauseTask(const TaskID& task_id);
    Shared::Types::Result<void> ResumeTask(const TaskID& task_id);
    Shared::Types::Result<void> CancelTask(const TaskID& task_id);
    void CleanupExpiredTasks();
    void StopExecution();
    Shared::Types::Result<void> WaitForStopSettle(
        const std::shared_ptr<JobExecutionContext>& context);
    void FinalizeStoppedJob(
        const std::shared_ptr<JobExecutionContext>& context,
        const std::string& error_message);

    TaskID GenerateTaskID();
    JobID GenerateJobID();
    RuntimeJobStatusResponse BuildJobStatusResponse(const std::shared_ptr<JobExecutionContext>& context) const;
    void RunJob(const std::shared_ptr<JobExecutionContext>& context);
    void FinalizeJob(
        const std::shared_ptr<JobExecutionContext>& context,
        JobState final_state,
        const std::string& error_message = std::string());
    std::string TaskStateToString(TaskState state) const;
    std::string JobStateToString(JobState state) const;
};

}  // namespace Siligen::Application::UseCases::Dispensing
