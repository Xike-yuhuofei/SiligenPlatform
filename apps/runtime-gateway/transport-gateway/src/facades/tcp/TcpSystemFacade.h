#pragma once

#include "runtime_execution/application/usecases/system/EmergencyStopUseCase.h"
#include "runtime_execution/application/usecases/system/InitializeSystemUseCase.h"

#include <memory>

namespace Siligen::Application::Facades::Tcp {

class TcpSystemFacade {
   public:
    TcpSystemFacade(std::shared_ptr<UseCases::System::InitializeSystemUseCase> initialize_use_case,
                    std::shared_ptr<UseCases::System::EmergencyStopUseCase> emergency_stop_use_case);

    Shared::Types::Result<UseCases::System::InitializeSystemResponse> Initialize(
        const UseCases::System::InitializeSystemRequest& request);
    Shared::Types::Result<UseCases::System::EmergencyStopResponse> EmergencyStop(
        const UseCases::System::EmergencyStopRequest& request);
    Shared::Types::Result<bool> IsInEmergencyStop() const;
    Shared::Types::Result<void> RecoverFromEmergencyStop();

   private:
    std::shared_ptr<UseCases::System::InitializeSystemUseCase> initialize_use_case_;
    std::shared_ptr<UseCases::System::EmergencyStopUseCase> emergency_stop_use_case_;
};

}  // namespace Siligen::Application::Facades::Tcp
