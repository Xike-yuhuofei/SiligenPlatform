// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "ConfigFile"

#include "ConfigFileAdapter.h"

#include "shared/interfaces/ILoggingService.h"
#include "siligen/shared/strings/string_manipulator.h"

namespace Siligen::Infrastructure::Adapters {

using namespace Domain::Configuration::Ports;
using namespace Shared::Types;

namespace {
using StringManipulator = Siligen::SharedKernel::StringManipulator;
}

// === 硬件模式配置 ===

Result<Shared::Types::HardwareMode> ConfigFileAdapter::GetHardwareMode() const {
    if (!config_loaded_) {
        return Result<Shared::Types::HardwareMode>(
            Shared::Types::Error(Shared::Types::ErrorCode::CONFIG_PARSE_ERROR,
                                 "配置未加载,请先调用LoadConfiguration()"));
    }

    auto mode_result = ReadHardwareModeValue("Hardware", "mode");
    if (mode_result.IsError()) {
        return Result<Shared::Types::HardwareMode>(mode_result.GetError());
    }
    return Result<Shared::Types::HardwareMode>(mode_result.Value());
}

// === 硬件配置 ===

Result<Shared::Types::HardwareConfiguration> ConfigFileAdapter::GetHardwareConfiguration() const {
    if (!config_loaded_) {
        return Result<Shared::Types::HardwareConfiguration>(
            Shared::Types::Error(Shared::Types::ErrorCode::CONFIG_PARSE_ERROR,
                                 "配置未加载,请先调用LoadConfiguration()"));
    }

    Shared::Types::HardwareConfiguration config;

    auto assign_float = [this](const std::string& sec, const std::string& key, float32& out) -> Result<void> {
        auto result = ReadFloatValue(sec, key);
        if (result.IsError()) {
            return Result<void>(result.GetError());
        }
        out = result.Value();
        return Result<void>();
    };
    auto assign_int = [this](const std::string& sec, const std::string& key, int32& out) -> Result<void> {
        auto result = ReadIntValue(sec, key);
        if (result.IsError()) {
            return Result<void>(result.GetError());
        }
        out = result.Value();
        return Result<void>();
    };

    auto result = assign_float("Hardware", "pulse_per_mm", config.pulse_per_mm);
    if (result.IsError()) {
        return Result<Shared::Types::HardwareConfiguration>(result.GetError());
    }
    result = assign_float("Hardware", "max_velocity_mm_s", config.max_velocity_mm_s);
    if (result.IsError()) {
        return Result<Shared::Types::HardwareConfiguration>(result.GetError());
    }
    result = assign_float("Hardware", "max_acceleration_mm_s2", config.max_acceleration_mm_s2);
    if (result.IsError()) {
        return Result<Shared::Types::HardwareConfiguration>(result.GetError());
    }
    result = assign_float("Hardware", "max_deceleration_mm_s2", config.max_deceleration_mm_s2);
    if (result.IsError()) {
        return Result<Shared::Types::HardwareConfiguration>(result.GetError());
    }
    result = assign_float("Hardware", "position_tolerance_mm", config.position_tolerance_mm);
    if (result.IsError()) {
        return Result<Shared::Types::HardwareConfiguration>(result.GetError());
    }
    result = assign_float("Hardware", "soft_limit_positive_mm", config.soft_limit_positive_mm);
    if (result.IsError()) {
        return Result<Shared::Types::HardwareConfiguration>(result.GetError());
    }
    result = assign_float("Hardware", "soft_limit_negative_mm", config.soft_limit_negative_mm);
    if (result.IsError()) {
        return Result<Shared::Types::HardwareConfiguration>(result.GetError());
    }
    result = assign_int("Hardware", "response_timeout_ms", config.response_timeout_ms);
    if (result.IsError()) {
        return Result<Shared::Types::HardwareConfiguration>(result.GetError());
    }
    result = assign_int("Hardware", "motion_timeout_ms", config.motion_timeout_ms);
    if (result.IsError()) {
        return Result<Shared::Types::HardwareConfiguration>(result.GetError());
    }
    result = assign_int("Hardware", "num_axes", config.num_axes);
    if (result.IsError()) {
        return Result<Shared::Types::HardwareConfiguration>(result.GetError());
    }

    // ✅ 添加加载成功日志
    SILIGEN_LOG_INFO("硬件配置加载成功: pulse_per_mm=" + std::to_string(config.pulse_per_mm) +
                      ", max_velocity=" + std::to_string(config.max_velocity_mm_s) + " mm/s");

    // 验证配置有效性
    auto validation_result = config.Validate();
    if (validation_result.IsError()) {
        return Result<Shared::Types::HardwareConfiguration>::Failure(validation_result.GetError());
    }

    return Result<Shared::Types::HardwareConfiguration>::Success(config);
}

// === 阀门配置加载 (Valve configuration loading) ===

Result<Shared::Types::DispenserValveConfig> ConfigFileAdapter::GetDispenserValveConfig() const {
    Shared::Types::DispenserValveConfig config;
    auto load_result = LoadDispenserValveSection(config);
    if (load_result.IsError()) {
        return Result<Shared::Types::DispenserValveConfig>(load_result.GetError());
    }

    auto validation = config.Validate();
    if (!validation) {
        return Result<Shared::Types::DispenserValveConfig>::Failure(
            Error(ErrorCode::ValidationFailed, "DispenserValveConfig validation failed"));
    }

    return Result<Shared::Types::DispenserValveConfig>::Success(config);
}

Result<Shared::Types::SupplyValveConfig> ConfigFileAdapter::GetSupplyValveConfig() const {
    Shared::Types::SupplyValveConfig config;
    auto load_result = LoadSupplyValveSection(config);
    if (load_result.IsError()) {
        return Result<Shared::Types::SupplyValveConfig>(load_result.GetError());
    }

    auto validation = config.Validate();
    if (!validation) {
        return Result<Shared::Types::SupplyValveConfig>::Failure(
            Error(ErrorCode::ValidationFailed, "SupplyValveConfig validation failed"));
    }

    return Result<Shared::Types::SupplyValveConfig>::Success(config);
}

Result<void> ConfigFileAdapter::LoadDispenserValveSection(Shared::Types::DispenserValveConfig& config) const {
    auto assign_int = [this](const std::string& sec, const std::string& key, int32& out) -> Result<void> {
        auto result = ReadIntValue(sec, key);
        if (result.IsError()) {
            return Result<void>(result.GetError());
        }
        out = result.Value();
        return Result<void>();
    };
    auto read_optional_int = [this](const std::string& sec, const std::string& key, int32& out, bool& found)
        -> Result<void> {
        auto load_result = LoadIniCache();
        if (load_result.IsError()) {
            return Result<void>(load_result.GetError());
        }

        found = false;
        const std::string section_key = StringManipulator::ToLower(sec);
        const std::string key_name = StringManipulator::ToLower(key);
        auto section_it = ini_cache_.find(section_key);
        if (section_it == ini_cache_.end()) {
            return Result<void>();
        }
        auto key_it = section_it->second.find(key_name);
        if (key_it == section_it->second.end()) {
            return Result<void>();
        }

        std::string raw = StringManipulator::Trim(key_it->second);
        if (raw.empty()) {
            return Result<void>();
        }

        try {
            size_t idx = 0;
            int32 value = std::stoi(raw, &idx);
            if (idx != raw.size()) {
                return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                          "配置项格式错误: [" + sec + "] " + key + "=" + raw));
            }
            out = value;
            found = true;
            return Result<void>();
        } catch (...) {
            return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                      "配置项格式错误: [" + sec + "] " + key + "=" + raw));
        }
    };

    auto result = assign_int("ValveDispenser", "cmp_channel", config.cmp_channel);
    if (result.IsError()) return result;
    result = assign_int("ValveDispenser", "pulse_type", config.pulse_type);
    if (result.IsError()) return result;
    result = assign_int("ValveDispenser", "abs_position_flag", config.abs_position_flag);
    if (result.IsError()) return result;
    bool cmp_axis_mask_found = false;
    result = read_optional_int("ValveDispenser", "cmp_axis_mask", config.cmp_axis_mask, cmp_axis_mask_found);
    if (result.IsError()) return result;
    if (!cmp_axis_mask_found) {
        result = read_optional_int("CMP", "cmp_axis_mask", config.cmp_axis_mask, cmp_axis_mask_found);
        if (result.IsError()) return result;
    }
    result = assign_int("ValveDispenser", "min_count", config.min_count);
    if (result.IsError()) return result;
    result = assign_int("ValveDispenser", "max_count", config.max_count);
    if (result.IsError()) return result;
    result = assign_int("ValveDispenser", "min_interval_ms", config.min_interval_ms);
    if (result.IsError()) return result;
    result = assign_int("ValveDispenser", "max_interval_ms", config.max_interval_ms);
    if (result.IsError()) return result;
    result = assign_int("ValveDispenser", "min_duration_ms", config.min_duration_ms);
    if (result.IsError()) return result;
    result = assign_int("ValveDispenser", "max_duration_ms", config.max_duration_ms);
    if (result.IsError()) return result;

    SILIGEN_LOG_INFO("点胶阀配置: CMP channel=" + std::to_string(config.cmp_channel) +
                     ", pulse_type=" + std::to_string(config.pulse_type) +
                     ", abs_position_flag=" + std::to_string(config.abs_position_flag) +
                     ", cmp_axis_mask=" + std::to_string(config.cmp_axis_mask));
    return Result<void>();
}

