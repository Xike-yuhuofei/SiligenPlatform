// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "ConfigFile"

#include "ConfigFileAdapter.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

#include "validators/ConfigValidator.h"

namespace Siligen::Infrastructure::Adapters {

using namespace Domain::Configuration::Ports;
using namespace Shared::Types;

namespace {

HomingConfig NormalizeReadyZeroSpeedForPersistence(const HomingConfig& config) {
    HomingConfig normalized = config;
    if (normalized.ready_zero_speed_mm_s <= 0.0f) {
        normalized.ready_zero_speed_mm_s = normalized.locate_velocity;
    }
    return normalized;
}

std::vector<HomingConfig> NormalizeReadyZeroSpeedsForPersistence(const std::vector<HomingConfig>& configs) {
    std::vector<HomingConfig> normalized;
    normalized.reserve(configs.size());
    for (const auto& config : configs) {
        normalized.push_back(NormalizeReadyZeroSpeedForPersistence(config));
    }
    return normalized;
}

}  // namespace

ConfigFileAdapter::ConfigFileAdapter(const std::string& config_file_path)
    : config_file_path_(config_file_path), config_loaded_(false) {
    auto load_result = LoadConfiguration();
    if (load_result.IsError()) {
        throw std::runtime_error("配置加载失败: " + load_result.GetError().GetMessage());
    }
}

// === 配置加载和保存 ===

Result<SystemConfig> ConfigFileAdapter::LoadConfiguration() {
    config_loaded_ = false;
    ini_cache_loaded_ = false;
    ini_cache_.clear();

    SystemConfig config;

    auto ini_result = LoadIniCache();
    if (ini_result.IsError()) {
        return Result<SystemConfig>(ini_result.GetError());
    }

    // 加载点胶配置
    auto dispensing_result = LoadDispensingSection(config.dispensing);
    if (dispensing_result.IsError()) {
        return Result<SystemConfig>(dispensing_result.GetError());
    }

    // 加载机器配置
    auto machine_result = LoadMachineSection(config.machine);
    if (machine_result.IsError()) {
        return Result<SystemConfig>(machine_result.GetError());
    }

    // 加载DXF配置
    auto dxf_result = LoadDxfSection(config.dxf);
    if (dxf_result.IsError()) {
        return Result<SystemConfig>(dxf_result.GetError());
    }

    // 加载回零配置
    auto homing_result = LoadHomingSection(config.homing_configs);
    if (homing_result.IsError()) {
        return Result<SystemConfig>(homing_result.GetError());
    }

    cached_config_ = config;
    config_loaded_ = true;

    return Result<SystemConfig>(config);
}

Result<void> ConfigFileAdapter::SaveConfiguration(const SystemConfig& config) {
    SystemConfig normalized = config;
    normalized.homing_configs = NormalizeReadyZeroSpeedsForPersistence(config.homing_configs);

    // 保存各个配置段
    SaveDispensingSection(normalized.dispensing);
    SaveMachineSection(normalized.machine);
    SaveDxfSection(normalized.dxf);
    SaveHomingSection(normalized.homing_configs);

    cached_config_ = normalized;

    return Result<void>();
}

Result<void> ConfigFileAdapter::ReloadConfiguration() {
    config_loaded_ = false;
    auto result = LoadConfiguration();
    if (result.IsError()) {
        return Result<void>(result.GetError());
    }
    return Result<void>();
}

// === 点胶配置 ===

Result<DispensingConfig> ConfigFileAdapter::GetDispensingConfig() const {
    if (!config_loaded_) {
        return Result<DispensingConfig>(Shared::Types::Error(Shared::Types::ErrorCode::CONFIG_PARSE_ERROR,
                                                             "配置未加载,请先调用LoadConfiguration()"));
    }
    return Result<DispensingConfig>(cached_config_.dispensing);
}

Result<void> ConfigFileAdapter::SetDispensingConfig(const DispensingConfig& config) {
    cached_config_.dispensing = config;
    SaveDispensingSection(config);
    return Result<void>();
}

// === 机器配置 ===

Result<MachineConfig> ConfigFileAdapter::GetMachineConfig() const {
    if (!config_loaded_) {
        return Result<MachineConfig>(Shared::Types::Error(Shared::Types::ErrorCode::CONFIG_PARSE_ERROR,
                                                          "配置未加载,请先调用LoadConfiguration()"));
    }
    return Result<MachineConfig>(cached_config_.machine);
}

Result<void> ConfigFileAdapter::SetMachineConfig(const MachineConfig& config) {
    cached_config_.machine = config;
    SaveMachineSection(config);
    return Result<void>();
}

// === 回零配置 ===

Result<HomingConfig> ConfigFileAdapter::GetHomingConfig(int axis) const {
    if (!config_loaded_) {
        return Result<HomingConfig>(Shared::Types::Error(Shared::Types::ErrorCode::CONFIG_PARSE_ERROR,
                                                         "配置未加载,请先调用LoadConfiguration()"));
    }

    if (axis < 0 || axis >= static_cast<int>(cached_config_.homing_configs.size())) {
        return Result<HomingConfig>(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS, "无效的轴编号: " + std::to_string(axis)));
    }

    return Result<HomingConfig>(cached_config_.homing_configs[axis]);
}

