// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "ConfigFile"

#include "ConfigFileAdapter.h"

#include <fstream>
#include <sstream>

#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

#include "shared/interfaces/ILoggingService.h"
#include "siligen/shared/strings/string_manipulator.h"

namespace Siligen::Infrastructure::Adapters {

using namespace Domain::Configuration::Ports;
using namespace Shared::Types;

namespace {
using StringManipulator = Siligen::SharedKernel::StringManipulator;
}

// === INI文件操作辅助方法 ===

Result<void> ConfigFileAdapter::LoadIniCache() const {
    if (ini_cache_loaded_) {
        return Result<void>();
    }

    if (config_file_path_.empty()) {
        return Result<void>(Error(ErrorCode::CONFIG_FILE_NOT_FOUND, "配置文件路径为空"));
    }

    std::ifstream file(config_file_path_);
    if (!file.is_open()) {
        return Result<void>(Error(ErrorCode::CONFIG_FILE_NOT_FOUND, "配置文件不存在: " + config_file_path_));
    }

    std::stringstream sanitized;
    std::string line;
    std::string current_section;
    size_t line_number = 0;

    while (std::getline(file, line)) {
        ++line_number;
        if (line_number == 1 && line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3);
        }

        std::string trimmed = StringManipulator::Trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            current_section = StringManipulator::Trim(trimmed.substr(1, trimmed.size() - 2));
            if (current_section.empty()) {
                return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                          "配置段名称为空, 行号: " + std::to_string(line_number)));
            }
            sanitized << "[" << current_section << "]\n";
            continue;
        }

        const auto eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) {
            return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                      "配置项缺少等号, 行号: " + std::to_string(line_number)));
        }

        if (current_section.empty()) {
            return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                      "配置项未归属任何段, 行号: " + std::to_string(line_number)));
        }

        std::string key = StringManipulator::Trim(trimmed.substr(0, eq_pos));
        std::string value = StringManipulator::StripInlineComment(trimmed.substr(eq_pos + 1));
        if (key.empty()) {
            return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                      "配置项键为空, 行号: " + std::to_string(line_number)));
        }

        sanitized << key << "=" << value << "\n";
    }

    ini_cache_.clear();

    try {
        boost::property_tree::ptree pt;
        boost::property_tree::ini_parser::read_ini(sanitized, pt);

        for (const auto& section : pt) {
            const std::string section_name = StringManipulator::Trim(section.first);
            if (section_name.empty()) {
                return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                          "配置段名称为空"));
            }
            for (const auto& kv : section.second) {
                const std::string key = StringManipulator::Trim(kv.first);
                if (key.empty()) {
                    return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                              "配置项键为空"));
                }
                std::string value = StringManipulator::Trim(kv.second.get_value<std::string>());
                ini_cache_[StringManipulator::ToLower(section_name)][StringManipulator::ToLower(key)] = value;
            }
        }
    } catch (const boost::property_tree::ini_parser::ini_parser_error& e) {
        return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                  std::string("配置文件解析失败: ") + e.what()));
    } catch (const std::exception& e) {
        return Result<void>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                  std::string("配置文件解析失败: ") + e.what()));
    }

    ini_cache_loaded_ = true;
    SILIGEN_LOG_INFO("ConfigFileAdapter: 仅从配置文件读取: " + config_file_path_);
    return Result<void>();
}

Result<std::string> ConfigFileAdapter::ReadIniValue(const std::string& section,
                                                    const std::string& key) const {
    auto load_result = LoadIniCache();
    if (load_result.IsError()) {
        return Result<std::string>(load_result.GetError());
    }

    const std::string section_key = StringManipulator::ToLower(section);
    const std::string key_name = StringManipulator::ToLower(key);

    auto section_it = ini_cache_.find(section_key);
    if (section_it == ini_cache_.end()) {
        return Result<std::string>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                         "缺少配置段: [" + section + "]"));
    }

    auto key_it = section_it->second.find(key_name);
    if (key_it == section_it->second.end()) {
        return Result<std::string>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                         "缺少配置项: [" + section + "] " + key));
    }

    std::string value = StringManipulator::Trim(key_it->second);
    if (value.empty()) {
        return Result<std::string>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                         "配置项为空: [" + section + "] " + key));
    }

    return Result<std::string>(value);
}

bool ConfigFileAdapter::WriteIniValue(const std::string& section,
                                      const std::string& key,
                                      const std::string& value) const {
#ifdef _WIN32
    if (config_file_path_.empty()) {
        return false;
    }
    return WritePrivateProfileStringA(section.c_str(),
                                      key.c_str(),
                                      value.c_str(),
                                      config_file_path_.c_str()) != 0;
#else
    (void)section;
    (void)key;
    (void)value;
    return false;
#endif
}

