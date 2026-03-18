#include "TcpSystemFacade.h"

namespace Siligen::Application::Facades::Tcp {

TcpSystemFacade::TcpSystemFacade(
    std::shared_ptr<UseCases::System::InitializeSystemUseCase> initialize_use_case,
    std::shared_ptr<UseCases::System::EmergencyStopUseCase> emergency_stop_use_case)
    : initialize_use_case_(std::move(initialize_use_case)),
      emergency_stop_use_case_(std::move(emergency_stop_use_case)) {}

Shared::Types::Result<UseCases::System::InitializeSystemResponse> TcpSystemFacade::Initialize(
    const UseCases::System::InitializeSystemRequest& request) {
    if (!initialize_use_case_) {
        return Shared::Types::Result<UseCases::System::InitializeSystemResponse>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "InitializeSystemUseCase not available"));
    }
    return initialize_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::System::EmergencyStopResponse> TcpSystemFacade::EmergencyStop(
    const UseCases::System::EmergencyStopRequest& request) {
    if (!emergency_stop_use_case_) {
        return Shared::Types::Result<UseCases::System::EmergencyStopResponse>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "EmergencyStopUseCase not available"));
    }
    return emergency_stop_use_case_->Execute(request);
}

}  // namespace Siligen::Application::Facades::Tcp