Result<void> ConfigFileAdapter::LoadSupplyValveSection(Shared::Types::SupplyValveConfig& config) const {
    auto assign_int = [this](const std::string& sec, const std::string& key, int32& out) -> Result<void> {
        auto result = ReadIntValue(sec, key);
        if (result.IsError()) {
            return Result<void>(result.GetError());
        }
        out = result.Value();
        return Result<void>();
    };
    auto assign_bool = [this](const std::string& sec, const std::string& key, bool& out) -> Result<void> {
        auto result = ReadBoolValue(sec, key);
        if (result.IsError()) {
            return Result<void>(result.GetError());
        }
        out = result.Value();
        return Result<void>();
    };

    auto result = assign_int("ValveSupply", "do_bit_index", config.do_bit_index);
    if (result.IsError()) return result;
    result = assign_int("ValveSupply", "do_card_index", config.do_card_index);
    if (result.IsError()) return result;
    result = assign_int("ValveSupply", "timeout_ms", config.timeout_ms);
    if (result.IsError()) return result;
    result = assign_bool("ValveSupply", "fail_closed", config.fail_closed);
    if (result.IsError()) return result;

    SILIGEN_LOG_INFO("供胶阀配置: DO bit index=" + std::to_string(config.do_bit_index) +
                     " (Y" + std::to_string(config.do_bit_index) + ")" +
                     ", 卡索引=" + std::to_string(config.do_card_index));
    return Result<void>();
}

