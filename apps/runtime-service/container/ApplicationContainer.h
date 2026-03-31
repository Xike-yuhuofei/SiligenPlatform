#pragma once

#include "ApplicationContainerFwd.h"
#include "../runtime/configuration/WorkspaceAssetPaths.h"
#include "domain/configuration/ports/IConfigurationPort.h"
#include "domain/motion/ports/IMotionRuntimePort.h"
#include "siligen/device/contracts/ports/device_ports.h"
#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>

namespace Siligen::Application::Container {

/**
 * @brief 应用容器 - 依赖注入容器
 *
 * 负责创建和管理所有依赖关系:
 * - 端口接口 -> 适配器实现的绑定
 * - 用例的依赖注入
 * - 单例生命周期管理
 */
class ApplicationContainer {
public:
    using InstanceMap = std::unordered_map<std::type_index, std::shared_ptr<void>>;

    /**
     * @brief 构造函数
     * @param config_file_path 配置文件路径
     * @param log_mode 日志输出模式
     * @param log_file_path 日志文件路径(仅在log_mode=File时有效)
     */
    explicit ApplicationContainer(
        const std::string& config_file_path = Infrastructure::Configuration::kCanonicalMachineConfigRelativePath,
        LogMode log_mode = LogMode::Console,
        const std::string& log_file_path = "system.log"
    );

    ~ApplicationContainer();

    // 禁止拷贝和移动
    ApplicationContainer(const ApplicationContainer&) = delete;
    ApplicationContainer& operator=(const ApplicationContainer&) = delete;
    ApplicationContainer(ApplicationContainer&&) = delete;
    ApplicationContainer& operator=(ApplicationContainer&&) = delete;

    /**
     * @brief 配置所有依赖
     * 必须在使用容器之前调用
     */
    void Configure();

    /**
     * @brief 恢复stdout输出（用于Silent模式下在配置完成后显示用户界面）
     */
    void RestoreStdout();

    /**
     * @brief 静默stdout输出（用于Silent模式下在析构前隐藏日志）
     */
    void SilenceStdout();

    /**
     * @brief 解析用例实例
     */
    template<typename T>
    std::shared_ptr<T> Resolve() {
        if (auto singleton = FindInRegistry<T>(singleton_instances_)) {
            return singleton;
        }

        if (auto use_case = FindInRegistry<T>(use_case_instances_)) {
            return use_case;
        }

        auto instance = CreateInstance<T>();
        if (instance) {
            use_case_instances_[std::type_index(typeid(T))] = instance;
        }
        return instance;
    }

    /**
     * @brief 解析端口实例
     */
    template<typename TPort>
    std::shared_ptr<TPort> ResolvePort() {
        return FindInRegistry<TPort>(port_instances_);
    }

    std::shared_ptr<Domain::Motion::Ports::IMotionRuntimePort> ResolveMotionRuntimePort() const;

    /**
     * @brief 解析Domain Service实例
     *
     * 注意：Domain Service与Port的区别：
     * - Port是Domain层定义的接口，由Infrastructure层实现
     * - Service是Domain层的业务逻辑封装，依赖Port接口
     *
     */
    template<typename TService>
    std::shared_ptr<TService> ResolveService() {
        return FindInRegistry<TService>(service_instances_);
    }

    /**
     * @brief 注册端口实现
     */
    template<typename TPort, typename TAdapter>
    void RegisterPort(std::shared_ptr<TAdapter> adapter) {
        auto typed_port = std::static_pointer_cast<TPort>(std::move(adapter));
        port_instances_[std::type_index(typeid(TPort))] = typed_port;
        CacheKnownPort<TPort>(typed_port);
    }

    /**
     * @brief 注册单例
     */
    template<typename T>
    void RegisterSingleton(std::shared_ptr<T> instance) {
        singleton_instances_[std::type_index(typeid(T))] = std::move(instance);
    }

    /**
     * @brief 获取MultiCard共享实例
     * @return MultiCard实例指针（不暴露类型）
     *
     * 注意：此方法返回void*以避免暴露infrastructure层类型
     * 调用者应将其转换为正确的MultiCard类型使用
     */
    void* GetMultiCardInstance();

