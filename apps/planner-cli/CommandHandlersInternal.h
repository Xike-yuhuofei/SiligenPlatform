#pragma once

#include "domain/dispensing/ports/IValvePort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "runtime/configuration/WorkspaceAssetPaths.h"
#include "shared/Strings/StringManipulator.h"
#include "shared/types/AxisTypes.h"
#include "shared/types/Error.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Siligen::StringManipulator;

std::string ResolveCliConfigPath(const std::string& preferred_path = "") {
    const auto requested_path = preferred_path.empty()
        ? Siligen::Infrastructure::Configuration::CanonicalMachineConfigRelativePath()
        : preferred_path;
    const auto resolution =
        Siligen::Infrastructure::Configuration::ResolveConfigFilePathWithBridge(requested_path);
    if (resolution.fail_fast) {
        throw std::runtime_error(
            "CLI 配置路径解析失败: requested='" + requested_path +
            "', mode=" +
            Siligen::Infrastructure::Configuration::ConfigPathBridgeModeName(resolution.mode) +
            ", detail=" + resolution.detail);
    }
    return resolution.resolved_path;
}

void ClearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

bool ReadIniValue(
    const std::string& config_path,
    const std::string& section,
    const std::string& key,
    std::string& out_value) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        return false;
    }

    const std::string section_key = StringManipulator::ToLower(section);
    const std::string key_name = StringManipulator::ToLower(key);
    std::string current_section;
    std::string line;

    while (std::getline(file, line)) {
        std::string trimmed = StringManipulator::Trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            current_section = StringManipulator::ToLower(
                StringManipulator::Trim(trimmed.substr(1, trimmed.size() - 2)));
            continue;
        }

        if (current_section != section_key) {
            continue;
        }

        const auto eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }

        std::string key_part = StringManipulator::ToLower(
            StringManipulator::Trim(trimmed.substr(0, eq_pos)));
        if (key_part != key_name) {
            continue;
        }

        std::string value = StringManipulator::StripInlineComment(trimmed.substr(eq_pos + 1));
        if (value.empty()) {
            return false;
        }
        out_value = value;
        return true;
    }

    return false;
}

bool ReadIniDoubleValue(
    const std::string& config_path,
    const std::string& section,
    const std::string& key,
    double& out_value) {
    std::string raw;
    if (!ReadIniValue(config_path, section, key, raw)) {
        return false;
    }
    try {
        size_t idx = 0;
        double value = std::stod(raw, &idx);
        if (idx != raw.size()) {
            return false;
        }
        out_value = value;
        return true;
    } catch (...) {
        return false;
    }
}

constexpr const char* kFallbackDxfFileRelativePath = "samples/dxf/rect_diag.dxf";
constexpr const char* kDefaultPreviewScriptRelativePath =
    "scripts/engineering-data/generate_preview.py";
constexpr const char* kDefaultPreviewOutputDirRelativePath = "tests/reports/dxf-preview";

bool ReadLine(std::string& out) {
    if (std::cin.eof()) {
        return false;
    }
    std::getline(std::cin, out);
    return !std::cin.fail();
}

bool ParseInt(const std::string& input, int& value) {
    std::istringstream iss(input);
    return (iss >> value) && iss.eof();
}

bool ParseDouble(const std::string& input, double& value) {
    std::istringstream iss(input);
    return (iss >> value) && iss.eof();
}

int PromptInt(const std::string& label, int default_value) {
    std::cout << label << " (默认 " << default_value << "): ";
    std::string input;
    if (!ReadLine(input)) {
        return default_value;
    }
    input = StringManipulator::Trim(input);
    if (input.empty()) {
        return default_value;
    }
    int value = default_value;
    if (!ParseInt(input, value)) {
        return default_value;
    }
    return value;
}

double PromptDouble(const std::string& label, double default_value) {
    std::cout << label << " (默认 " << default_value << "): ";
    std::string input;
    if (!ReadLine(input)) {
        return default_value;
    }
    input = StringManipulator::Trim(input);
    if (input.empty()) {
        return default_value;
    }
    double value = default_value;
    if (!ParseDouble(input, value)) {
        return default_value;
    }
    return value;
}