// === 日志配置加载 (Log configuration loading) ===

Result<LogConfig> ConfigFileAdapter::GetLogConfiguration() const {
    if (!config_loaded_) {
        return Result<LogConfig>(Shared::Types::Error(Shared::Types::ErrorCode::CONFIG_PARSE_ERROR,
                                                     "配置未加载,请先调用LoadConfiguration()"));
    }

    LogConfig config;

    // 从 [Debug] 段读取日志配置
    // log_level: 0=DEBUG, 1=INFO, 2=WARNING, 3=ERROR, 4=CRITICAL
    auto log_level_result = ReadIntValue("Debug", "log_level");
    if (log_level_result.IsError()) {
        return Result<LogConfig>(log_level_result.GetError());
    }
    int32 log_level_int = log_level_result.Value();

    // 将整数转换为 LogLevel 枚举
    switch (log_level_int) {
        case 0:
            config.min_level = LogLevel::DEBUG;
            break;
        case 1:
            config.min_level = LogLevel::INFO;
            break;
        case 2:
            config.min_level = LogLevel::WARNING;
            break;
        case 3:
            config.min_level = LogLevel::ERR;
            break;
        case 4:
            config.min_level = LogLevel::CRITICAL;
            break;
        default:
            return Result<LogConfig>(Error(ErrorCode::INVALID_CONFIG_VALUE,
                                           "log_level 超出有效范围: " + std::to_string(log_level_int)));
    }

    // 读取日志文件路径
    auto log_file_result = ReadIniValue("Debug", "log_file");
    if (log_file_result.IsError()) {
        return Result<LogConfig>(log_file_result.GetError());
    }
    config.log_file_path = log_file_result.Value();

    // 读取最大日志文件大小 (MB)
    auto max_size_result = ReadIntValue("Debug", "max_log_size");
    if (max_size_result.IsError()) {
        return Result<LogConfig>(max_size_result.GetError());
    }
    config.max_file_size_mb = static_cast<size_t>(max_size_result.Value());

    // 读取最大备份文件数量
    auto max_files_result = ReadIntValue("Debug", "max_log_files");
    if (max_files_result.IsError()) {
        return Result<LogConfig>(max_files_result.GetError());
    }
    config.max_backup_files = static_cast<size_t>(max_files_result.Value());

    // 读取启用标志
    auto enable_file_result = ReadBoolValue("Debug", "enable_file_log");
    if (enable_file_result.IsError()) {
        return Result<LogConfig>(enable_file_result.GetError());
    }
    config.enable_file = enable_file_result.Value();

    auto enable_console_result = ReadBoolValue("Debug", "enable_console_log");
    if (enable_console_result.IsError()) {
        return Result<LogConfig>(enable_console_result.GetError());
    }
    config.enable_console = enable_console_result.Value();

    // 时间戳和线程ID默认启用
    config.enable_timestamp = true;
    config.enable_thread_id = false;

    SILIGEN_LOG_INFO(std::string("日志配置加载成功: ") +
                     "level=" + LogLevelToString(config.min_level) + ", " +
                     "file=" + (config.enable_file ? std::string("启用") : std::string("禁用")) + ", " +
                     "path=" + config.log_file_path);

    return Result<LogConfig>::Success(config);
}

