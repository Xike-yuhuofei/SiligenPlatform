#pragma once

#include "dispense_packaging/application/usecases/dispensing/valve/ValveCommandUseCase.h"
#include "dispense_packaging/application/usecases/dispensing/valve/ValveQueryUseCase.h"
#include "job_ingest/contracts/dispensing/UploadContracts.h"
#include "domain/safety/value-objects/InterlockTypes.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "workflow/application/phase-control/DispensingWorkflowUseCase.h"
#include "workflow/application/planning-trigger/PlanningUseCase.h"

#include <memory>

namespace Siligen::Application::Facades::Tcp {

class TcpDispensingFacade {
   public:
    TcpDispensingFacade(std::shared_ptr<UseCases::Dispensing::Valve::ValveCommandUseCase> valve_command_use_case,
                        std::shared_ptr<UseCases::Dispensing::Valve::ValveQueryUseCase> valve_query_use_case,
                        std::shared_ptr<UseCases::Dispensing::DispensingExecutionUseCase> dxf_execute_use_case,
                        std::shared_ptr<UseCases::Dispensing::IUploadFilePort> dxf_upload_use_case,
                        std::shared_ptr<UseCases::Dispensing::PlanningUseCase> dxf_planning_use_case,
                        std::shared_ptr<UseCases::Dispensing::DispensingWorkflowUseCase> dxf_workflow_use_case);

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
    Shared::Types::Result<UseCases::Dispensing::CreateArtifactResponse> CreateDxfArtifact(
        const UseCases::Dispensing::UploadRequest& request);
    Shared::Types::Result<UseCases::Dispensing::PreparePlanResponse> PrepareDxfPlan(
        const UseCases::Dispensing::PreparePlanRequest& request);
    Shared::Types::Result<UseCases::Dispensing::PreviewSnapshotResponse> GetDxfPreviewSnapshot(
        const UseCases::Dispensing::PreviewSnapshotRequest& request);
    Shared::Types::Result<UseCases::Dispensing::ConfirmPreviewResponse> ConfirmDxfPreview(
        const UseCases::Dispensing::ConfirmPreviewRequest& request);
    Shared::Types::Result<UseCases::Dispensing::StartJobResponse> StartDxfJob(
        const UseCases::Dispensing::StartJobRequest& request);
    Shared::Types::Result<UseCases::Dispensing::JobStatusResponse> GetDxfJobStatus(
        const UseCases::Dispensing::JobID& job_id) const;
    Shared::Types::Result<void> PauseDxfJob(const UseCases::Dispensing::JobID& job_id);
    Shared::Types::Result<void> ResumeDxfJob(const UseCases::Dispensing::JobID& job_id);
    Shared::Types::Result<void> StopDxfJob(const UseCases::Dispensing::JobID& job_id);
    Shared::Types::Result<Domain::Safety::ValueObjects::InterlockSignals> ReadInterlockSignals() const;
    bool IsInterlockLatched() const;

   private:
    std::shared_ptr<UseCases::Dispensing::Valve::ValveCommandUseCase> valve_command_use_case_;
    std::shared_ptr<UseCases::Dispensing::Valve::ValveQueryUseCase> valve_query_use_case_;
    std::shared_ptr<UseCases::Dispensing::DispensingExecutionUseCase> dxf_execute_use_case_;
    std::shared_ptr<UseCases::Dispensing::IUploadFilePort> dxf_upload_use_case_;
    std::shared_ptr<UseCases::Dispensing::PlanningUseCase> dxf_planning_use_case_;
    std::shared_ptr<UseCases::Dispensing::DispensingWorkflowUseCase> dxf_workflow_use_case_;
};

}  // namespace Siligen::Application::Facades::Tcp