std::string PromptString(const std::string& label, const std::string& default_value) {
    std::cout << label << " (默认 " << default_value << "): ";
    std::string input;
    if (!ReadLine(input)) {
        return default_value;
    }
    input = StringManipulator::Trim(input);
    if (input.empty()) {
        return default_value;
    }
    return input;
}

Siligen::Shared::Types::Error BuildAxesNotHomedError(
    const std::vector<Siligen::Shared::Types::LogicalAxisId>& axes) {
    using Siligen::Shared::Types::Error;
    using Siligen::Shared::Types::ErrorCode;
    using Siligen::Shared::Types::ToUserDisplay;

    std::ostringstream message;
    message << "轴未回零: ";
    for (size_t i = 0; i < axes.size(); ++i) {
        if (i > 0) {
            message << ", ";
        }
        message << ToUserDisplay(axes[i]);
    }
    message << "。请先执行回零或使用 --auto-home";
    return Error(ErrorCode::AXIS_NOT_HOMED, message.str(), "CLI");
}

bool PromptYesNo(const std::string& label, bool default_value) {
    std::cout << label << " (" << (default_value ? "Y/n" : "y/N") << "): ";
    std::string input;
    if (!ReadLine(input)) {
        return default_value;
    }
    input = StringManipulator::Trim(input);
    if (input.empty()) {
        return default_value;
    }
    return (input == "y" || input == "Y");
}

std::string BuildPreviewOutputPath(const std::string& dxf_path) {
    try {
        std::filesystem::path path(dxf_path);
        auto stem = path.stem().string();
        if (stem.empty()) {
            stem = "dxf_preview";
        }
        const std::string file_name = stem + "_preview.html";
        if (path.has_parent_path()) {
            return (path.parent_path() / file_name).string();
        }
        return file_name;
    } catch (...) {
        return "dxf_preview.html";
    }
}

std::string BuildPreviewTitle(const std::string& dxf_path) {
    try {
        std::filesystem::path path(dxf_path);
        const auto name = path.filename().string();
        if (!name.empty()) {
            return "DXF轨迹预览 - " + name;
        }
    } catch (...) {
    }
    return "DXF轨迹预览";
}

const char* MotionStateToString(Siligen::Domain::Motion::Ports::MotionState state) {
    using Siligen::Domain::Motion::Ports::MotionState;
    switch (state) {
        case MotionState::IDLE:
            return "IDLE";
        case MotionState::MOVING:
            return "MOVING";
        case MotionState::STOPPED:
            return "STOPPED";
        case MotionState::FAULT:
            return "FAULT";
        case MotionState::HOMING:
            return "HOMING";
        case MotionState::HOMED:
            return "HOMED";
        case MotionState::ESTOP:
            return "ESTOP";
        case MotionState::DISABLED:
            return "DISABLED";
        default:
            return "UNKNOWN";
    }
}

const char* DispenserStatusToString(Siligen::Domain::Dispensing::Ports::DispenserValveStatus status) {
    using Siligen::Domain::Dispensing::Ports::DispenserValveStatus;
    switch (status) {
        case DispenserValveStatus::Idle:
            return "Idle";
        case DispenserValveStatus::Running:
            return "Running";
        case DispenserValveStatus::Paused:
            return "Paused";
        case DispenserValveStatus::Stopped:
            return "Stopped";
        case DispenserValveStatus::Error:
            return "Error";
        default:
            return "Unknown";
    }
}

const char* SupplyStatusToString(Siligen::Domain::Dispensing::Ports::SupplyValveState state) {
    using Siligen::Domain::Dispensing::Ports::SupplyValveState;
    switch (state) {
        case SupplyValveState::Open:
            return "Open";
        case SupplyValveState::Closed:
            return "Closed";
        case SupplyValveState::Error:
            return "Error";
        default:
            return "Unknown";
    }
}

}  // namespace


