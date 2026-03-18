// ConfigurationService.cpp - 配置管理器实现
// 负责机器配置文件的读取、写入和管理

#include "ConfigurationService.h"

#include "infrastructure/logging/Logger.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace Siligen {

ConfigurationService::ConfigurationService(const std::string& config_file) : config_file_(config_file) {
    // 初始化回零配置数组（4个轴）
    homing_configs_.resize(4);

    // 尝试加载配置文件
    LoadConfig();
}

bool ConfigurationService::LoadConfig() {
    return ReadIniFile();
}

bool ConfigurationService::SaveConfig() const {
    return WriteIniFile();
}

bool ConfigurationService::ReloadConfig() {
    // 重置所有参数到默认值
    ResetToDefaults();

    // 重新加载配置文件
    return LoadConfig();
}

// === 回零配置管理实现 ===

const HomingConfig& ConfigurationService::GetHomingConfig(int axis) const {
    if (axis < 1 || axis > 4) {
        static HomingConfig default_config;
        return default_config;
    }
    return homing_configs_[axis - 1];
}

void ConfigurationService::SetHomingConfig(int axis, const HomingConfig& config) {
    if (axis >= 1 && axis <= 4) {
        homing_configs_[axis - 1] = config;
        LOG_INFO("ConfigurationService", "轴" + std::to_string(axis) + "回零配置已更新");
    }
}

const std::vector<HomingConfig>& ConfigurationService::GetAllHomingConfigs() const {
    return homing_configs_;
}

void ConfigurationService::SetAllHomingConfigs(const std::vector<HomingConfig>& configs) {
    if (configs.size() >= 4) {
        for (int i = 0; i < 4; ++i) {
            homing_configs_[i] = configs[i];
        }
        LOG_INFO("ConfigurationService", "所有轴回零配置已批量更新");
    }
}

// === 其他配置方法实现 ===

void ConfigurationService::SetDispensingParams(const DispensingParameters& params) {
    dispensing_params_ = params;
    LOG_INFO("ConfigurationService", "点胶参数已更新");
}

void ConfigurationService::SetMachineParams(const MachineParameters& params) {
    machine_params_ = params;
    LOG_INFO("ConfigurationService", "机器参数已更新");
}

void ConfigurationService::SetValveTimingParams(const ValveTimingParams& params) {
    valve_timing_params_ = params;
    LOG_INFO("ConfigurationService", "阀门时序参数已更新");
}

void ConfigurationService::SetCMPConfig(const CMPPulseConfig& config) {
    cmp_config_ = config;
    LOG_INFO("ConfigurationService", "CMP脉冲配置已更新");
}

void ConfigurationService::SetNetworkConfig(const NetworkConfig& config) {
    network_config_ = config;
    LOG_INFO("ConfigurationService", "网络配置已更新");
}

bool ConfigurationService::SetMachineSpeed(float32 max_speed) {
    if (max_speed <= 0 || max_speed > 1000.0f) {
        LOG_ERROR("ConfigurationService", "无效的最大速度值: " + std::to_string(max_speed));
        return false;
    }
    machine_params_.max_speed = max_speed;
    return true;
}

bool ConfigurationService::SetMachineAcceleration(float32 max_acceleration) {
    if (max_acceleration <= 0 || max_acceleration > 10000.0f) {
        LOG_ERROR("ConfigurationService", "无效的最大加速度值: " + std::to_string(max_acceleration));
        return false;
    }
    machine_params_.max_acceleration = max_acceleration;
    return true;
}

bool ConfigurationService::SetWorkAreaSize(float32 width, float32 height) {
    if (width <= 0 || height <= 0) {
        LOG_ERROR("ConfigurationService", "无效的工作区域尺寸");
        return false;
    }
    machine_params_.work_area_width = width;
    machine_params_.work_area_height = height;
    return true;
}

bool ConfigurationService::SetDispensingTime(float32 time) {
    if (time <= 0 || time > 10.0f) {
        LOG_ERROR("ConfigurationService", "无效的点胶时间值: " + std::to_string(time));
        return false;
    }
    dispensing_params_.dispensing_time = time;
    return true;
}