    /**
     * @brief 设置MultiCard共享实例（由组合根注入）
     */
    void SetMultiCardInstance(std::shared_ptr<void> multicard_instance);

    /**
     * @brief 设置上传目录（由组合根注入）
     */
    void SetUploadBaseDir(const std::string& upload_base_dir);

    /**
     * @brief 获取日志服务接口
     * @return ILoggingService共享指针
     *
     * Phase 3: 六边形架构日志系统集成
     * 返回统一的日志服务接口，供各层使用
     */
    std::shared_ptr<Shared::Interfaces::ILoggingService> GetLoggingService();

    /**
     * @brief 设置日志服务（由组合根注入）
     */
    void SetLoggingService(std::shared_ptr<Shared::Interfaces::ILoggingService> logging_service);

private:
    std::string config_file_path_;

    // 分类后的实例存储
    InstanceMap singleton_instances_;
    InstanceMap use_case_instances_;
    InstanceMap port_instances_;
    InstanceMap service_instances_;

    // 端口实例(强类型)
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port_;
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> device_connection_port_;
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort> machine_execution_state_port_;
    std::shared_ptr<Domain::Motion::Ports::IMotionRuntimePort> motion_runtime_port_;
    std::shared_ptr<Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port_;
    std::shared_ptr<Domain::Motion::Ports::IAxisControlPort> axis_control_port_;
    std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port_;
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port_;
    std::shared_ptr<Domain::Motion::Ports::IJogControlPort> motion_jog_port_;
    std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port_;
    std::shared_ptr<Domain::Dispensing::Ports::ITriggerControllerPort> trigger_port_;
    std::shared_ptr<Domain::Diagnostics::Ports::IDiagnosticsPort> diagnostics_port_;
    std::shared_ptr<Domain::System::Ports::IEventPublisherPort> event_port_;
    std::shared_ptr<Domain::Diagnostics::Ports::ICMPTestPresetPort> preset_port_;

    // State Machine (P0 Refactoring)

    std::shared_ptr<Domain::Diagnostics::Ports::ITestRecordRepository> test_record_repository_;
    std::shared_ptr<Domain::Diagnostics::Ports::ITestConfigurationPort> test_config_manager_;

    // Domain层端口（供适配器层使用）
    std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port_;
    std::shared_ptr<Domain::Configuration::Ports::IFileStoragePort> file_storage_port_;
    std::shared_ptr<Domain::Motion::Ports::IIOControlPort> io_control_port_;
    std::shared_ptr<Domain::Motion::Ports::IInterpolationPort> interpolation_port_;
    std::shared_ptr<Domain::Motion::Ports::IVelocityProfilePort> velocity_profile_port_;
    std::shared_ptr<Domain::Dispensing::Ports::ITaskSchedulerPort> task_scheduler_port_;  // Phase 2: 任务调度器
    std::shared_ptr<Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository_;
    std::shared_ptr<Domain::Recipes::Ports::ITemplateRepositoryPort> template_repository_;
    std::shared_ptr<Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository_;
    std::shared_ptr<Domain::Recipes::Ports::IParameterSchemaPort> parameter_schema_port_;
    std::shared_ptr<Domain::Recipes::Ports::IRecipeBundleSerializerPort> recipe_bundle_serializer_port_;

    // Domain Service实例
    std::shared_ptr<Domain::Motion::DomainServices::JogController> jog_controller_;
    std::shared_ptr<Domain::Motion::DomainServices::VelocityProfileService> velocity_profile_service_;
    std::shared_ptr<Domain::Dispensing::DomainServices::ValveCoordinationService> valve_controller_;
    std::shared_ptr<Siligen::Application::UseCases::System::IHardLimitMonitor> hard_limit_monitor_;

    // 配置数据
    Domain::Configuration::Ports::ValveSupplyConfig valve_supply_config_;  ///< 供胶阀配置
    Shared::Types::DispenserValveConfig dispenser_config_;     ///< 点胶阀配置
    std::array<Domain::Configuration::Ports::HomingConfig, 4> homing_configs_;     ///< 回零配置（仅使用前2轴）

    // MultiCard共享实例（内部管理，不暴露给外部）
    std::shared_ptr<void> multiCard_;  // 使用void*避免类型暴露

