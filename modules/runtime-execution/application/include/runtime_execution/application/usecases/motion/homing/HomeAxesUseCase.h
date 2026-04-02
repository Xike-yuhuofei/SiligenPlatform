#pragma once

#include "runtime_execution/contracts/configuration/IConfigurationPort.h"
#include "domain/motion/ports/IHomingPort.h"
#include "domain/motion/ports/IMotionConnectionPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "domain/system/ports/IEventPublisherPort.h"
#include "runtime_execution/contracts/motion/IHomeAxesExecutionPort.h"
#include "shared/types/AxisTypes.h"
#include "shared/types/Result.h"

#include <memory>
#include <vector>

namespace Siligen::Application::UseCases::Motion::Homing {

using HomeAxesRequest = Siligen::RuntimeExecution::Contracts::Motion::HomeAxesExecutionRequest;
using HomeAxesResponse = Siligen::RuntimeExecution::Contracts::Motion::HomeAxesExecutionResponse;

}  // namespace Siligen::Application::UseCases::Motion::Homing

namespace Siligen::RuntimeExecution::Application::Services::Motion {
class HomingProcess;
}

namespace Siligen::Application::UseCases::Motion::Homing {

class HomeAxesUseCase : public Siligen::RuntimeExecution::Contracts::Motion::IHomeAxesExecutionPort {
   public:
    explicit HomeAxesUseCase(std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port,
                             std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
                             std::shared_ptr<Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port,
                             std::shared_ptr<Domain::System::Ports::IEventPublisherPort> event_port,
                             std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port = nullptr);

    ~HomeAxesUseCase() = default;

    HomeAxesUseCase(const HomeAxesUseCase&) = delete;
    HomeAxesUseCase& operator=(const HomeAxesUseCase&) = delete;
    HomeAxesUseCase(HomeAxesUseCase&&) = delete;
    HomeAxesUseCase& operator=(HomeAxesUseCase&&) = delete;

    Result<HomeAxesResponse> Execute(const HomeAxesRequest& request) override;
    Result<void> StopHoming();
    Result<void> StopHomingAxes(const std::vector<Siligen::Shared::Types::LogicalAxisId>& axes);
    Result<bool> IsAxisHomed(Siligen::Shared::Types::LogicalAxisId axis) const;

   private:
    std::shared_ptr<Siligen::RuntimeExecution::Application::Services::Motion::HomingProcess> homing_process_;
    std::shared_ptr<Domain::System::Ports::IEventPublisherPort> event_port_;

    void PublishHomingEvent(Siligen::Shared::Types::LogicalAxisId axis, bool success);
};

}  // namespace Siligen::Application::UseCases::Motion::Homing