Result<Shared::Types::VelocityTraceConfig> ConfigFileAdapter::GetVelocityTraceConfig() const {
    if (!config_loaded_) {
        return Result<Shared::Types::VelocityTraceConfig>(
            Shared::Types::Error(Shared::Types::ErrorCode::CONFIG_PARSE_ERROR,
                                 "配置未加载,请先调用LoadConfiguration()"));
    }

    Shared::Types::VelocityTraceConfig config;

    auto enabled_result = ReadBoolValue("VelocityTrace", "enabled");
    if (enabled_result.IsError()) {
        return Result<Shared::Types::VelocityTraceConfig>(enabled_result.GetError());
    }
    config.enabled = enabled_result.Value();

    auto interval_result = ReadIntValue("VelocityTrace", "interval_ms");
    if (interval_result.IsError()) {
        return Result<Shared::Types::VelocityTraceConfig>(interval_result.GetError());
    }
    config.interval_ms = interval_result.Value();

    auto path_result = ReadIniValue("VelocityTrace", "output_path");
    if (path_result.IsError()) {
        return Result<Shared::Types::VelocityTraceConfig>(path_result.GetError());
    }
    config.output_path = path_result.Value();

    if (!config.Validate()) {
        return Result<Shared::Types::VelocityTraceConfig>::Failure(
            Error(ErrorCode::CONFIGURATION_ERROR,
                  "VelocityTraceConfig validation failed: " + config.GetValidationError()));
    }

    SILIGEN_LOG_INFO(std::string("速度采样配置加载成功: ") +
                     "enabled=" + std::string(config.enabled ? "true" : "false") +
                     ", interval=" + std::to_string(config.interval_ms) +
                     "ms, path=" + config.output_path);

    return Result<Shared::Types::VelocityTraceConfig>::Success(config);
}