    // 上传目录绝对路径（用于 LocalFileStorageAdapter 和 CleanupFilesUseCase）
    std::string upload_base_dir_;

    // 日志重定向相关
    LogMode log_mode_ = LogMode::Console;
    std::string log_file_path_;
    int saved_stdout_fd_ = -1;
    int saved_stderr_fd_ = -1;
    FILE* log_file_ = nullptr;

    // Phase 3: 六边形架构日志系统
    // 使用接口类型存储，避免暴露具体日志适配实现细节
    std::shared_ptr<Shared::Interfaces::ILoggingService> logging_service_;

    /**
     * @brief 配置端口绑定
     */
    void ConfigurePorts();

    /**
     * @brief 配置用例
     */
    void ConfigureUseCases();

    /**
     * @brief 配置Domain Service
     * 在ConfigurePorts()之后调用，因为Service依赖Port
     */
    void ConfigureServices();

    void ValidateSystemPorts();
    void ValidateDiagnosticsPorts();
    void ValidateMotionPorts();
    void ValidateDispensingPorts();
    void ValidateRecipePorts();

    void ConfigureMotionServices();
    void ConfigureDispensingServices();
    void ConfigureRecipeServices();

    template<typename T>
    std::shared_ptr<T> FindInRegistry(const InstanceMap& registry) const {
        auto it = registry.find(std::type_index(typeid(T)));
        if (it == registry.end()) {
            return nullptr;
        }
        return std::static_pointer_cast<T>(it->second);
    }

    template<typename TPort>
    void CacheKnownPort(const std::shared_ptr<TPort>& port) {
        if constexpr (std::is_same_v<TPort, Domain::Configuration::Ports::IConfigurationPort>) {
            config_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Siligen::Device::Contracts::Ports::DeviceConnectionPort>) {
            device_connection_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort>) {
            machine_execution_state_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Motion::Ports::IMotionRuntimePort>) {
            motion_runtime_port_ = port;
            CacheMotionRuntimeAliases(port);
        } else if constexpr (std::is_same_v<TPort, Domain::Motion::Ports::IMotionConnectionPort>) {
            motion_connection_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Motion::Ports::IAxisControlPort>) {
            axis_control_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Motion::Ports::IPositionControlPort>) {
            position_control_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Motion::Ports::IMotionStatePort>) {
            motion_state_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Motion::Ports::IJogControlPort>) {
            motion_jog_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Motion::Ports::IIOControlPort>) {
            io_control_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Motion::Ports::IHomingPort>) {
            homing_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Dispensing::Ports::ITriggerControllerPort>) {
            trigger_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Diagnostics::Ports::IDiagnosticsPort>) {
            diagnostics_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::System::Ports::IEventPublisherPort>) {
            event_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Diagnostics::Ports::ICMPTestPresetPort>) {
            preset_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Diagnostics::Ports::ITestRecordRepository>) {
            test_record_repository_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Diagnostics::Ports::ITestConfigurationPort>) {
            test_config_manager_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Dispensing::Ports::IValvePort>) {
            valve_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Configuration::Ports::IFileStoragePort>) {
            file_storage_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Motion::Ports::IInterpolationPort>) {
            interpolation_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Motion::Ports::IVelocityProfilePort>) {
            velocity_profile_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Dispensing::Ports::ITaskSchedulerPort>) {
            task_scheduler_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Recipes::Ports::IRecipeRepositoryPort>) {
            recipe_repository_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Recipes::Ports::ITemplateRepositoryPort>) {
            template_repository_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Recipes::Ports::IAuditRepositoryPort>) {
            audit_repository_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Recipes::Ports::IParameterSchemaPort>) {
            parameter_schema_port_ = port;
        } else if constexpr (std::is_same_v<TPort, Domain::Recipes::Ports::IRecipeBundleSerializerPort>) {
            recipe_bundle_serializer_port_ = port;
        }
    }

