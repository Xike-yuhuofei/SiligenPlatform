#include "ApplicationContainer.h"

#include "runtime_execution/application/usecases/system/IHardLimitMonitor.h"
#include "shared/di/LoggingServiceLocator.h"
#include "shared/interfaces/ILoggingService.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "ApplicationContainer"

namespace Siligen::Application::Container {

ApplicationContainer::ApplicationContainer(
    const std::string& config_file_path,
    LogMode log_mode,
    const std::string& log_file_path)
    : config_file_path_(config_file_path),
      log_mode_(log_mode),
      log_file_path_(log_file_path) {}

ApplicationContainer::~ApplicationContainer() {
    if (hard_limit_monitor_) {
        hard_limit_monitor_->Stop();
        hard_limit_monitor_.reset();
    }

    use_case_instances_.clear();
    singleton_instances_.clear();
    service_instances_.clear();
    port_instances_.clear();

    velocity_profile_service_.reset();
    jog_controller_.reset();

    task_scheduler_port_.reset();
    velocity_profile_port_.reset();
    interpolation_port_.reset();
    io_control_port_.reset();
    file_storage_port_.reset();
    dispenser_device_port_.reset();
    valve_port_.reset();
    test_config_manager_.reset();
    test_record_repository_.reset();
    preset_port_.reset();
    event_port_.reset();
    diagnostics_port_.reset();
    trigger_port_.reset();
    homing_port_.reset();
    motion_jog_port_.reset();
    motion_state_port_.reset();
    position_control_port_.reset();
    axis_control_port_.reset();
    motion_connection_port_.reset();
    motion_device_port_.reset();
    motion_runtime_port_.reset();
    config_port_.reset();
    multiCard_.reset();

    if (logging_service_) {
        logging_service_->Flush();
        logging_service_.reset();
    }

    Shared::DI::LoggingServiceLocator::GetInstance().ClearService();
}

void ApplicationContainer::Configure() {
    ConfigurePorts();
    ConfigureServices();
    ConfigureSystemOwnerPorts();
    ConfigureUseCases();
}

void ApplicationContainer::RestoreStdout() {
    // 保留接口以兼容旧调用链。当前日志方案不再重定向 stdout/stderr。
}

void ApplicationContainer::SilenceStdout() {
    // 保留接口以兼容旧调用链。当前日志方案不再重定向 stdout/stderr。
}

void ApplicationContainer::ConfigurePorts() {
    ValidateSystemPorts();
    ValidateDiagnosticsPorts();
    ValidateMotionPorts();
    ValidateDispensingPorts();
}

void ApplicationContainer::ConfigureUseCases() {
    // Use case 保持懒加载，由 Resolve<T>() 首次调用时创建。
}

void ApplicationContainer::ConfigureServices() {
    ConfigureMotionServices();
    ConfigureDispensingServices();
    SILIGEN_LOG_INFO("Domain Services configuration complete");
}

void* ApplicationContainer::GetMultiCardInstance() {
    return multiCard_.get();
}

void ApplicationContainer::SetMultiCardInstance(std::shared_ptr<void> multicard_instance) {
    multiCard_ = std::move(multicard_instance);
}

std::shared_ptr<Shared::Interfaces::ILoggingService> ApplicationContainer::GetLoggingService() {
    return logging_service_;
}

void ApplicationContainer::SetLoggingService(std::shared_ptr<Shared::Interfaces::ILoggingService> logging_service) {
    logging_service_ = std::move(logging_service);
    if (logging_service_) {
        Shared::DI::LoggingServiceLocator::GetInstance().SetService(logging_service_);
    } else {
        Shared::DI::LoggingServiceLocator::GetInstance().ClearService();
    }
}

template<>
std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort>
ApplicationContainer::CreateInstance<Domain::Configuration::Ports::IConfigurationPort>() {
    return config_port_;
}

}  // namespace Siligen::Application::Container
