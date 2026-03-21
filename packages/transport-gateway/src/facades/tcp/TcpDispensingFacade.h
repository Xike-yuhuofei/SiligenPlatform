#pragma once

#include "application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "application/usecases/dispensing/PlanningUseCase.h"
#include "application/usecases/dispensing/UploadFileUseCase.h"
#include "application/usecases/dispensing/valve/ValveCommandUseCase.h"
#include "application/usecases/dispensing/valve/ValveQueryUseCase.h"

#include <memory>

namespace Siligen::Application::Facades::Tcp {

class TcpDispensingFacade {
   public:
    TcpDispensingFacade(std::shared_ptr<UseCases::Dispensing::Valve::ValveCommandUseCase> valve_command_use_case,
                        std::shared_ptr<UseCases::Dispensing::Valve::ValveQueryUseCase> valve_query_use_case,
                        std::shared_ptr<UseCases::Dispensing::DispensingExecutionUseCase> dxf_execute_use_case,
                        std::shared_ptr<UseCases::Dispensing::UploadFileUseCase> dxf_upload_use_case,
                        std::shared_ptr<UseCases::Dispensing::PlanningUseCase> dxf_planning_use_case);

    Shared::Types::Result<Domain::Dispensing::Ports::DispenserValveState> StartDispenser(
        const Domain::Dispensing::Ports::DispenserValveParams& params);
    Shared::Types::Result<UseCases::Dispensing::Valve::PurgeDispenserResponse> PurgeDispenser(
        const UseCases::Dispensing::Valve::PurgeDispenserRequest& request);
    Shared::Types::Result<void> StopDispenser();
    Shared::Types::Result<void> PauseDispenser();
    Shared::Types::Result<void> ResumeDispenser();
    Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveState> OpenSupplyValve();
    Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveState> CloseSupplyValve();
    Shared::Types::Result<Domain::Dispensing::Ports::DispenserValveState> GetDispenserStatus();
    Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveStatusDetail> GetSupplyStatus();

    Shared::Types::Result<UseCases::Dispensing::UploadResponse> UploadDxf(
        const UseCases::Dispensing::UploadRequest& request);
    Shared::Types::Result<UseCases::Dispensing::PlanningResponse> PlanDxf(
        const UseCases::Dispensing::PlanningRequest& request);
    Shared::Types::Result<UseCases::Dispensing::TaskID> ExecuteDxfAsync(
        const UseCases::Dispensing::DispensingMVPRequest& request);
    Shared::Types::Result<UseCases::Dispensing::TaskStatusResponse> GetDxfTaskStatus(
        const UseCases::Dispensing::TaskID& task_id) const;
    Shared::Types::Result<void> PauseDxfTask(const UseCases::Dispensing::TaskID& task_id);
    Shared::Types::Result<void> ResumeDxfTask(const UseCases::Dispensing::TaskID& task_id);
    Shared::Types::Result<void> CancelDxfTask(const UseCases::Dispensing::TaskID& task_id);
    void StopDxfExecution();

   private:
    std::shared_ptr<UseCases::Dispensing::Valve::ValveCommandUseCase> valve_command_use_case_;
    std::shared_ptr<UseCases::Dispensing::Valve::ValveQueryUseCase> valve_query_use_case_;
    std::shared_ptr<UseCases::Dispensing::DispensingExecutionUseCase> dxf_execute_use_case_;
    std::shared_ptr<UseCases::Dispensing::UploadFileUseCase> dxf_upload_use_case_;
    std::shared_ptr<UseCases::Dispensing::PlanningUseCase> dxf_planning_use_case_;
};

}  // namespace Siligen::Application::Facades::Tcp


