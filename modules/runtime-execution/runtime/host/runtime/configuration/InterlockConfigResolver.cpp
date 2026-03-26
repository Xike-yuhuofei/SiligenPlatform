#include "runtime/configuration/InterlockConfigResolver.h"

#include "siligen/shared/strings/string_manipulator.h"

#include <fstream>
#include <optional>
#include <sstream>
#include <unordered_map>

namespace Siligen::Infrastructure::Configuration {

namespace {

using StringManipulator = Siligen::SharedKernel::StringManipulator;
using IniSectionMap = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;
constexpr int kGeneralPurposeInputMin = 0;
constexpr int kGeneralPurposeInputMax = 15;

std::string NormalizeToken(const std::string& value) {
    return StringManipulator::ToLower(StringManipulator::Trim(value));
}

InterlockConfig BuildDefaultConfig() {
    InterlockConfig config{};
    config.enabled = false;
    config.emergency_stop_input = 0;
    config.emergency_stop_active_low = true;
    config.safety_door_input = 1;
    config.pressure_sensor_input = 2;
    config.temperature_sensor_input = 3;
    config.voltage_sensor_input = 4;
    config.poll_interval_ms = 50;
    config.pressure_critical_low = 0.4f;
    config.pressure_warning_low = 0.5f;
    config.pressure_warning_high = 0.7f;
    config.pressure_critical_high = 0.8f;
    config.temperature_critical_low = 5.0f;
    config.temperature_normal_low = 10.0f;
    config.temperature_normal_high = 50.0f;
    config.temperature_critical_high = 55.0f;
    config.voltage_min = 220.0f;
    config.voltage_max = 240.0f;
    config.self_test_interval_hours = 24;
    return config;
}

std::optional<IniSectionMap> LoadIniSnapshot(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        return std::nullopt;
    }

    IniSectionMap snapshot;
    std::string line;
    std::string current_section;
    while (std::getline(file, line)) {
        std::string trimmed = StringManipulator::Trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            current_section = NormalizeToken(trimmed.substr(1, trimmed.size() - 2));
            if (!current_section.empty()) {
                snapshot[current_section];
            }
            continue;
        }

        const auto pos = trimmed.find('=');
        if (pos == std::string::npos || current_section.empty()) {
            continue;
        }

        const auto key = NormalizeToken(trimmed.substr(0, pos));
        const auto value = StringManipulator::Trim(trimmed.substr(pos + 1));
        snapshot[current_section][key] = value;
    }
    return snapshot;
}

bool HasSection(const IniSectionMap& snapshot, const std::string& section) {
    return snapshot.find(NormalizeToken(section)) != snapshot.end();
}

std::optional<std::string> GetIniValue(const IniSectionMap& snapshot, const std::string& section, const std::string& key) {
    const auto section_it = snapshot.find(NormalizeToken(section));
    if (section_it == snapshot.end()) {
        return std::nullopt;
    }
    const auto key_it = section_it->second.find(NormalizeToken(key));
    if (key_it == section_it->second.end()) {
        return std::nullopt;
    }
    return key_it->second;
}

std::optional<bool> ParseBoolOption(const IniSectionMap& snapshot, const std::string& section, const std::string& key) {
    const auto value = GetIniValue(snapshot, section, key);
    if (!value.has_value()) {
        return std::nullopt;
    }

    const auto normalized = NormalizeToken(value.value());
    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off") {
        return false;
    }
    return std::nullopt;
}

template <typename TValue>
std::optional<TValue> ParseNumericOption(const IniSectionMap& snapshot, const std::string& section, const std::string& key) {
    const auto value = GetIniValue(snapshot, section, key);
    if (!value.has_value()) {
        return std::nullopt;
    }

    std::istringstream iss(value.value());
    TValue parsed{};
    iss >> parsed;
    if (iss.fail()) {
        return std::nullopt;
    }
    return parsed;
}

std::optional<int> ParseRangedIntOption(
    const IniSectionMap& snapshot,
    const std::string& section,
    const std::string& key,
    int min_value,
    int max_value) {
    const auto value = ParseNumericOption<int>(snapshot, section, key);
    if (!value.has_value() || value.value() < min_value || value.value() > max_value) {
        return std::nullopt;
    }
    return value;
}

std::optional<int> ParseInputIndexOption(
    const IniSectionMap& snapshot,
    const std::string& section,
    const std::string& key,
    int disabled_value = -1) {
    const auto value = ParseNumericOption<int>(snapshot, section, key);
    if (!value.has_value()) {
        return std::nullopt;
    }
    if (value.value() < disabled_value) {
        return std::nullopt;
    }
    return value;
}

