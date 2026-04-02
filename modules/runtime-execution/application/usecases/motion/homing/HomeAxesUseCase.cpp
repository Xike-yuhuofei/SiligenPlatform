#include "runtime_execution/application/usecases/motion/homing/HomeAxesUseCase.h"

#include "runtime_execution/application/services/motion/HomingProcess.h"
#include "shared/types/Error.h"

#include <chrono>
#include <utility>

namespace Siligen::Application::UseCases::Motion::Homing {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

namespace {

constexpr const char* kErrorSource = "HomeAxesUseCase";

int64 CurrentTimestampMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

HomeAxesUseCase::HomeAxesUseCase(std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port,
                                 std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
                                 std::shared_ptr<Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port,
                                 std::shared_ptr<Domain::System::Ports::IEventPublisherPort> event_port,
                                 std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port)
    : homing_process_(std::make_shared<Siligen::RuntimeExecution::Application::Services::Motion::HomingProcess>(
          std::move(homing_port),
          std::move(config_port),
          std::move(motion_connection_port),
          std::move(motion_state_port))),
      event_port_(std::move(event_port)) {}

Result<HomeAxesResponse> HomeAxesUseCase::Execute(const HomeAxesRequest& request) {
    if (!homing_process_) {
        return Result<HomeAxesResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Homing process not initialized", kErrorSource));
    }

    auto result = homing_process_->Execute(request);
    if (result.IsError()) {
        return Result<HomeAxesResponse>::Failure(result.GetError());
    }

    const auto& response = result.Value();
    for (const auto axis_id : response.successfully_homed_axes) {
        PublishHomingEvent(axis_id, true);
    }
    for (const auto axis_id : response.failed_axes) {
        PublishHomingEvent(axis_id, false);
    }

    return Result<HomeAxesResponse>::Success(response);
}

Result<void> HomeAxesUseCase::StopHoming() {
    if (!homing_process_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Homing process not initialized", kErrorSource));
    }

    return homing_process_->StopHoming();
}

Result<void> HomeAxesUseCase::StopHomingAxes(const std::vector<LogicalAxisId>& axes) {
    if (!homing_process_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Homing process not initialized", kErrorSource));
    }

    return homing_process_->StopHomingAxes(axes);
}

Result<bool> HomeAxesUseCase::IsAxisHomed(LogicalAxisId axis_id) const {
    if (!homing_process_) {
        return Result<bool>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Homing process not initialized", kErrorSource));
    }

    return homing_process_->IsAxisHomed(axis_id);
}

void HomeAxesUseCase::PublishHomingEvent(LogicalAxisId axis_id, bool success) {
    if (!event_port_) {
        return;
    }

    Domain::System::Ports::DomainEvent event;
    event.type = Domain::System::Ports::EventType::HOMING_COMPLETED;
    event.timestamp = CurrentTimestampMs();
    event.source = "HomeAxesUseCase";
    event.message = std::string(Siligen::Shared::Types::AxisName(axis_id)) +
                    (success ? ": Homing completed" : ": Homing failed");

    event_port_->PublishAsync(event);
}

}  // namespace Siligen::Application::UseCases::Motion::Homing