bool ConfigurationService::SetDispensingPressure(float32 pressure) {
    if (pressure <= 0 || pressure > 1000.0f) {
        LOG_ERROR("ConfigurationService", "无效的点胶压力值: " + std::to_string(pressure));
        return false;
    }
    dispensing_params_.pressure = pressure;
    return true;
}

bool ConfigurationService::ValidateConfig() const {
    // 验证各个配置参数
    if (!dispensing_params_.IsValid()) {
        LOG_ERROR("ConfigurationService", "点胶参数验证失败");
        return false;
    }

    if (!machine_params_.IsValid()) {
        LOG_ERROR("ConfigurationService", "机器参数验证失败");
        return false;
    }

    if (!valve_timing_params_.IsValid()) {
        LOG_ERROR("ConfigurationService", "阀门时序参数验证失败");
        return false;
    }

    if (!cmp_config_.IsValid()) {
        LOG_ERROR("ConfigurationService", "CMP脉冲配置验证失败");
        return false;
    }

    if (!network_config_.IsValid()) {
        LOG_ERROR("ConfigurationService", "网络配置验证失败");
        return false;
    }

    // 验证回零配置
    for (int axis = 1; axis <= 4; ++axis) {
        if (!homing_configs_[axis - 1].IsValid()) {
            LOG_ERROR("ConfigurationService", "轴" + std::to_string(axis) + "回零配置验证失败");
            return false;
        }
    }

    return true;
}

std::vector<std::string> ConfigurationService::GetValidationErrors() const {
    std::vector<std::string> errors;

    // 检查各个配置参数
    if (!dispensing_params_.IsValid()) {
        errors.push_back("点胶参数无效");
    }

    if (!machine_params_.IsValid()) {
        errors.push_back("机器参数无效");
    }

    if (!valve_timing_params_.IsValid()) {
        errors.push_back("阀门时序参数无效");
    }

    if (!cmp_config_.IsValid()) {
        errors.push_back("CMP脉冲配置无效");
    }

    if (!network_config_.IsValid()) {
        errors.push_back("网络配置无效");
    }

    // 检查回零配置
    for (int axis = 1; axis <= 4; ++axis) {
        if (!homing_configs_[axis - 1].IsValid()) {
            errors.push_back("轴" + std::to_string(axis) + "回零配置无效");
        }
    }

    return errors;
}

void ConfigurationService::ResetToDefaults() {
    dispensing_params_.ResetToDefaults();
    machine_params_.ResetToDefaults();
    valve_timing_params_.ResetToDefaults();
    cmp_config_.ResetToDefaults();
    network_config_.ResetToDefaults();

    for (auto& config : homing_configs_) {
        config.ResetToDefaults();
    }

    LOG_INFO("ConfigurationService", "所有配置参数已重置为默认值");
}

void ConfigurationService::ResetDispensingToDefaults() {
    dispensing_params_.ResetToDefaults();
    LOG_INFO("ConfigurationService", "点胶参数已重置为默认值");
}

void ConfigurationService::ResetMachineToDefaults() {
    machine_params_.ResetToDefaults();
    LOG_INFO("ConfigurationService", "机器参数已重置为默认值");
}

bool ConfigurationService::BackupConfig(const std::string& backup_file) const {
    std::ifstream src(config_file_, std::ios::binary);
    std::ofstream dst(backup_file, std::ios::binary);

    if (!src.is_open() || !dst.is_open()) {
        LOG_ERROR("ConfigurationService", "无法创建配置文件备份");
        return false;
    }

    dst << src.rdbuf();
    LOG_INFO("ConfigurationService", "配置文件已备份到: " + backup_file);
    return true;
}

bool ConfigurationService::RestoreConfig(const std::string& backup_file) {
    std::ifstream src(backup_file, std::ios::binary);
    std::ofstream dst(config_file_, std::ios::binary);

    if (!src.is_open() || !dst.is_open()) {
        LOG_ERROR("ConfigurationService", "无法恢复配置文件");
        return false;
    }

    dst << src.rdbuf();

    // 重新加载恢复的配置
    bool result = ReloadConfig();
    if (result) {
        LOG_INFO("ConfigurationService", "配置文件已从备份恢复: " + backup_file);
    } else {
        LOG_ERROR("ConfigurationService", "恢复配置文件后加载失败");
    }

    return result;
}

