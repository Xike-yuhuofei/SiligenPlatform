#include "runtime/supervision/WorkflowRuntimeSupervisionPort.h"

#include "runtime/supervision/RuntimeExecutionSupervisionBackend.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "shared/types/Error.h"

#include <utility>

namespace Siligen::Runtime::Service::Supervision {
namespace {

using IRuntimeSupervisionPort = Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort;
using RuntimeSupervisionSnapshot = Siligen::RuntimeExecution::Contracts::System::RuntimeSupervisionSnapshot;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

constexpr const char* kErrorSource = "WorkflowRuntimeSupervisionPort";

bool IsTerminalJobState(const std::string& state) {
    return state == "completed" || state == "failed" || state == "cancelled";
}

Result<std::shared_ptr<IRuntimeSupervisionPort>> EnsureInnerPort(
    const std::shared_ptr<IRuntimeSupervisionPort>& inner_port) {
    if (!inner_port) {
        return Result<std::shared_ptr<IRuntimeSupervisionPort>>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "runtime supervision port not initialized",
            kErrorSource));
    }
    return Result<std::shared_ptr<IRuntimeSupervisionPort>>::Success(inner_port);
}

}  // namespace

WorkflowRuntimeSupervisionPort::WorkflowRuntimeSupervisionPort(
    std::shared_ptr<IRuntimeSupervisionPort> inner_port,
    std::shared_ptr<Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase>
        dispensing_execution_use_case,
    std::shared_ptr<IRuntimeJobTerminalSync> terminal_job_sync)
    : inner_port_(std::move(inner_port)),
      dispensing_execution_use_case_(std::move(dispensing_execution_use_case)),
      terminal_job_sync_(std::move(terminal_job_sync)) {}

void WorkflowRuntimeSupervisionPort::RememberObservedJob(const std::string& job_id) const {
    std::lock_guard<std::mutex> lock(observed_job_mutex_);
    last_observed_job_id_ = job_id;
}

void WorkflowRuntimeSupervisionPort::ClearObservedJob() const {
    std::lock_guard<std::mutex> lock(observed_job_mutex_);
    last_observed_job_id_.clear();
}

void WorkflowRuntimeSupervisionPort::SyncTerminalJobIfNeeded() const {
    std::string observed_job_id;
    {
        std::lock_guard<std::mutex> lock(observed_job_mutex_);
        observed_job_id = last_observed_job_id_;
    }
    if (observed_job_id.empty()) {
        return;
    }
    if (!dispensing_execution_use_case_ || !terminal_job_sync_) {
        ClearObservedJob();
        return;
    }

    auto job_status_result = dispensing_execution_use_case_->GetJobStatus(observed_job_id);
    if (job_status_result.IsError()) {
        ClearObservedJob();
        return;
    }

    if (!IsTerminalJobState(job_status_result.Value().state)) {
        return;
    }

    terminal_job_sync_->OnTerminalJobObserved(observed_job_id);
    ClearObservedJob();
}

Result<RuntimeSupervisionSnapshot> WorkflowRuntimeSupervisionPort::ReadSnapshot() const {
    auto inner_port_result = EnsureInnerPort(inner_port_);
    if (inner_port_result.IsError()) {
        return Result<RuntimeSupervisionSnapshot>::Failure(inner_port_result.GetError());
    }

    auto snapshot_result = inner_port_->ReadSnapshot();
    if (snapshot_result.IsError()) {
        return snapshot_result;
    }

    auto snapshot = snapshot_result.Value();
    if (!snapshot.active_job_id.empty()) {
        RememberObservedJob(snapshot.active_job_id);
    } else {
        SyncTerminalJobIfNeeded();
    }

    return Result<RuntimeSupervisionSnapshot>::Success(std::move(snapshot));
}

}  // namespace Siligen::Runtime::Service::Supervision