    void CacheMotionRuntimeAliases(const std::shared_ptr<Domain::Motion::Ports::IMotionRuntimePort>& port) {
        if (!motion_connection_port_) {
            motion_connection_port_ = std::static_pointer_cast<Domain::Motion::Ports::IMotionConnectionPort>(port);
        }
        if (!port_instances_.count(std::type_index(typeid(Domain::Motion::Ports::IMotionConnectionPort)))) {
            port_instances_[std::type_index(typeid(Domain::Motion::Ports::IMotionConnectionPort))] =
                std::static_pointer_cast<Domain::Motion::Ports::IMotionConnectionPort>(port);
        }
        if (!axis_control_port_) {
            axis_control_port_ = std::static_pointer_cast<Domain::Motion::Ports::IAxisControlPort>(port);
        }
        if (!port_instances_.count(std::type_index(typeid(Domain::Motion::Ports::IAxisControlPort)))) {
            port_instances_[std::type_index(typeid(Domain::Motion::Ports::IAxisControlPort))] =
                std::static_pointer_cast<Domain::Motion::Ports::IAxisControlPort>(port);
        }
        if (!position_control_port_) {
            position_control_port_ = std::static_pointer_cast<Domain::Motion::Ports::IPositionControlPort>(port);
        }
        if (!port_instances_.count(std::type_index(typeid(Domain::Motion::Ports::IPositionControlPort)))) {
            port_instances_[std::type_index(typeid(Domain::Motion::Ports::IPositionControlPort))] =
                std::static_pointer_cast<Domain::Motion::Ports::IPositionControlPort>(port);
        }
        if (!motion_state_port_) {
            motion_state_port_ = std::static_pointer_cast<Domain::Motion::Ports::IMotionStatePort>(port);
        }
        if (!port_instances_.count(std::type_index(typeid(Domain::Motion::Ports::IMotionStatePort)))) {
            port_instances_[std::type_index(typeid(Domain::Motion::Ports::IMotionStatePort))] =
                std::static_pointer_cast<Domain::Motion::Ports::IMotionStatePort>(port);
        }
        if (!motion_jog_port_) {
            motion_jog_port_ = std::static_pointer_cast<Domain::Motion::Ports::IJogControlPort>(port);
        }
        if (!port_instances_.count(std::type_index(typeid(Domain::Motion::Ports::IJogControlPort)))) {
            port_instances_[std::type_index(typeid(Domain::Motion::Ports::IJogControlPort))] =
                std::static_pointer_cast<Domain::Motion::Ports::IJogControlPort>(port);
        }
        if (!io_control_port_) {
            io_control_port_ = std::static_pointer_cast<Domain::Motion::Ports::IIOControlPort>(port);
        }
        if (!port_instances_.count(std::type_index(typeid(Domain::Motion::Ports::IIOControlPort)))) {
            port_instances_[std::type_index(typeid(Domain::Motion::Ports::IIOControlPort))] =
                std::static_pointer_cast<Domain::Motion::Ports::IIOControlPort>(port);
        }
        if (!homing_port_) {
            homing_port_ = std::static_pointer_cast<Domain::Motion::Ports::IHomingPort>(port);
        }
        if (!port_instances_.count(std::type_index(typeid(Domain::Motion::Ports::IHomingPort)))) {
            port_instances_[std::type_index(typeid(Domain::Motion::Ports::IHomingPort))] =
                std::static_pointer_cast<Domain::Motion::Ports::IHomingPort>(port);
        }
    }

    template<typename TService, typename TImplementation = TService>
    void RegisterService(std::shared_ptr<TImplementation> service) {
        auto typed_service = std::static_pointer_cast<TService>(std::move(service));
        service_instances_[std::type_index(typeid(TService))] = typed_service;
        CacheKnownService<TService>(typed_service);
    }

    template<typename TService>
    void CacheKnownService(const std::shared_ptr<TService>& service) {
        if constexpr (std::is_same_v<TService, Domain::Motion::DomainServices::JogController>) {
            jog_controller_ = service;
        } else if constexpr (std::is_same_v<TService, Domain::Motion::DomainServices::VelocityProfileService>) {
            velocity_profile_service_ = service;
        } else if constexpr (std::is_same_v<TService, Domain::Dispensing::DomainServices::ValveCoordinationService>) {
            valve_controller_ = service;
        }
    }