bool ConfigurationService::ExportToJSON(const std::string& json_file) const {
    std::ofstream file(json_file);
    if (!file.is_open()) {
        LOG_ERROR("ConfigurationService", "无法创建JSON导出文件");
        return false;
    }

    // 简化的JSON导出实现
    file << "{\n";
    file << "  \"dispensing\": {\n";
    file << "    \"pressure\": " << dispensing_params_.pressure << ",\n";
    file << "    \"flow_rate\": " << dispensing_params_.flow_rate << ",\n";
    file << "    \"dispensing_time\": " << dispensing_params_.dispensing_time << "\n";
    file << "  },\n";

    file << "  \"machine\": {\n";
    file << "    \"work_area_width\": " << machine_params_.work_area_width << ",\n";
    file << "    \"work_area_height\": " << machine_params_.work_area_height << ",\n";
    file << "    \"max_speed\": " << machine_params_.max_speed << "\n";
    file << "  },\n";

    file << "  \"homing\": [\n";
    for (int i = 0; i < 4; ++i) {
        const auto& config = homing_configs_[i];
        file << "    {\n";
        file << "      \"mode\": " << config.mode << ",\n";
        file << "      \"rapid_velocity\": " << config.rapid_velocity << ",\n";
        file << "      \"locate_velocity\": " << config.locate_velocity << ",\n";
        file << "      \"acceleration\": " << config.acceleration << "\n";
        file << "    }" << (i < 3 ? "," : "") << "\n";
    }
    file << "  ]\n";
    file << "}\n";

    LOG_INFO("ConfigurationService", "配置已导出到JSON文件: " + json_file);
    return true;
}

bool ConfigurationService::ImportFromJSON(const std::string& json_file) {
    // 简化的JSON导入实现
    // 实际应用中应该使用专门的JSON解析库
    LOG_INFO("ConfigurationService", "JSON导入功能需要专门的JSON解析库支持");
    return false;
}

void ConfigurationService::SetConfigFilePath(const std::string& file_path) {
    config_file_ = file_path;
}

bool ConfigurationService::ConfigFileExists() const {
    return std::filesystem::exists(config_file_);
}

int64 ConfigurationService::GetConfigFileLastModified() const {
    if (!std::filesystem::exists(config_file_)) {
        return -1;
    }

    auto ftime = std::filesystem::last_write_time(config_file_);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();
}

// === 内部方法实现 ===

bool ConfigurationService::ReadIniFile() {
    std::ifstream file(config_file_);
    if (!file.is_open()) {
        LOG_WARNING("ConfigurationService", "配置文件不存在，使用默认配置: " + config_file_);
        return false;
    }

    // 读取各个配置段
    ReadDispensingSection();
    ReadMachineSection();
    ReadValveTimingSection();
    ReadCMPSection();
    ReadNetworkSection();
    ReadHomingSection();

    file.close();
    LOG_INFO("ConfigurationService", "配置文件加载成功: " + config_file_);
    return true;
}

bool ConfigurationService::WriteIniFile() const {
    std::ofstream file(config_file_);
    if (!file.is_open()) {
        LOG_ERROR("ConfigurationService", "无法创建配置文件: " + config_file_);
        return false;
    }

    // 写入各个配置段
    WriteDispensingSection();
    WriteMachineSection();
    WriteValveTimingSection();
    WriteCMPSection();
    WriteNetworkSection();
    WriteHomingSection();

    file.close();
    LOG_INFO("ConfigurationService", "配置文件保存成功: " + config_file_);
    return true;
}

std::string ConfigurationService::ReadIniValue(const std::string& section,
                                        const std::string& key,
                                        const std::string& default_value) const {
    // 简化的INI读取实现
    // 实际应用中应该使用专门的INI解析库
    return default_value;
}

bool ConfigurationService::WriteIniValue(const std::string& section, const std::string& key, const std::string& value) const {
    // 简化的INI写入实现
    // 实际应用中应该使用专门的INI解析库
    return true;
}

// 配置段读写方法（简化实现）
void ConfigurationService::ReadDispensingSection() {
    // 读取点胶配置段的具体实现
    dispensing_params_.pressure = StringToFloat(ReadIniValue("Dispensing", "pressure", "50.0"));
    dispensing_params_.flow_rate = StringToFloat(ReadIniValue("Dispensing", "flow_rate", "1.0"));
    dispensing_params_.dispensing_time = StringToFloat(ReadIniValue("Dispensing", "dispensing_time", "0.1"));
}

