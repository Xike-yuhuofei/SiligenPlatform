#pragma once

#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "shared/types/ConfigTypes.h"
#include "shared/types/LogTypes.h"
#include "shared/types/DiagnosticsConfig.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace Siligen::Infrastructure::Adapters {

// Using declarations for convenience
using Domain::Configuration::Ports::DispensingConfig;
using Domain::Configuration::Ports::DispensingMode;
using Domain::Configuration::Ports::HomingConfig;
using Domain::Configuration::Ports::MachineConfig;
using Domain::Configuration::Ports::DxfConfig;
using Domain::Configuration::Ports::SystemConfig;
using Shared::Types::Error;
using Shared::Types::ErrorCode;
using Shared::Types::float32;
using Shared::Types::int32;
using Shared::Types::LogConfig;
using Shared::Types::LogLevel;
using Shared::Types::Result;

/**
 * @brief INI配置文件适配器
 * 实现IConfigurationPort接口,提供配置文件的读写功能
 */
class ConfigFileAdapter : public Domain::Configuration::Ports::IConfigurationPort {
   public:
    /**
     * @brief 构造函数
     * @param config_file_path 配置文件路径
     */
    explicit ConfigFileAdapter(const std::string& config_file_path);

    ~ConfigFileAdapter() override = default;

    // 禁止拷贝和移动
    ConfigFileAdapter(const ConfigFileAdapter&) = delete;
    ConfigFileAdapter& operator=(const ConfigFileAdapter&) = delete;
    ConfigFileAdapter(ConfigFileAdapter&&) = delete;
    ConfigFileAdapter& operator=(ConfigFileAdapter&&) = delete;

    // === IConfigurationPort接口实现 ===

    // 配置加载和保存
    Result<Domain::Configuration::Ports::SystemConfig> LoadConfiguration() override;
    Result<void> SaveConfiguration(const Domain::Configuration::Ports::SystemConfig& config) override;
    Result<void> ReloadConfiguration() override;

    // 点胶配置
    Result<Domain::Configuration::Ports::DispensingConfig> GetDispensingConfig() const override;
    Result<void> SetDispensingConfig(const Domain::Configuration::Ports::DispensingConfig& config) override;
    Result<Domain::Configuration::Ports::DxfImportConfig> GetDxfImportConfig() const override;
    Result<Domain::Configuration::Ports::DxfTrajectoryConfig> GetDxfTrajectoryConfig() const override;

    // 机器配置
    Result<Domain::Configuration::Ports::MachineConfig> GetMachineConfig() const override;
    Result<void> SetMachineConfig(const Domain::Configuration::Ports::MachineConfig& config) override;

    // 回零配置
    Result<Domain::Configuration::Ports::HomingConfig> GetHomingConfig(int axis) const override;
    Result<void> SetHomingConfig(int axis, const Domain::Configuration::Ports::HomingConfig& config) override;
    Result<std::vector<Domain::Configuration::Ports::HomingConfig>> GetAllHomingConfigs() const override;
    Result<Domain::Configuration::Ports::ValveSupplyConfig> GetValveSupplyConfig() const override;

    // 配置验证
    Result<bool> ValidateConfiguration() const override;
    Result<std::vector<std::string>> GetValidationErrors() const override;

    // 配置备份和恢复
    Result<void> BackupConfiguration(const std::string& backup_path) override;
    Result<void> RestoreConfiguration(const std::string& backup_path) override;

    // 硬件模式配置
    Result<Shared::Types::HardwareMode> GetHardwareMode() const override;

    // 硬件配置
    Result<Shared::Types::HardwareConfiguration> GetHardwareConfiguration() const override;

    // 日志配置加载 (Log configuration loading)
    Result<Shared::Types::LogConfig> GetLogConfiguration() const;

    // 阀门配置加载 (Valve configuration loading)
    Result<Shared::Types::DispenserValveConfig> GetDispenserValveConfig() const override;
    Result<Shared::Types::ValveCoordinationConfig> GetValveCoordinationConfig() const override;
    Result<Shared::Types::SupplyValveConfig> GetSupplyValveConfig() const;
    Result<Shared::Types::VelocityTraceConfig> GetVelocityTraceConfig() const override;
    Result<Shared::Types::DiagnosticsConfig> GetDiagnosticsConfig() const override;

   private:
    std::string config_file_path_;
    Domain::Configuration::Ports::SystemConfig cached_config_;
    bool config_loaded_;
    mutable bool ini_cache_loaded_ = false;
    mutable std::unordered_map<std::string, std::unordered_map<std::string, std::string>> ini_cache_;

    Result<Domain::Configuration::Ports::SystemConfig> LoadConfigurationCore();

    // INI文件操作辅助方法
    Result<void> LoadIniCache() const;
    Result<std::string> ReadIniValue(const std::string& section, const std::string& key) const;
    bool WriteIniValue(const std::string& section, const std::string& key, const std::string& value) const;

    // 类型转换辅助方法
    Result<float32> ReadFloatValue(const std::string& section, const std::string& key) const;
    Result<int32> ReadIntValue(const std::string& section, const std::string& key) const;
    Result<bool> ReadBoolValue(const std::string& section, const std::string& key) const;
    Result<DispensingMode> ReadDispensingModeValue(const std::string& section, const std::string& key) const;
    Result<Shared::Types::HardwareMode> ReadHardwareModeValue(const std::string& section, const std::string& key) const;
    std::string FloatToString(float32 value) const;
    std::string IntToString(int32 value) const;
    std::string BoolToString(bool value) const;

    // 配置段读取
    Result<void> LoadDispensingSection(Domain::Configuration::Ports::DispensingConfig& config);
    Result<void> LoadMachineSection(Domain::Configuration::Ports::MachineConfig& config);
    Result<void> LoadDxfSection(Domain::Configuration::Ports::DxfConfig& config);
    Result<void> LoadHomingSection(std::vector<Domain::Configuration::Ports::HomingConfig>& configs);

    // 阀门配置段读取 (Valve configuration section loading)
    Result<void> LoadDispenserValveSection(Shared::Types::DispenserValveConfig& config) const;
    Result<void> LoadValveCoordinationSection(Shared::Types::ValveCoordinationConfig& config) const;
    Result<void> LoadSupplyValveSection(Shared::Types::SupplyValveConfig& config) const;

    // 配置段写入
    void SaveDispensingSection(const Domain::Configuration::Ports::DispensingConfig& config) const;
    void SaveMachineSection(const Domain::Configuration::Ports::MachineConfig& config) const;
    void SaveDxfSection(const Domain::Configuration::Ports::DxfConfig& config) const;
    void SaveHomingSection(const std::vector<Domain::Configuration::Ports::HomingConfig>& configs) const;

    // 配置验证辅助方法
    static bool ValidateDispensingConfig(const Domain::Configuration::Ports::DispensingConfig& config,
                                         std::vector<std::string>& errors);
    static bool ValidateMachineConfig(const Domain::Configuration::Ports::MachineConfig& config,
                                      std::vector<std::string>& errors);
};

}  // namespace Siligen::Infrastructure::Adapters





