#pragma once

#include "runtime_execution/contracts/configuration/IConfigurationPort.h"
#include "runtime_execution/application/services/motion/ReadyZeroDecisionService.h"
#include "runtime_execution/application/usecases/motion/homing/HomeAxesUseCase.h"
#include "runtime_execution/application/usecases/motion/manual/ManualMotionControlUseCase.h"
#include "runtime_execution/application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
#include "shared/types/AxisTypes.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>
#include <vector>

namespace Siligen::Application::UseCases::Motion::Homing {

using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

struct EnsureAxesReadyZeroRequest {
    std::vector<LogicalAxisId> axes;
    bool home_all_axes = false;
    bool wait_for_completion = true;
    int32 timeout_ms = 30000;
    bool force_rehome = false;

    bool Validate() const {
        if (home_all_axes && !axes.empty()) {
            return false;
        }
        return timeout_ms > 0 && timeout_ms <= 300000;
    }
};

struct EnsureAxesReadyZeroResponse {
    struct AxisResult {
        LogicalAxisId axis = LogicalAxisId::INVALID;
        std::string supervisor_state;
        std::string planned_action;
        bool executed = false;
        bool success = false;
        std::string reason_code;
        std::string message;
    };

    bool accepted = false;
    std::string summary_state;
    std::string message;
    std::vector<AxisResult> axis_results;
    int32 total_time_ms = 0;
};

class EnsureAxesReadyZeroUseCase {
   public:
    EnsureAxesReadyZeroUseCase(
        std::shared_ptr<HomeAxesUseCase> home_use_case,
        std::shared_ptr<Manual::ManualMotionControlUseCase> manual_motion_use_case,
        std::shared_ptr<Monitoring::MotionMonitoringUseCase> monitoring_use_case,
        std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
        std::shared_ptr<Siligen::RuntimeExecution::Application::Services::Motion::ReadyZeroDecisionService>
            decision_service);

    ~EnsureAxesReadyZeroUseCase() = default;

    EnsureAxesReadyZeroUseCase(const EnsureAxesReadyZeroUseCase&) = delete;
    EnsureAxesReadyZeroUseCase& operator=(const EnsureAxesReadyZeroUseCase&) = delete;
    EnsureAxesReadyZeroUseCase(EnsureAxesReadyZeroUseCase&&) = delete;
    EnsureAxesReadyZeroUseCase& operator=(EnsureAxesReadyZeroUseCase&&) = delete;

    Result<EnsureAxesReadyZeroResponse> Execute(const EnsureAxesReadyZeroRequest& request);

   private:
    std::shared_ptr<HomeAxesUseCase> home_use_case_;
    std::shared_ptr<Manual::ManualMotionControlUseCase> manual_motion_use_case_;
    std::shared_ptr<Monitoring::MotionMonitoringUseCase> monitoring_use_case_;
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port_;
    std::shared_ptr<Siligen::RuntimeExecution::Application::Services::Motion::ReadyZeroDecisionService>
        decision_service_;
};

}  // namespace Siligen::Application::UseCases::Motion::Homing