void ApplyOptionalInterlockFields(InterlockConfig& config, const IniSectionMap& snapshot) {
    config.pressure_sensor_input =
        static_cast<int16>(ParseNumericOption<int>(snapshot, "Interlock", "pressure_sensor_input").value_or(config.pressure_sensor_input));
    config.emergency_stop_active_low =
        ParseBoolOption(snapshot, "Interlock", "emergency_stop_active_low").value_or(config.emergency_stop_active_low);
    config.temperature_sensor_input =
        static_cast<int16>(ParseNumericOption<int>(snapshot, "Interlock", "temperature_sensor_input").value_or(config.temperature_sensor_input));
    config.voltage_sensor_input =
        static_cast<int16>(ParseNumericOption<int>(snapshot, "Interlock", "voltage_sensor_input").value_or(config.voltage_sensor_input));
    config.poll_interval_ms =
        ParseNumericOption<int>(snapshot, "Interlock", "poll_interval_ms").value_or(config.poll_interval_ms);
    config.pressure_critical_low =
        ParseNumericOption<float>(snapshot, "Interlock", "pressure_critical_low_mpa").value_or(config.pressure_critical_low);
    config.pressure_warning_low =
        ParseNumericOption<float>(snapshot, "Interlock", "pressure_warning_low_mpa").value_or(config.pressure_warning_low);
    config.pressure_warning_high =
        ParseNumericOption<float>(snapshot, "Interlock", "pressure_warning_high_mpa").value_or(config.pressure_warning_high);
    config.pressure_critical_high =
        ParseNumericOption<float>(snapshot, "Interlock", "pressure_critical_high_mpa").value_or(config.pressure_critical_high);
    config.temperature_critical_low =
        ParseNumericOption<float>(snapshot, "Interlock", "temperature_critical_low_celsius").value_or(config.temperature_critical_low);
    config.temperature_normal_low =
        ParseNumericOption<float>(snapshot, "Interlock", "temperature_normal_low_celsius").value_or(config.temperature_normal_low);
    config.temperature_normal_high =
        ParseNumericOption<float>(snapshot, "Interlock", "temperature_normal_high_celsius").value_or(config.temperature_normal_high);
    config.temperature_critical_high =
        ParseNumericOption<float>(snapshot, "Interlock", "temperature_critical_high_celsius").value_or(config.temperature_critical_high);
    config.voltage_min =
        ParseNumericOption<float>(snapshot, "Interlock", "voltage_min_v").value_or(config.voltage_min);
    config.voltage_max =
        ParseNumericOption<float>(snapshot, "Interlock", "voltage_max_v").value_or(config.voltage_max);
    config.self_test_interval_hours =
        ParseNumericOption<int>(snapshot, "Interlock", "self_test_interval_hours").value_or(config.self_test_interval_hours);
}

}  // namespace

InterlockConfigResolution ResolveInterlockConfigFromIni(const std::string& config_path) {
    InterlockConfigResolution resolution;
    resolution.config = BuildDefaultConfig();

    const auto snapshot = LoadIniSnapshot(config_path);
    if (!snapshot.has_value()) {
        resolution.warnings.emplace_back(
            "Interlock configuration file could not be loaded; using built-in defaults with interlock disabled");
        return resolution;
    }

    const auto safety_enabled = ParseBoolOption(snapshot.value(), "Safety", "emergency_stop_enabled").value_or(false);
    resolution.explicit_interlock_section = HasSection(snapshot.value(), "Interlock");

    if (!resolution.explicit_interlock_section) {
        resolution.used_compat_fallback = true;
        resolution.config.enabled = safety_enabled;
        resolution.warnings.emplace_back(
            "Interlock section missing; using compatibility fallback from [Safety].emergency_stop_enabled with default emergency_stop_input=0, emergency_stop_active_low=true and safety_door_input=1");
        return resolution;
    }

    const auto enabled = ParseBoolOption(snapshot.value(), "Interlock", "enabled");
    const auto emergency_stop_input = ParseRangedIntOption(
        snapshot.value(),
        "Interlock",
        "emergency_stop_input",
        kGeneralPurposeInputMin,
        kGeneralPurposeInputMax);
    const auto safety_door_input = ParseInputIndexOption(snapshot.value(), "Interlock", "safety_door_input");

    if (!enabled.has_value() || !emergency_stop_input.has_value() || !safety_door_input.has_value()) {
        resolution.used_compat_fallback = true;
        resolution.config.enabled = safety_enabled;
        resolution.warnings.emplace_back(
            "Interlock section is incomplete or invalid; required keys are enabled, emergency_stop_input(range: 0-15), safety_door_input (use -1 to disable optional door input). Optional key emergency_stop_active_low defaults to true. Falling back to compatibility defaults");
        return resolution;
    }

    resolution.config.enabled = enabled.value();
    resolution.config.emergency_stop_input = static_cast<int16>(emergency_stop_input.value());
    resolution.config.safety_door_input = static_cast<int16>(safety_door_input.value());
    ApplyOptionalInterlockFields(resolution.config, snapshot.value());
    return resolution;
}

}  // namespace Siligen::Infrastructure::Configuration