Result<Shared::Types::DiagnosticsConfig> ConfigFileAdapter::GetDiagnosticsConfig() const {
    if (!config_loaded_) {
        return Result<Shared::Types::DiagnosticsConfig>(
            Shared::Types::Error(Shared::Types::ErrorCode::CONFIG_PARSE_ERROR,
                                 "配置未加载,请先调用LoadConfiguration()"));
    }

    Shared::Types::DiagnosticsConfig config;

    auto load_result = LoadIniCache();
    if (load_result.IsError()) {
        return Result<Shared::Types::DiagnosticsConfig>(load_result.GetError());
    }

    auto read_optional_raw = [this](const std::string& sec, const std::string& key,
                                    std::string& out, bool& found) -> Result<void> {
        found = false;
        const std::string section_key = StringManipulator::ToLower(sec);
        const std::string key_name = StringManipulator::ToLower(key);
        auto section_it = ini_cache_.find(section_key);
        if (section_it == ini_cache_.end()) {
            return Result<void>();
        }
        auto key_it = section_it->second.find(key_name);
        if (key_it == section_it->second.end()) {
            return Result<void>();
        }
        std::string raw = StringManipulator::Trim(key_it->second);
        if (raw.empty()) {
            return Result<void>();
        }
        out = raw;
        found = true;
        return Result<void>();
    };

    auto parse_bool = [](const std::string& sec, const std::string& key,
                         const std::string& raw, bool& out) -> Result<void> {
        std::string value = StringManipulator::ToLower(raw);
        if (value == "true" || value == "1" || value == "yes") {
            out = true;
            return Result<void>();
        }
        if (value == "false" || value == "0" || value == "no") {
            out = false;
            return Result<void>();
        }
        return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                  "配置项格式错误: [" + sec + "] " + key + "=" + raw));
    };

    auto parse_int = [](const std::string& sec, const std::string& key,
                        const std::string& raw, int32& out) -> Result<void> {
        try {
            size_t idx = 0;
            int32 value = std::stoi(raw, &idx);
            if (idx != raw.size()) {
                return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                          "配置项格式错误: [" + sec + "] " + key + "=" + raw));
            }
            out = value;
            return Result<void>();
        } catch (...) {
            return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                      "配置项格式错误: [" + sec + "] " + key + "=" + raw));
        }
    };

    const std::string section = "Diagnostics";
    bool found = false;
    std::string raw;
    Result<void> result;

    result = read_optional_raw(section, "deep_tcp_logging", raw, found);
    if (result.IsError()) return Result<Shared::Types::DiagnosticsConfig>(result.GetError());
    if (found) {
        result = parse_bool(section, "deep_tcp_logging", raw, config.deep_tcp_logging);
        if (result.IsError()) return Result<Shared::Types::DiagnosticsConfig>(result.GetError());
    }

    result = read_optional_raw(section, "deep_motion_logging", raw, found);
    if (result.IsError()) return Result<Shared::Types::DiagnosticsConfig>(result.GetError());
    if (found) {
        result = parse_bool(section, "deep_motion_logging", raw, config.deep_motion_logging);
        if (result.IsError()) return Result<Shared::Types::DiagnosticsConfig>(result.GetError());
    }

    result = read_optional_raw(section, "deep_hardware_logging", raw, found);
    if (result.IsError()) return Result<Shared::Types::DiagnosticsConfig>(result.GetError());
    if (found) {
        result = parse_bool(section, "deep_hardware_logging", raw, config.deep_hardware_logging);
        if (result.IsError()) return Result<Shared::Types::DiagnosticsConfig>(result.GetError());
    }

    result = read_optional_raw(section, "snapshot_delay_ms", raw, found);
    if (result.IsError()) return Result<Shared::Types::DiagnosticsConfig>(result.GetError());
    if (found) {
        result = parse_int(section, "snapshot_delay_ms", raw, config.snapshot_delay_ms);
        if (result.IsError()) return Result<Shared::Types::DiagnosticsConfig>(result.GetError());
    }

    result = read_optional_raw(section, "snapshot_after_stop_ms", raw, found);
    if (result.IsError()) return Result<Shared::Types::DiagnosticsConfig>(result.GetError());
    if (found) {
        result = parse_int(section, "snapshot_after_stop_ms", raw, config.snapshot_after_stop_ms);
        if (result.IsError()) return Result<Shared::Types::DiagnosticsConfig>(result.GetError());
    }

    result = read_optional_raw(section, "tcp_payload_max_chars", raw, found);
    if (result.IsError()) return Result<Shared::Types::DiagnosticsConfig>(result.GetError());
    if (found) {
        result = parse_int(section, "tcp_payload_max_chars", raw, config.tcp_payload_max_chars);
        if (result.IsError()) return Result<Shared::Types::DiagnosticsConfig>(result.GetError());
    }

    if (!config.Validate()) {
        return Result<Shared::Types::DiagnosticsConfig>::Failure(
            Error(ErrorCode::CONFIGURATION_ERROR,
                  "DiagnosticsConfig validation failed"));
    }

    SILIGEN_LOG_INFO(std::string("诊断配置加载成功: ") + config.ToString());

    return Result<Shared::Types::DiagnosticsConfig>::Success(config);
}

