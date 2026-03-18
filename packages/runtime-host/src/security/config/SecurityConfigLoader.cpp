#include "SecurityConfigLoader.h"

#include "security/SecurityLogHelper.h"
#include "shared/errors/ErrorDescriptions.h"
#include "shared/errors/ErrorHandler.h"

#include <fstream>
#include <sstream>

namespace Siligen {

using Shared::Types::LogLevel;

bool SecurityConfigLoader::Load(const std::string& config_file) {
    SecurityLogHelper::Log(LogLevel::INFO, "SecurityConfigLoader", "开始加载安全配置: " + config_file);

    std::ifstream file(config_file);
    if (!file.is_open()) {
        SecurityLogHelper::Log(
            LogLevel::ERR,
            "SecurityConfigLoader",
            "无法打开配置文件: " + config_file +
                " (错误码: " + std::to_string(static_cast<int32>(SystemErrorCode::CONFIG_LOAD_FAILED)) + ")");
        return false;
    }

    SecurityLogHelper::Log(LogLevel::INFO, "SecurityConfigLoader", "加载安全配置: " + config_file);

    std::string line, current_section;
    int32 line_count = 0;
    bool version_found = false;

    while (std::getline(file, line)) {
        line_count++;
        // 移除首尾空格
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        // 跳过空行和注释
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        // 处理section
        if (line[0] == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            SecurityLogHelper::Log(LogLevel::DEBUG, "SecurityConfigLoader", "解析配置段: [" + current_section + "]");
            continue;
        }

        // 解析key=value
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        // 检查版本字段
        if (current_section == "General" && key == "version") {
            config_.version = value;
            version_found = true;
            // 验证版本兼容性
            if (!version_mgr_.ValidateVersion(value, "1.0.0")) {
                SecurityLogHelper::Log(LogLevel::ERR,
                            "SecurityConfigLoader",
                            "[ERROR] ConfigurationVersionService: " + version_mgr_.GetLastError() + " (" +
                                std::to_string(static_cast<int32>(SystemErrorCode::CONFIG_VERSION_INCOMPATIBLE)) + ")");
                return false;
            }
            SecurityLogHelper::Log(LogLevel::INFO, "SecurityConfigLoader", "配置文件版本: " + value);
        }

        if (!ParseLine(current_section, key, value)) {
            SecurityLogHelper::Log(LogLevel::WARNING, "SecurityConfigLoader", "跳过无效配置项: [" + current_section + "] " + key);
        }
    }

    // 版本检查
    if (!version_found) {
        SecurityLogHelper::Log(LogLevel::ERR,
                    "SecurityConfigLoader",
                    "[ERROR] 配置文件缺少版本字段 (" +
                        std::to_string(static_cast<int32>(SystemErrorCode::CONFIG_VERSION_INCOMPATIBLE)) + ")");
        return false;
    }

    if (!Validate()) {
        SecurityLogHelper::Log(
            LogLevel::ERR,
            "SecurityConfigLoader",
            "配置验证失败 (错误码: " +
                std::to_string(static_cast<int32>(SystemErrorCode::CONFIG_VALIDATION_FAILED)) + ")");
        return false;
    }

    SecurityLogHelper::Log(LogLevel::INFO, "SecurityConfigLoader", "安全配置加载完成");
    return true;
}

bool SecurityConfigLoader::ParseLine(const std::string& section, const std::string& key, const std::string& value) {
    if (section == "General") {
        if (key == "session_timeout_minutes")
            config_.session_timeout_minutes = std::stoi(value);
        else if (key == "password_min_length")
            config_.min_password_length = std::stoi(value);
        else if (key == "max_login_attempts")
            config_.max_failed_login_attempts = std::stoi(value);
        else if (key == "lockout_duration_minutes")
            config_.lockout_duration_minutes = std::stoi(value);
        else if (key == "audit_retention_days")
            config_.audit_log_retention_days = std::stoi(value);
        else if (key == "version")
            return true;  // 已在前面处理
    } else if (section == "SafetyLimits") {
        if (key == "max_speed_mm_s")
            config_.max_speed_mm_s = std::stof(value);
        else if (key == "max_acceleration_mm_s2")
            config_.max_acceleration_mm_s2 = std::stof(value);
        else if (key == "max_jerk_mm_s3")
            config_.max_jerk_mm_s3 = std::stof(value);
        else if (key == "workspace_min_x")
            config_.x_min_mm = std::stof(value);
        else if (key == "workspace_max_x")
            config_.x_max_mm = std::stof(value);
        else if (key == "workspace_min_y")
            config_.y_min_mm = std::stof(value);
        else if (key == "workspace_max_y")
            config_.y_max_mm = std::stof(value);
    } else if (section == "NetworkWhitelist") {
        if (key == "enabled")
            config_.enable_ip_whitelist = (value == "true" || value == "1");
        else if (key == "allowed_ips")
            ParseIPWhitelist(value);
    } else if (section == "Interlock") {
        if (key == "enabled")
            config_.interlock_enabled = (value == "true" || value == "1");
        else if (key == "emergency_stop_input")
            config_.emergency_stop_input = static_cast<int16>(std::stoi(value));
        else if (key == "safety_door_input")
            config_.safety_door_input = static_cast<int16>(std::stoi(value));
        else if (key == "pressure_sensor_input")
            config_.pressure_sensor_input = static_cast<int16>(std::stoi(value));
        else if (key == "temperature_sensor_input")
            config_.temperature_sensor_input = static_cast<int16>(std::stoi(value));
        else if (key == "voltage_sensor_input")
            config_.voltage_sensor_input = static_cast<int16>(std::stoi(value));
        else if (key == "poll_interval_ms")
            config_.interlock_poll_interval_ms = std::stoi(value);
        else if (key == "pressure_critical_low_mpa")
            config_.pressure_critical_low = std::stof(value);
        else if (key == "pressure_warning_low_mpa")
            config_.pressure_warning_low = std::stof(value);
        else if (key == "pressure_warning_high_mpa")
            config_.pressure_warning_high = std::stof(value);
        else if (key == "pressure_critical_high_mpa")
            config_.pressure_critical_high = std::stof(value);
        else if (key == "temperature_critical_low_celsius")
            config_.temperature_critical_low = std::stof(value);
        else if (key == "temperature_normal_low_celsius")
            config_.temperature_normal_low = std::stof(value);
        else if (key == "temperature_normal_high_celsius")
            config_.temperature_normal_high = std::stof(value);
        else if (key == "temperature_critical_high_celsius")
            config_.temperature_critical_high = std::stof(value);
        else if (key == "voltage_min_v")
            config_.voltage_min = std::stof(value);
        else if (key == "voltage_max_v")
            config_.voltage_max = std::stof(value);
        else if (key == "self_test_interval_hours")
            config_.interlock_self_test_interval_hours = std::stoi(value);
    } else {
        return false;
    }
    return true;
}

void SecurityConfigLoader::ParseIPWhitelist(const std::string& value) {
    std::istringstream iss(value);
    std::string ip;
    config_.ip_whitelist.clear();
    while (std::getline(iss, ip, ',')) {
        ip.erase(0, ip.find_first_not_of(" \t"));
        ip.erase(ip.find_last_not_of(" \t") + 1);
        if (!ip.empty()) {
            config_.ip_whitelist.push_back(ip);
        }
    }
}

bool SecurityConfigLoader::Validate() const {
    bool valid = true;

    if (config_.session_timeout_minutes < 1 || config_.session_timeout_minutes > 1440) {
        SecurityLogHelper::Log(LogLevel::ERR, "SecurityConfigLoader", "会话超时时间无效(1-1440分钟)");
        valid = false;
    }

    if (config_.max_failed_login_attempts < 1 || config_.max_failed_login_attempts > 20) {
        SecurityLogHelper::Log(LogLevel::ERR, "SecurityConfigLoader", "最大登录失败次数无效(1-20)");
        valid = false;
    }

    if (config_.min_password_length < 6 || config_.min_password_length > 32) {
        SecurityLogHelper::Log(LogLevel::ERR, "SecurityConfigLoader", "密码长度要求无效(6-32)");
        valid = false;
    }

    if (config_.max_speed_mm_s <= 0.0f || config_.max_speed_mm_s > 50.0f) {
        SecurityLogHelper::Log(LogLevel::ERR, "SecurityConfigLoader", "最大速度无效(0-50 mm/s)");
        valid = false;
    }

    if (config_.x_min_mm >= config_.x_max_mm || config_.y_min_mm >= config_.y_max_mm ||
        config_.z_min_mm >= config_.z_max_mm) {
        SecurityLogHelper::Log(LogLevel::ERR, "SecurityConfigLoader", "工作空间边界无效");
        valid = false;
    }

    return valid;
}

}  // namespace Siligen