Result<void> ConfigFileAdapter::SetHomingConfig(int axis, const HomingConfig& config) {
    auto hw_config_result = GetHardwareConfiguration();
    if (hw_config_result.IsError()) {
        return Result<void>(hw_config_result.GetError());
    }
    const int axis_count = static_cast<int>(hw_config_result.Value().EffectiveAxisCount());
    if (axis < 0 || axis >= axis_count) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS, "无效的轴编号: " + std::to_string(axis)));
    }

    // 确保vector大小足够
    if (cached_config_.homing_configs.size() <= static_cast<size_t>(axis)) {
        cached_config_.homing_configs.resize(axis + 1);
    }

    cached_config_.homing_configs[axis] = NormalizeReadyZeroSpeedForPersistence(config);
    SaveHomingSection(cached_config_.homing_configs);

    return Result<void>();
}

Result<std::vector<HomingConfig>> ConfigFileAdapter::GetAllHomingConfigs() const {
    if (!config_loaded_) {
        return Result<std::vector<HomingConfig>>(Shared::Types::Error(Shared::Types::ErrorCode::CONFIG_PARSE_ERROR,
                                                                      "配置未加载,请先调用LoadConfiguration()"));
    }
    return Result<std::vector<HomingConfig>>(cached_config_.homing_configs);
}

Result<ValveSupplyConfig> ConfigFileAdapter::GetValveSupplyConfig() const {
    if (!config_loaded_) {
        return Result<ValveSupplyConfig>(
            Shared::Types::Error(Shared::Types::ErrorCode::CONFIG_PARSE_ERROR,
                                 "配置未加载,请先调用LoadConfiguration()"));
    }

    ValveSupplyConfig config;

    auto card_result = ReadIntValue("ValveSupply", "do_card_index");
    if (card_result.IsError()) {
        return Result<ValveSupplyConfig>(card_result.GetError());
    }
    config.do_card_index = card_result.Value();

    auto bit_result = ReadIntValue("ValveSupply", "do_bit_index");
    if (bit_result.IsError()) {
        return Result<ValveSupplyConfig>(bit_result.GetError());
    }
    config.do_bit_index = bit_result.Value();

    if (!config.IsValid()) {
        return Result<ValveSupplyConfig>(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER,
                                 "供胶阀配置无效: do_card_index="
                                 + std::to_string(config.do_card_index)
                                 + ", do_bit_index="
                                 + std::to_string(config.do_bit_index)));
    }

    return Result<ValveSupplyConfig>(config);
}

// === 配置验证 ===

Result<bool> ConfigFileAdapter::ValidateConfiguration() const {
    if (!config_loaded_) {
        return Result<bool>(Shared::Types::Error(Shared::Types::ErrorCode::CONFIG_PARSE_ERROR,
                                                 "配置未加载,请先调用LoadConfiguration()"));
    }

    std::vector<std::string> errors;

    bool valid = ValidateDispensingConfig(cached_config_.dispensing, errors) &&
                 ValidateMachineConfig(cached_config_.machine, errors);

    auto hw_config_result = GetHardwareConfiguration();
    if (hw_config_result.IsError()) {
        return Result<bool>(hw_config_result.GetError());
    }
    const int axis_count = static_cast<int>(hw_config_result.Value().EffectiveAxisCount());

    for (const auto& homing_config : cached_config_.homing_configs) {
        auto homing_result = ConfigValidator::ValidateHomingConfigDetailed(
            homing_config,
            cached_config_.machine,
            axis_count);
        if (!homing_result.is_valid) {
            valid = false;
        }
        errors.insert(errors.end(), homing_result.errors.begin(), homing_result.errors.end());
        errors.insert(errors.end(), homing_result.warnings.begin(), homing_result.warnings.end());
    }

    return Result<bool>(valid);
}

Result<std::vector<std::string>> ConfigFileAdapter::GetValidationErrors() const {
    if (!config_loaded_) {
        return Result<std::vector<std::string>>(Shared::Types::Error(
            Shared::Types::ErrorCode::CONFIG_PARSE_ERROR, "配置未加载,请先调用LoadConfiguration()"));
    }

    std::vector<std::string> errors;

    ValidateDispensingConfig(cached_config_.dispensing, errors);
    ValidateMachineConfig(cached_config_.machine, errors);

    auto hw_config_result = GetHardwareConfiguration();
    if (hw_config_result.IsError()) {
        return Result<std::vector<std::string>>(hw_config_result.GetError());
    }
    const int axis_count = static_cast<int>(hw_config_result.Value().EffectiveAxisCount());

    for (const auto& homing_config : cached_config_.homing_configs) {
        auto homing_result = ConfigValidator::ValidateHomingConfigDetailed(
            homing_config,
            cached_config_.machine,
            axis_count);
        errors.insert(errors.end(), homing_result.errors.begin(), homing_result.errors.end());
        errors.insert(errors.end(), homing_result.warnings.begin(), homing_result.warnings.end());
    }

    return Result<std::vector<std::string>>(errors);
}

// === 配置备份和恢复 ===

Result<void> ConfigFileAdapter::BackupConfiguration(const std::string& backup_path) {
    try {
        std::filesystem::copy_file(config_file_path_, backup_path, std::filesystem::copy_options::overwrite_existing);
        return Result<void>();
    } catch (const std::exception& e) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::CONFIG_PARSE_ERROR,
                                                 std::string("备份配置文件失败: ") + e.what()));
    }
}

Result<void> ConfigFileAdapter::RestoreConfiguration(const std::string& backup_path) {
    try {
        std::filesystem::copy_file(backup_path, config_file_path_, std::filesystem::copy_options::overwrite_existing);
        return ReloadConfiguration();
    } catch (const std::exception& e) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::CONFIG_PARSE_ERROR,
                                                 std::string("恢复配置文件失败: ") + e.what()));
    }
}


}  // namespace Siligen::Infrastructure::Adapters