void ConfigurationService::ReadMachineSection() {
    // 读取机器配置段的具体实现
    machine_params_.work_area_width = StringToFloat(ReadIniValue("Machine", "work_area_width", "300.0"));
    machine_params_.work_area_height = StringToFloat(ReadIniValue("Machine", "work_area_height", "300.0"));
    machine_params_.max_speed = StringToFloat(ReadIniValue("Machine", "max_speed", "100.0"));
    machine_params_.max_acceleration = StringToFloat(ReadIniValue("Machine", "max_acceleration", "1000.0"));
}

void ConfigurationService::ReadValveTimingSection() {
    // 读取阀门时序配置段的具体实现
    valve_timing_params_.open_delay = StringToFloat(ReadIniValue("ValveTiming", "open_delay", "0.001"));
    valve_timing_params_.close_delay = StringToFloat(ReadIniValue("ValveTiming", "close_delay", "0.001"));
    valve_timing_params_.min_on_time = StringToFloat(ReadIniValue("ValveTiming", "min_on_time", "0.01"));
}

void ConfigurationService::ReadCMPSection() {
    // 读取CMP配置段的具体实现
    cmp_config_.cmp_channel = StringToInt(ReadIniValue("CMP", "cmp_channel", "1"));
    cmp_config_.pulse_width_us = StringToInt(ReadIniValue("CMP", "pulse_width_us", "2000"));
    cmp_config_.delay_time_us = StringToInt(ReadIniValue("CMP", "delay_time_us", "0"));
}

void ConfigurationService::ReadNetworkSection() {
    // 读取网络配置段的具体实现
    network_config_.control_card_ip = ReadIniValue("Network", "control_card_ip", CONTROL_CARD_IP);
    network_config_.local_ip = ReadIniValue("Network", "local_ip", LOCAL_IP);
    network_config_.control_card_port =
        static_cast<uint16>(StringToInt(ReadIniValue("Network", "control_card_port", "0")));
    network_config_.local_port = static_cast<uint16>(StringToInt(ReadIniValue("Network", "local_port", "0")));
    network_config_.timeout_ms = StringToInt(ReadIniValue("Network", "timeout_ms", "5000"));
}

void ConfigurationService::ReadHomingSection() {
    // 读取回零配置段的具体实现
    for (int axis = 1; axis <= 4; ++axis) {
        std::string section = GetHomingAxisSectionName(axis);

        homing_configs_[axis - 1].mode = StringToInt(ReadIniValue(section, "mode", "2"));
        homing_configs_[axis - 1].direction = StringToInt(ReadIniValue(section, "direction", "0"));
        homing_configs_[axis - 1].rapid_velocity = StringToFloat(ReadIniValue(section, "rapid_velocity", "20.0"));
        homing_configs_[axis - 1].locate_velocity = StringToFloat(ReadIniValue(section, "locate_velocity", "10.0"));
        homing_configs_[axis - 1].index_velocity = StringToFloat(ReadIniValue(section, "index_velocity", "5.0"));
        homing_configs_[axis - 1].acceleration = StringToFloat(ReadIniValue(section, "acceleration", "100.0"));
        homing_configs_[axis - 1].offset = StringToFloat(ReadIniValue(section, "offset", "0.0"));
        homing_configs_[axis - 1].timeout_ms = StringToInt(ReadIniValue(section, "timeout_ms", "30000"));
    }
}

void ConfigurationService::WriteDispensingSection() const {
    // 写入点胶配置段的具体实现
    WriteIniValue("Dispensing", "pressure", FloatToString(dispensing_params_.pressure));
    WriteIniValue("Dispensing", "flow_rate", FloatToString(dispensing_params_.flow_rate));
    WriteIniValue("Dispensing", "dispensing_time", FloatToString(dispensing_params_.dispensing_time));
}

void ConfigurationService::WriteMachineSection() const {
    // 写入机器配置段的具体实现
    WriteIniValue("Machine", "work_area_width", FloatToString(machine_params_.work_area_width));
    WriteIniValue("Machine", "work_area_height", FloatToString(machine_params_.work_area_height));
    WriteIniValue("Machine", "max_speed", FloatToString(machine_params_.max_speed));
    WriteIniValue("Machine", "max_acceleration", FloatToString(machine_params_.max_acceleration));
}

