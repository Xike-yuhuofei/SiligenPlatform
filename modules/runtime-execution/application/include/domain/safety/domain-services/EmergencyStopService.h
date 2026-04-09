#pragma once

#include "domain/dispensing/domain-services/CMPTriggerService.h"
#include "runtime_execution/contracts/motion/MotionControlService.h"
#include "runtime_execution/contracts/motion/MotionStatusService.h"
#include "runtime_execution/contracts/system/IMachineExecutionStatePort.h"
#include "shared/types/Error.h"
#include "shared/types/Point2D.h"
#include "shared/types/Result.h"

#include <cstdint>
#include <memory>

namespace Siligen::Domain::Safety::DomainServices {

using Siligen::Domain::Dispensing::DomainServices::CMPService;
using Siligen::Domain::Motion::DomainServices::MotionControlService;
using Siligen::Domain::Motion::DomainServices::MotionStatusService;
using Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;

enum class EmergencyStopFailureKind : uint8_t {
    None = 0,
    DependencyMissing = 1,
    OperationFailed = 2
};

struct EmergencyStopStepResult {
    bool attempted = false;
    bool success = false;
    EmergencyStopFailureKind failure = EmergencyStopFailureKind::None;
    Error error;
};

struct EmergencyStopOutcome {
    EmergencyStopStepResult motion_stop;
    EmergencyStopStepResult cmp_disable;
    EmergencyStopStepResult task_queue_clear;
    EmergencyStopStepResult hardware_disable;
    EmergencyStopStepResult state_update;

    bool task_queue_size_available = false;
    int32_t task_queue_size = 0;

    bool stop_position_available = false;
    Point2D stop_position{};
};

struct EmergencyStopOptions {
    bool disable_cmp = true;
    bool clear_task_queue = true;
    bool disable_hardware = true;
};

class EmergencyStopService {
   public:
    EmergencyStopService(std::shared_ptr<MotionControlService> motion_control_service,
                         std::shared_ptr<MotionStatusService> motion_status_service,
                         std::shared_ptr<CMPService> cmp_service,
                         std::shared_ptr<IMachineExecutionStatePort> machine_execution_state_port) noexcept;

    EmergencyStopOutcome Execute(const EmergencyStopOptions& options) noexcept;
    Result<bool> IsInEmergencyStop() const noexcept;
    Result<void> RecoverFromEmergencyStop() noexcept;

   private:
    std::shared_ptr<MotionControlService> motion_control_service_;
    std::shared_ptr<MotionStatusService> motion_status_service_;
    std::shared_ptr<CMPService> cmp_service_;
    std::shared_ptr<IMachineExecutionStatePort> machine_execution_state_port_;

    static EmergencyStopStepResult DependencyMissingResult(const char* message) noexcept;
    static EmergencyStopStepResult OperationFailedResult(const Error& error) noexcept;
    static EmergencyStopStepResult SuccessResult() noexcept;
};

}  // namespace Siligen::Domain::Safety::DomainServices
