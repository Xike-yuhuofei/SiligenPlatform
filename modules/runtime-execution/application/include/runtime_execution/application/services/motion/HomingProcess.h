#pragma once

#include "runtime_execution/contracts/configuration/IConfigurationPort.h"
#include "domain/motion/ports/IHomingPort.h"
#include "domain/motion/ports/IMotionConnectionPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "runtime_execution/contracts/motion/IHomeAxesExecutionPort.h"
#include "shared/types/AxisTypes.h"
#include "shared/types/Result.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace Siligen::RuntimeExecution::Application::Services::Motion {

using Siligen::Domain::Configuration::Ports::IConfigurationPort;
using Siligen::Domain::Motion::Ports::HomingState;
using Siligen::Domain::Motion::Ports::IHomingPort;
using Siligen::Domain::Motion::Ports::IMotionConnectionPort;
using Siligen::Domain::Motion::Ports::IMotionStatePort;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

using HomeAxesRequest = Siligen::RuntimeExecution::Contracts::Motion::HomeAxesExecutionRequest;
using HomeAxesResponse = Siligen::RuntimeExecution::Contracts::Motion::HomeAxesExecutionResponse;

class HomingProcess final {
   public:
    explicit HomingProcess(std::shared_ptr<IHomingPort> homing_port,
                           std::shared_ptr<IConfigurationPort> config_port,
                           std::shared_ptr<IMotionConnectionPort> motion_connection_port,
                           std::shared_ptr<IMotionStatePort> motion_state_port);

    HomingProcess(const HomingProcess&) = delete;
    HomingProcess& operator=(const HomingProcess&) = delete;
    HomingProcess(HomingProcess&&) = delete;
    HomingProcess& operator=(HomingProcess&&) = delete;

    Result<HomeAxesResponse> Execute(const HomeAxesRequest& request);
    Result<void> StopHoming();
    Result<void> StopHomingAxes(const std::vector<LogicalAxisId>& axes);
    Result<bool> IsAxisHomed(LogicalAxisId axis_id) const;

   private:
    std::shared_ptr<IHomingPort> homing_port_;
    std::shared_ptr<IConfigurationPort> config_port_;
    std::shared_ptr<IMotionConnectionPort> motion_connection_port_;
    std::shared_ptr<IMotionStatePort> motion_state_port_;

    Result<void> ValidateRequest(const HomeAxesRequest& request);
    Result<void> CheckSystemState();
    Result<void> ValidateHomingConfiguration(const std::vector<LogicalAxisId>& axes, int axis_count) const;
    Result<void> ValidateSafetyPreconditions(const std::vector<LogicalAxisId>& axes) const;
    Result<void> HomeAxis(LogicalAxisId axis_id);
    Result<void> WaitForHomingComplete(LogicalAxisId axis_id, int32 timeout_ms);
    Result<void> WaitForAxisSettleAfterHoming(
        LogicalAxisId axis_id,
        const std::chrono::steady_clock::time_point& deadline) const;
    Result<int32> GetHomingSettleTimeMs(LogicalAxisId axis_id) const;
    Result<void> VerifyHomingSuccess(LogicalAxisId axis_id);
    Result<int> GetConfiguredAxisCount() const;
};

}  // namespace Siligen::RuntimeExecution::Application::Services::Motion
