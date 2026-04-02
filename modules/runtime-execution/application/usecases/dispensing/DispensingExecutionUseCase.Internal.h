#pragma once

#include "ExecutionObserverSupport.h"
#include "ExecutionSessionStore.h"
#include "ExecutionWorkerCoordinator.h"

#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"

#include <memory>
#include <string>

namespace Siligen::Application::UseCases::Dispensing {

using Siligen::Shared::Types::LogicalAxisId;

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
        std::shared_ptr<RuntimeInterlockSignalPort> interlock_signal_port);

    ~Impl();

    Shared::Types::Result<DispensingExecutionResult> Execute(const DispensingExecutionRequest& request);
    Shared::Types::Result<JobID> StartJob(const RuntimeStartJobRequest& request);
    Shared::Types::Result<RuntimeJobStatusResponse> GetJobStatus(const JobID& job_id) const;
    JobID GetActiveJobId() const;
    Shared::Types::Result<void> PauseJob(const JobID& job_id);
    Shared::Types::Result<void> ResumeJob(const JobID& job_id);
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
    ExecutionWorkerCoordinator worker_coordinator_;
    ExecutionSessionStore session_store_;

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
    Shared::Types::Result<TaskStatusResponse> GetTaskStatus(const TaskID& task_id) const;
    Shared::Types::Result<void> PauseTask(const TaskID& task_id);
    Shared::Types::Result<void> ResumeTask(const TaskID& task_id);
    Shared::Types::Result<void> CancelTask(const TaskID& task_id);
    void CleanupExpiredTasks();
    void StopExecution();

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
