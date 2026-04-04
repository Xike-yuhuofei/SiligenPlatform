#include "ApplicationContainer.h"

#include "application/usecases/dispensing/CleanupFilesUseCase.h"
#include "application/usecases/dispensing/valve/ValveCommandUseCase.h"
#include "application/usecases/dispensing/valve/ValveQueryUseCase.h"
#include "domain/dispensing/domain-services/ValveCoordinationService.h"
#include "domain/safety/ports/IInterlockSignalPort.h"
#include "process_path/contracts/IPathSourcePort.h"
#include "application/usecases/dispensing/UploadFileUseCase.h"
#include "job_ingest/contracts/dispensing/UploadContracts.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "runtime/dispensing/WorkflowDispensingProcessPortAdapter.h"
#include "runtime/planning/PlanningArtifactExportPortAdapter.h"
#include "runtime/storage/files/LocalFileStorageAdapter.h"
#include "application/usecases/dispensing/DispensingWorkflowUseCase.h"
#include "application/usecases/dispensing/PlanningUseCase.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Result.h"

#include <memory>
#include <stdexcept>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "ApplicationContainer.Dispensing"

namespace Siligen::Application::Container {

namespace {

class CleanupFileStorageAdapter final : public Siligen::JobIngest::Contracts::Storage::IFileStoragePort {
   public:
    explicit CleanupFileStorageAdapter(
        std::shared_ptr<Siligen::Infrastructure::Adapters::LocalFileStorageAdapter> file_storage_port)
        : file_storage_port_(std::move(file_storage_port)) {}

    Siligen::Shared::Types::Result<std::string> StoreFile(
        const Siligen::JobIngest::Contracts::Storage::FileData& file_data,
        const std::string& filename) override {
        return file_storage_port_->StoreFile(
            {file_data.content, file_data.original_name, file_data.size, file_data.content_type},
            filename);
    }

    Siligen::Shared::Types::Result<void> ValidateFile(
        const Siligen::JobIngest::Contracts::Storage::FileData& file_data,
        size_t max_size_mb,
        const std::vector<std::string>& allowed_extensions) override {
        return file_storage_port_->ValidateFile(
            {file_data.content, file_data.original_name, file_data.size, file_data.content_type},
            max_size_mb,
            allowed_extensions);
    }

    Siligen::Shared::Types::Result<void> DeleteFile(const std::string& filepath) override {
        return file_storage_port_->DeleteFile(filepath);
    }

    Siligen::Shared::Types::Result<bool> FileExists(const std::string& filepath) override {
        return file_storage_port_->FileExists(filepath);
    }

    Siligen::Shared::Types::Result<size_t> GetFileSize(const std::string& filepath) override {
        return file_storage_port_->GetFileSize(filepath);
    }

   private:
    std::shared_ptr<Siligen::Infrastructure::Adapters::LocalFileStorageAdapter> file_storage_port_;
};

}  // namespace

void ApplicationContainer::ValidateDispensingPorts() {
    if (!trigger_port_) {
        throw std::runtime_error("ITriggerControllerPort 未注册");
    }
    if (!valve_port_) {
        throw std::runtime_error("IValvePort 未注册");
    }
    if (!file_storage_port_) {
        throw std::runtime_error("IFileStoragePort 未注册");
    }
    if (!task_scheduler_port_) {
        SILIGEN_LOG_WARNING("ITaskSchedulerPort 未注册");
    }
}

void ApplicationContainer::ConfigureDispensingServices() {
    valve_controller_ =
        std::make_shared<Domain::Dispensing::DomainServices::ValveCoordinationService>(valve_port_);
    RegisterService<Domain::Dispensing::DomainServices::ValveCoordinationService>(valve_controller_);
    SILIGEN_LOG_INFO("ValveController registered");
}

template<>
std::shared_ptr<UseCases::Dispensing::Valve::ValveCommandUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::Valve::ValveCommandUseCase>() {
    return std::make_shared<UseCases::Dispensing::Valve::ValveCommandUseCase>(
        valve_controller_,
        valve_port_,
        config_port_,
        device_connection_port_);
}

template<>
std::shared_ptr<UseCases::Dispensing::Valve::ValveQueryUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::Valve::ValveQueryUseCase>() {
    return std::make_shared<UseCases::Dispensing::Valve::ValveQueryUseCase>(valve_port_);
}

template<>
std::shared_ptr<UseCases::Dispensing::PlanningUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::PlanningUseCase>() {
    auto path_source = ResolvePort<Domain::Trajectory::Ports::IPathSourcePort>();
    if (!path_source) {
        throw std::runtime_error("IPathSourcePort 未注册");
    }

    return std::make_shared<UseCases::Dispensing::PlanningUseCase>(
        path_source,
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningPathPreparationPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningMotionPlanPort(
            velocity_profile_service_),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningAssemblyPort(),
        config_port_,
        nullptr,
        Siligen::RuntimeExecution::Host::Planning::CreatePlanningArtifactExportPort(),
        diagnostics_port_,
        event_port_);
}

template<>
std::shared_ptr<UseCases::Dispensing::UploadFileUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::UploadFileUseCase>() {
    return std::make_shared<UseCases::Dispensing::UploadFileUseCase>(
        file_storage_port_,
        10,
        config_port_);
}

template<>
std::shared_ptr<UseCases::Dispensing::IUploadFilePort>
ApplicationContainer::CreateInstance<UseCases::Dispensing::IUploadFilePort>() {
    return std::static_pointer_cast<UseCases::Dispensing::IUploadFilePort>(
        Resolve<UseCases::Dispensing::UploadFileUseCase>());
}

template<>
std::shared_ptr<UseCases::Dispensing::CleanupFilesUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::CleanupFilesUseCase>() {
    auto local_file_storage =
        std::dynamic_pointer_cast<Siligen::Infrastructure::Adapters::LocalFileStorageAdapter>(file_storage_port_);
    if (!local_file_storage) {
        throw std::runtime_error("LocalFileStorageAdapter 未注册");
    }
    return std::make_shared<UseCases::Dispensing::CleanupFilesUseCase>(
        std::make_shared<CleanupFileStorageAdapter>(std::move(local_file_storage)),
        upload_base_dir_);
}

template<>
std::shared_ptr<UseCases::Dispensing::DispensingExecutionUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::DispensingExecutionUseCase>() {
    auto process_port = std::make_shared<Siligen::Runtime::Service::Dispensing::WorkflowDispensingProcessPortAdapter>(
        valve_port_,
        interpolation_port_,
        motion_state_port_,
        device_connection_port_,
        config_port_);
    return std::make_shared<UseCases::Dispensing::DispensingExecutionUseCase>(
        valve_port_,
        interpolation_port_,
        motion_state_port_,
        device_connection_port_,
        config_port_,
        process_port,
        event_port_,
        task_scheduler_port_,
        homing_port_,
        ResolvePort<Domain::Safety::Ports::IInterlockSignalPort>());
}

template<>
std::shared_ptr<UseCases::Dispensing::DispensingWorkflowUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::DispensingWorkflowUseCase>() {
    return std::make_shared<UseCases::Dispensing::DispensingWorkflowUseCase>(
        Resolve<UseCases::Dispensing::IUploadFilePort>(),
        Resolve<UseCases::Dispensing::PlanningUseCase>(),
        Resolve<UseCases::Dispensing::DispensingExecutionUseCase>(),
        device_connection_port_,
        motion_state_port_,
        homing_port_,
        ResolvePort<Domain::Safety::Ports::IInterlockSignalPort>());
}

}  // namespace Siligen::Application::Container