    /**
     * @brief 创建实例(模板特化)
     */
    template<typename T>
    std::shared_ptr<T> CreateInstance() {
        return nullptr;
    }
};

// 模板特化声明（定义分布在各 feature cpp 中）
template<>
std::shared_ptr<UseCases::System::InitializeSystemUseCase>
ApplicationContainer::CreateInstance<UseCases::System::InitializeSystemUseCase>();

template<>
std::shared_ptr<UseCases::Motion::Homing::HomeAxesUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Homing::HomeAxesUseCase>();

template<>
std::shared_ptr<UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase>();

template<>
std::shared_ptr<UseCases::Motion::MotionControlUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::MotionControlUseCase>();

template<>
std::shared_ptr<UseCases::Motion::Trajectory::ExecuteTrajectoryUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Trajectory::ExecuteTrajectoryUseCase>();

template<>
std::shared_ptr<UseCases::Dispensing::Valve::ValveCommandUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::Valve::ValveCommandUseCase>();

template<>
std::shared_ptr<UseCases::Dispensing::Valve::ValveQueryUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::Valve::ValveQueryUseCase>();

template<>
std::shared_ptr<UseCases::Motion::Manual::ManualMotionControlUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Manual::ManualMotionControlUseCase>();

template<>
std::shared_ptr<UseCases::Motion::Initialization::MotionInitializationUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Initialization::MotionInitializationUseCase>();

template<>
std::shared_ptr<UseCases::Motion::Safety::MotionSafetyUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Safety::MotionSafetyUseCase>();

template<>
std::shared_ptr<UseCases::Motion::Monitoring::MotionMonitoringUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Monitoring::MotionMonitoringUseCase>();

template<>
std::shared_ptr<UseCases::Motion::Coordination::MotionCoordinationUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Coordination::MotionCoordinationUseCase>();

template<>
std::shared_ptr<UseCases::Motion::Interpolation::InterpolationPlanningUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Interpolation::InterpolationPlanningUseCase>();

template<>
std::shared_ptr<UseCases::Dispensing::PlanningUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::PlanningUseCase>();

template<>
std::shared_ptr<UseCases::Dispensing::UploadFileUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::UploadFileUseCase>();

template<>
std::shared_ptr<UseCases::Dispensing::IUploadFilePort>
ApplicationContainer::CreateInstance<UseCases::Dispensing::IUploadFilePort>();

// DXF Cleanup and Execution UseCase 特化声明
template<>
std::shared_ptr<UseCases::Dispensing::CleanupFilesUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::CleanupFilesUseCase>();

template<>
std::shared_ptr<UseCases::Dispensing::DispensingExecutionUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::DispensingExecutionUseCase>();

template<>
std::shared_ptr<UseCases::Dispensing::DispensingWorkflowUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::DispensingWorkflowUseCase>();

template<>
std::shared_ptr<UseCases::System::EmergencyStopUseCase>
ApplicationContainer::CreateInstance<UseCases::System::EmergencyStopUseCase>();

// Recipe UseCase 特化声明
template<>
std::shared_ptr<UseCases::Recipes::CreateRecipeUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::CreateRecipeUseCase>();

template<>
std::shared_ptr<UseCases::Recipes::UpdateRecipeUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::UpdateRecipeUseCase>();

template<>
std::shared_ptr<UseCases::Recipes::CreateDraftVersionUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::CreateDraftVersionUseCase>();

template<>
std::shared_ptr<UseCases::Recipes::UpdateDraftVersionUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::UpdateDraftVersionUseCase>();

template<>
std::shared_ptr<UseCases::Recipes::RecipeCommandUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::RecipeCommandUseCase>();

template<>
std::shared_ptr<UseCases::Recipes::RecipeQueryUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::RecipeQueryUseCase>();

template<>
std::shared_ptr<UseCases::Recipes::CreateVersionFromPublishedUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::CreateVersionFromPublishedUseCase>();

template<>
std::shared_ptr<UseCases::Recipes::CompareRecipeVersionsUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::CompareRecipeVersionsUseCase>();

template<>
std::shared_ptr<UseCases::Recipes::ExportRecipeBundlePayloadUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::ExportRecipeBundlePayloadUseCase>();

template<>
std::shared_ptr<UseCases::Recipes::ImportRecipeBundlePayloadUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::ImportRecipeBundlePayloadUseCase>();

template<>
std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort>
ApplicationContainer::CreateInstance<Domain::Configuration::Ports::IConfigurationPort>();

} // namespace Siligen::Application::Container