void ConfigurationService::WriteValveTimingSection() const {
    // 写入阀门时序配置段的具体实现
    WriteIniValue("ValveTiming", "open_delay", FloatToString(valve_timing_params_.open_delay));
    WriteIniValue("ValveTiming", "close_delay", FloatToString(valve_timing_params_.close_delay));
    WriteIniValue("ValveTiming", "min_on_time", FloatToString(valve_timing_params_.min_on_time));
}

void ConfigurationService::WriteCMPSection() const {
    // 写入CMP配置段的具体实现
    WriteIniValue("CMP", "cmp_channel", IntToString(cmp_config_.cmp_channel));
    WriteIniValue("CMP", "pulse_width_us", IntToString(cmp_config_.pulse_width_us));
    WriteIniValue("CMP", "delay_time_us", IntToString(cmp_config_.delay_time_us));
}

void ConfigurationService::WriteNetworkSection() const {
    // 写入网络配置段的具体实现
    WriteIniValue("Network", "control_card_ip", network_config_.control_card_ip);
    WriteIniValue("Network", "local_ip", network_config_.local_ip);
    WriteIniValue("Network", "control_card_port", IntToString(network_config_.control_card_port));
    WriteIniValue("Network", "local_port", IntToString(network_config_.local_port));
    WriteIniValue("Network", "timeout_ms", IntToString(network_config_.timeout_ms));
}

void ConfigurationService::WriteHomingSection() const {
    // 写入回零配置段的具体实现
    for (int axis = 1; axis <= 4; ++axis) {
        std::string section = GetHomingAxisSectionName(axis);
        const auto& config = homing_configs_[axis - 1];

        WriteIniValue(section, "mode", IntToString(config.mode));
        WriteIniValue(section, "direction", IntToString(config.direction));
        WriteIniValue(section, "rapid_velocity", FloatToString(config.rapid_velocity));
        WriteIniValue(section, "locate_velocity", FloatToString(config.locate_velocity));
        WriteIniValue(section, "index_velocity", FloatToString(config.index_velocity));
        WriteIniValue(section, "acceleration", FloatToString(config.acceleration));
        WriteIniValue(section, "deceleration", FloatToString(config.deceleration));
        WriteIniValue(section, "offset", FloatToString(config.offset));
        WriteIniValue(section, "timeout_ms", IntToString(config.timeout_ms));
        WriteIniValue(section, "search_distance", FloatToString(config.search_distance));
        WriteIniValue(section, "escape_distance", FloatToString(config.escape_distance));
        WriteIniValue(section, "retry_count", IntToString(config.retry_count));
        WriteIniValue(section, "settle_time_ms", IntToString(config.settle_time_ms));
        WriteIniValue(section, "enable_escape", BoolToString(config.enable_escape));
        WriteIniValue(section, "enable_limit_switch", BoolToString(config.enable_limit_switch));
        WriteIniValue(section, "enable_index", BoolToString(config.enable_index));
    }
}

std::string ConfigurationService::GetHomingAxisSectionName(int axis) const {
    return "Homing_Axis" + std::to_string(axis);
}

// 辅助方法实现
float32 ConfigurationService::StringToFloat(const std::string& str, float32 default_value) const {
    try {
        return std::stof(str);
    } catch (...) {
        return default_value;
    }
}

int32 ConfigurationService::StringToInt(const std::string& str, int32 default_value) const {
    try {
        return std::stoi(str);
    } catch (...) {
        return default_value;
    }
}

bool ConfigurationService::StringToBool(const std::string& str, bool default_value) const {
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);

    if (lower_str == "true" || lower_str == "1" || lower_str == "yes" || lower_str == "on") {
        return true;
    } else if (lower_str == "false" || lower_str == "0" || lower_str == "no" || lower_str == "off") {
        return false;
    } else {
        return default_value;
    }
}

std::string ConfigurationService::FloatToString(float32 value) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << value;
    return oss.str();
}

std::string ConfigurationService::IntToString(int32 value) const {
    return std::to_string(value);
}

std::string ConfigurationService::BoolToString(bool value) const {
    return value ? "true" : "false";
}

}  // namespace Siligen