Result<DxfPreprocessConfig> ConfigFileAdapter::GetDxfPreprocessConfig() const {
    DxfPreprocessConfig config;
    auto load_result = LoadIniCache();
    if (load_result.IsError()) {
        return Result<DxfPreprocessConfig>(load_result.GetError());
    }

    auto read_optional_raw = [this](const std::string& sec, const std::string& key,
                                    std::string& out, bool& found) -> Result<void> {
        found = false;
        const std::string section_key = StringManipulator::ToLower(sec);
        const std::string key_name = StringManipulator::ToLower(key);
        auto section_it = ini_cache_.find(section_key);
        if (section_it == ini_cache_.end()) {
            return Result<void>();
        }
        auto key_it = section_it->second.find(key_name);
        if (key_it == section_it->second.end()) {
            return Result<void>();
        }
        std::string raw = StringManipulator::Trim(key_it->second);
        if (raw.empty()) {
            return Result<void>();
        }
        out = raw;
        found = true;
        return Result<void>();
    };

    auto parse_bool = [](const std::string& sec, const std::string& key,
                         const std::string& raw, bool& out) -> Result<void> {
        std::string value = StringManipulator::ToLower(raw);
        if (value == "true" || value == "1" || value == "yes") {
            out = true;
            return Result<void>();
        }
        if (value == "false" || value == "0" || value == "no") {
            out = false;
            return Result<void>();
        }
        return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                  "配置项格式错误: [" + sec + "] " + key + "=" + raw));
    };

    auto parse_int = [](const std::string& sec, const std::string& key,
                        const std::string& raw, int32& out) -> Result<void> {
        try {
            size_t idx = 0;
            int32 value = std::stoi(raw, &idx);
            if (idx != raw.size()) {
                return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                          "配置项格式错误: [" + sec + "] " + key + "=" + raw));
            }
            out = value;
            return Result<void>();
        } catch (...) {
            return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                      "配置项格式错误: [" + sec + "] " + key + "=" + raw));
        }
    };

    auto parse_float = [](const std::string& sec, const std::string& key,
                          const std::string& raw, float32& out) -> Result<void> {
        try {
            size_t idx = 0;
            float32 value = std::stof(raw, &idx);
            if (idx != raw.size()) {
                return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                          "配置项格式错误: [" + sec + "] " + key + "=" + raw));
            }
            out = value;
            return Result<void>();
        } catch (...) {
            return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                      "配置项格式错误: [" + sec + "] " + key + "=" + raw));
        }
    };

    const std::string section = "DXFPreprocess";
    bool found = false;
    std::string raw;
    Result<void> result;

    result = read_optional_raw(section, "normalize_units", raw, found);
    if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    if (found) {
        result = parse_bool(section, "normalize_units", raw, config.normalize_units);
        if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    }

    result = read_optional_raw(section, "approx_splines", raw, found);
    if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    if (found) {
        result = parse_bool(section, "approx_splines", raw, config.approx_splines);
        if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    }

    result = read_optional_raw(section, "snap_enabled", raw, found);
    if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    if (found) {
        result = parse_bool(section, "snap_enabled", raw, config.snap_enabled);
        if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    }

    result = read_optional_raw(section, "densify_enabled", raw, found);
    if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    if (found) {
        result = parse_bool(section, "densify_enabled", raw, config.densify_enabled);
        if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    }

    result = read_optional_raw(section, "min_seg_enabled", raw, found);
    if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    if (found) {
        result = parse_bool(section, "min_seg_enabled", raw, config.min_seg_enabled);
        if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    }

    result = read_optional_raw(section, "spline_samples", raw, found);
    if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    if (found) {
        result = parse_int(section, "spline_samples", raw, config.spline_samples);
        if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    }

    result = read_optional_raw(section, "spline_max_step", raw, found);
    if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    if (found) {
        result = parse_float(section, "spline_max_step", raw, config.spline_max_step);
        if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    }

    result = read_optional_raw(section, "chordal", raw, found);
    if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    if (found) {
        result = parse_float(section, "chordal", raw, config.chordal);
        if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    }

    result = read_optional_raw(section, "max_seg", raw, found);
    if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    if (found) {
        result = parse_float(section, "max_seg", raw, config.max_seg);
        if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    }

    result = read_optional_raw(section, "snap", raw, found);
    if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    if (found) {
        result = parse_float(section, "snap", raw, config.snap);
        if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    }

    result = read_optional_raw(section, "angular", raw, found);
    if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    if (found) {
        result = parse_float(section, "angular", raw, config.angular);
        if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    }

    result = read_optional_raw(section, "min_seg", raw, found);
    if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    if (found) {
        result = parse_float(section, "min_seg", raw, config.min_seg);
        if (result.IsError()) return Result<DxfPreprocessConfig>(result.GetError());
    }

    return Result<DxfPreprocessConfig>::Success(config);
}

