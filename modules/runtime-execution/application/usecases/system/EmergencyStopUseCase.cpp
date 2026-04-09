#include "EmergencyStopUseCase.h"

#include "shared/types/Types.h"

namespace Siligen::Application::UseCases::System {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

EmergencyStopUseCase::EmergencyStopUseCase(
    std::shared_ptr<Siligen::Domain::Motion::DomainServices::MotionControlService> motion_control_service,
    std::shared_ptr<Siligen::Domain::Motion::DomainServices::MotionStatusService> motion_status_service,
    std::shared_ptr<Siligen::Domain::Dispensing::DomainServices::CMPService> cmp_service,
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort> machine_execution_state_port,
    std::shared_ptr<Siligen::Shared::Interfaces::ILoggingService> logging_service)
    : emergency_stop_service_(std::make_shared<Siligen::Domain::Safety::DomainServices::EmergencyStopService>(
          std::move(motion_control_service),
          std::move(motion_status_service),
          std::move(cmp_service),
          std::move(machine_execution_state_port))),
      logging_service_(std::move(logging_service)) {}

Result<EmergencyStopResponse> EmergencyStopUseCase::Execute(const EmergencyStopRequest& request) {
    auto start_time = std::chrono::steady_clock::now();
    EmergencyStopResponse response;

    if (emergency_stop_service_) {
        Siligen::Domain::Safety::DomainServices::EmergencyStopOptions options;
        options.disable_cmp = request.disable_cmp;
        options.clear_task_queue = request.clear_task_queue;
        options.disable_hardware = request.disable_hardware;

        auto outcome = emergency_stop_service_->Execute(options);

        auto log_failure = [this](const Siligen::Domain::Safety::DomainServices::EmergencyStopStepResult& step,
                                  const char* operation_failed_prefix) {
            if (step.failure == Siligen::Domain::Safety::DomainServices::EmergencyStopFailureKind::DependencyMissing) {
                LogError(step.error.GetMessage());
            } else if (step.failure == Siligen::Domain::Safety::DomainServices::EmergencyStopFailureKind::OperationFailed) {
                LogError(std::string(operation_failed_prefix) + step.error.GetMessage());
            }
        };

        if (outcome.motion_stop.success) {
            response.motion_stopped = true;
            response.actions_taken.push_back("motion_emergency_stop");
        }
        log_failure(outcome.motion_stop, "Motion emergency stop failed: ");

        if (outcome.cmp_disable.success) {
            response.cmp_disabled = true;
            response.actions_taken.push_back("cmp_disabled");
        }
        log_failure(outcome.cmp_disable, "Disable CMP failed: ");

        if (outcome.task_queue_size_available) {
            response.tasks_cleared = outcome.task_queue_size;
        }
        if (outcome.task_queue_clear.success) {
            response.actions_taken.push_back("task_queue_cleared");
        }
        log_failure(outcome.task_queue_clear, "Clear task queue failed: ");

        if (outcome.hardware_disable.success) {
            response.hardware_disabled = true;
            response.actions_taken.push_back("hardware_disabled");
        }
        log_failure(outcome.hardware_disable, "Disable hardware failed: ");

        if (outcome.state_update.success) {
            response.actions_taken.push_back("state_emergency_stop");
        }
        log_failure(outcome.state_update, "Update dispenser state failed: ");

        if (outcome.stop_position_available) {
            response.stop_position = outcome.stop_position;
        }
    } else {
        LogError("Emergency stop service not initialized");
    }

    response.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);
    response.status_message = "Emergency stop executed";

    LogEmergencyStop(request, response);
    return Result<EmergencyStopResponse>::Success(response);
}

Result<bool> EmergencyStopUseCase::IsInEmergencyStop() const {
    if (!emergency_stop_service_) {
        return Result<bool>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Emergency stop service not initialized", "EmergencyStopUseCase"));
    }

    auto result = emergency_stop_service_->IsInEmergencyStop();
    if (result.IsError()) {
        return Result<bool>::Failure(
            Error(result.GetError().GetCode(), result.GetError().GetMessage(), "EmergencyStopUseCase"));
    }
    return result;
}

Result<void> EmergencyStopUseCase::RecoverFromEmergencyStop() {
    if (!emergency_stop_service_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Emergency stop service not initialized", "EmergencyStopUseCase"));
    }

    auto result = emergency_stop_service_->RecoverFromEmergencyStop();
    if (result.IsError()) {
        return Result<void>::Failure(
            Error(result.GetError().GetCode(), result.GetError().GetMessage(), "EmergencyStopUseCase"));
    }
    return result;
}

void EmergencyStopUseCase::LogEmergencyStop(const EmergencyStopRequest& request,
                                            const EmergencyStopResponse& response) {
    if (!logging_service_) {
        return;
    }

    std::string message = "Emergency stop: reason=" + std::string(EmergencyStopReasonToString(request.reason)) +
                          ", actions=" + std::to_string(response.actions_taken.size());
    logging_service_->LogCritical(message, "EmergencyStopUseCase");
}

void EmergencyStopUseCase::LogInfo(const std::string& message) {
    if (logging_service_) {
        logging_service_->LogInfo(message, "EmergencyStopUseCase");
    }
}

void EmergencyStopUseCase::LogError(const std::string& message) {
    if (logging_service_) {
        logging_service_->LogError(message, "EmergencyStopUseCase");
    }
}

void EmergencyStopUseCase::LogCritical(const std::string& message) {
    if (logging_service_) {
        logging_service_->LogCritical(message, "EmergencyStopUseCase");
    }
}

}  // namespace Siligen::Application::UseCases::System
