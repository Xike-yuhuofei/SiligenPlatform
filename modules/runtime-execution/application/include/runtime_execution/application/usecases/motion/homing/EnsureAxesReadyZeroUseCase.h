#pragma once

#include "runtime_execution/application/usecases/motion/homing/HomeAxesUseCase.h"
#include "runtime_execution/application/usecases/motion/manual/ManualMotionControlUseCase.h"
#include "runtime_execution/application/services/motion/execution/MotionReadinessService.h"
#include "runtime_execution/application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
#include "runtime_execution/application/usecases/motion/homing/EnsureAxesReadyZeroTypes.h"
#include "domain/motion/domain-services/ReadyZeroDecisionService.h"
#include "process_planning/contracts/configuration/IConfigurationPort.h"

#include <memory>

namespace Siligen::Application::UseCases::Motion::Homing {

class EnsureAxesReadyZeroUseCase {
   public:
    EnsureAxesReadyZeroUseCase(
        std::shared_ptr<HomeAxesUseCase> home_use_case,
        std::shared_ptr<Manual::ManualMotionControlUseCase> manual_motion_use_case,
        std::shared_ptr<Monitoring::MotionMonitoringUseCase> monitoring_use_case,
        std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
        std::shared_ptr<Domain::Motion::DomainServices::ReadyZeroDecisionService> decision_service,
        std::shared_ptr<Siligen::Application::Services::Motion::Execution::MotionReadinessService>
            readiness_service = nullptr);

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
    std::shared_ptr<Domain::Motion::DomainServices::ReadyZeroDecisionService> decision_service_;
    std::shared_ptr<Siligen::Application::Services::Motion::Execution::MotionReadinessService> readiness_service_;
};

}  // namespace Siligen::Application::UseCases::Motion::Homing