Result<DxfTrajectoryConfig> ConfigFileAdapter::GetDxfTrajectoryConfig() const {
    DxfTrajectoryConfig config;
    auto load_result = LoadIniCache();
    if (load_result.IsError()) {
        return Result<DxfTrajectoryConfig>(load_result.GetError());
    }

    auto read_optional_raw = [this](const std::string& sec, const std::string& key,
                                    std::string& out, bool& found) -> Result<void> {
        found = false;
        const std::string section_key = StringManipulator::ToLower(sec);
        const std::string key_name = StringManipulator::ToLower(key);
        auto section_it = ini_cache_.find(section_key);
        if (section_it == ini_cache_.end()) {
            return Result<void>();
        }
        auto key_it = section_it->second.find(key_name);
        if (key_it == section_it->second.end()) {
            return Result<void>();
        }
        std::string raw = StringManipulator::Trim(key_it->second);
        if (raw.empty()) {
            return Result<void>();
        }
        out = raw;
        found = true;
        return Result<void>();
    };

    const std::string section = "DXFTrajectory";
    bool found = false;
    std::string raw;
    Result<void> result;

    result = read_optional_raw(section, "python", raw, found);
    if (result.IsError()) return Result<DxfTrajectoryConfig>(result.GetError());
    if (found) {
        config.python = raw;
    }

    result = read_optional_raw(section, "script", raw, found);
    if (result.IsError()) return Result<DxfTrajectoryConfig>(result.GetError());
    if (found) {
        config.script = raw;
    }

    return Result<DxfTrajectoryConfig>::Success(config);
}