// === 类型转换辅助方法 ===

Result<float32> ConfigFileAdapter::ReadFloatValue(const std::string& section, const std::string& key) const {
    auto value_result = ReadIniValue(section, key);
    if (value_result.IsError()) {
        return Result<float32>(value_result.GetError());
    }

    const std::string& raw = value_result.Value();
    try {
        size_t idx = 0;
        float32 value = std::stof(raw, &idx);
        if (idx != raw.size()) {
            return Result<float32>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                         "配置项格式错误: [" + section + "] " + key + "=" + raw));
        }
        return Result<float32>(value);
    } catch (...) {
        return Result<float32>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                     "配置项格式错误: [" + section + "] " + key + "=" + raw));
    }
}

Result<int32> ConfigFileAdapter::ReadIntValue(const std::string& section, const std::string& key) const {
    auto value_result = ReadIniValue(section, key);
    if (value_result.IsError()) {
        return Result<int32>(value_result.GetError());
    }

    const std::string& raw = value_result.Value();
    try {
        size_t idx = 0;
        int32 value = std::stoi(raw, &idx);
        if (idx != raw.size()) {
            return Result<int32>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                       "配置项格式错误: [" + section + "] " + key + "=" + raw));
        }
        return Result<int32>(value);
    } catch (...) {
        return Result<int32>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                                   "配置项格式错误: [" + section + "] " + key + "=" + raw));
    }
}

Result<bool> ConfigFileAdapter::ReadBoolValue(const std::string& section, const std::string& key) const {
    auto value_result = ReadIniValue(section, key);
    if (value_result.IsError()) {
        return Result<bool>(value_result.GetError());
    }

    const std::string raw = StringManipulator::ToLower(value_result.Value());
    if (raw == "true" || raw == "1" || raw == "yes") {
        return Result<bool>(true);
    }
    if (raw == "false" || raw == "0" || raw == "no") {
        return Result<bool>(false);
    }

    return Result<bool>(Error(ErrorCode::CONFIG_PARSE_ERROR,
                              "配置项格式错误: [" + section + "] " + key + "=" + value_result.Value()));
}

Result<DispensingMode> ConfigFileAdapter::ReadDispensingModeValue(const std::string& section,
                                                                  const std::string& key) const {
    auto value_result = ReadIniValue(section, key);
    if (value_result.IsError()) {
        return Result<DispensingMode>(value_result.GetError());
    }

    std::string value = StringManipulator::ToLower(value_result.Value());
    if (value == "0" || value == "contact") {
        return Result<DispensingMode>(DispensingMode::CONTACT);
    }
    if (value == "1" || value == "non_contact") {
        return Result<DispensingMode>(DispensingMode::NON_CONTACT);
    }
    if (value == "2" || value == "jetting") {
        return Result<DispensingMode>(DispensingMode::JETTING);
    }
    if (value == "3" || value == "position_trigger") {
        return Result<DispensingMode>(DispensingMode::POSITION_TRIGGER);
    }

    return Result<DispensingMode>(Error(ErrorCode::INVALID_CONFIG_VALUE,
                                        "无效点胶模式: [" + section + "] " + key + "=" + value_result.Value()));
}

Result<Shared::Types::HardwareMode> ConfigFileAdapter::ReadHardwareModeValue(
    const std::string& section,
    const std::string& key) const {
    auto value_result = ReadIniValue(section, key);
    if (value_result.IsError()) {
        return Result<Shared::Types::HardwareMode>(value_result.GetError());
    }

    std::string value = StringManipulator::ToLower(value_result.Value());
    if (value == "real") {
        return Result<Shared::Types::HardwareMode>(Shared::Types::HardwareMode::Real);
    }
    if (value == "mock") {
        return Result<Shared::Types::HardwareMode>(Shared::Types::HardwareMode::Mock);
    }

    return Result<Shared::Types::HardwareMode>(Error(ErrorCode::INVALID_CONFIG_VALUE,
                                                     "无效硬件模式: [" + section + "] " + key + "=" + value_result.Value()));
}

std::string ConfigFileAdapter::FloatToString(float32 value) const {
    return std::to_string(value);
}

std::string ConfigFileAdapter::IntToString(int32 value) const {
    return std::to_string(value);
}

std::string ConfigFileAdapter::BoolToString(bool value) const {
    return value ? "true" : "false";
}


}  // namespace Siligen::Infrastructure::Adapters