Result<Shared::Types::ValveCoordinationConfig> ConfigFileAdapter::GetValveCoordinationConfig() const {
    Shared::Types::ValveCoordinationConfig config;
    auto load_result = LoadValveCoordinationSection(config);
    if (load_result.IsError()) {
        return Result<Shared::Types::ValveCoordinationConfig>(load_result.GetError());
    }

    if (!config.Validate()) {
        return Result<Shared::Types::ValveCoordinationConfig>::Failure(
            Error(ErrorCode::CONFIGURATION_ERROR, "ValveCoordinationConfig validation failed: " + config.GetValidationError()));
    }

    return Result<Shared::Types::ValveCoordinationConfig>::Success(config);
}

Result<void> ConfigFileAdapter::LoadValveCoordinationSection(Shared::Types::ValveCoordinationConfig& config) const {
    auto assign_int = [this](const std::string& sec, const std::string& key, int32& out) -> Result<void> {
        auto result = ReadIntValue(sec, key);
        if (result.IsError()) {
            return Result<void>(result.GetError());
        }
        out = result.Value();
        return Result<void>();
    };
    auto assign_float = [this](const std::string& sec, const std::string& key, float32& out) -> Result<void> {
        auto result = ReadFloatValue(sec, key);
        if (result.IsError()) {
            return Result<void>(result.GetError());
        }
        out = result.Value();
        return Result<void>();
    };

    auto result = assign_int("ValveCoordination", "DispensingIntervalMs", config.dispensing_interval_ms);
    if (result.IsError()) return result;
    result = assign_int("ValveCoordination", "DispensingDurationMs", config.dispensing_duration_ms);
    if (result.IsError()) return result;
    result = assign_int("ValveCoordination", "ValveOpenDelayMs", config.valve_open_delay_ms);
    if (result.IsError()) return result;
    result = assign_int("ValveCoordination", "ValveResponseMs", config.valve_response_ms);
    if (result.IsError()) return result;
    result = assign_int("ValveCoordination", "SafetyTimeoutMs", config.safety_timeout_ms);
    if (result.IsError()) return result;
    result = assign_int("ValveCoordination", "VisualMarginMs", config.visual_margin_ms);
    if (result.IsError()) return result;
    result = assign_int("ValveCoordination", "MinIntervalMs", config.min_interval_ms);
    if (result.IsError()) return result;
    result = assign_int("ValveCoordination", "PrebufferPoints", config.prebuffer_points);
    if (result.IsError()) return result;
    result = assign_int("ValveCoordination", "EnabledValves", config.enabled_valves);
    if (result.IsError()) return result;
    result = assign_float("ValveCoordination", "ArcSegmentationMaxDegree", config.arc_segmentation_max_degree);
    if (result.IsError()) return result;
    result = assign_float("ValveCoordination", "ArcChordToleranceMm", config.arc_chord_tolerance_mm);
    if (result.IsError()) return result;

    SILIGEN_LOG_INFO(std::string("阀门协调配置加载成功: ") +
                     "interval=" + std::to_string(config.dispensing_interval_ms) + "ms, " +
                     "duration=" + std::to_string(config.dispensing_duration_ms) + "ms, " +
                     "response=" + std::to_string(config.valve_response_ms) + "ms, " +
                     "margin=" + std::to_string(config.visual_margin_ms) + "ms, " +
                     "min_interval=" + std::to_string(config.min_interval_ms) + "ms, " +
                     "prebuffer=" + std::to_string(config.prebuffer_points) + ", " +
                     "valves=" + std::to_string(config.enabled_valves) + ", " +
                     "arc_max_degree=" + std::to_string(config.arc_segmentation_max_degree) + ", " +
                     "arc_tolerance=" + std::to_string(config.arc_chord_tolerance_mm) + "mm");
    return Result<void>();
}


}  // namespace Siligen::Infrastructure::Adapters

