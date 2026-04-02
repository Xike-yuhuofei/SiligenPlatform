#pragma once

#include "process_planning/contracts/configuration/IConfigurationPort.h"

#include <memory>
#include <utility>

namespace Siligen::Domain::Configuration::Services {

using Ports::HomingConfig;
using Ports::IConfigurationPort;
using Ports::MachineConfig;
using Shared::Types::Error;
using Shared::Types::ErrorCode;
using Shared::Types::LogicalAxisId;
using Shared::Types::Result;
using Shared::Types::float32;

struct ReadyZeroSpeedResolution {
    float32 speed_mm_s = 0.0f;
    bool used_fallback = false;
};

inline Result<ReadyZeroSpeedResolution> ResolveReadyZeroSpeed(
    const HomingConfig& homing_config,
    const char* error_source = "ReadyZeroSpeedResolver") {
    ReadyZeroSpeedResolution resolution;
    resolution.speed_mm_s = homing_config.ready_zero_speed_mm_s;
    if (resolution.speed_mm_s <= 0.0f) {
        resolution.speed_mm_s = homing_config.locate_velocity;
        resolution.used_fallback = true;
    }
    if (resolution.speed_mm_s <= 0.0f) {
        return Result<ReadyZeroSpeedResolution>::Failure(
            Error(ErrorCode::INVALID_CONFIG_VALUE,
                  "ready_zero_speed_mm_s must be positive or locate_velocity must remain available for fallback",
                  error_source));
    }
    return Result<ReadyZeroSpeedResolution>::Success(resolution);
}

inline Result<ReadyZeroSpeedResolution> ResolveReadyZeroSpeed(
    const HomingConfig& homing_config,
    const MachineConfig& machine_config,
    const char* error_source = "ReadyZeroSpeedResolver") {
    auto resolution_result = ResolveReadyZeroSpeed(homing_config, error_source);
    if (resolution_result.IsError()) {
        return resolution_result;
    }

    const auto& resolution = resolution_result.Value();
    if (machine_config.max_speed > 0.0f && resolution.speed_mm_s > machine_config.max_speed) {
        return Result<ReadyZeroSpeedResolution>::Failure(
            Error(ErrorCode::INVALID_CONFIG_VALUE,
                  "ready-zero speed exceeds machine.max_speed",
                  error_source));
    }

    return resolution_result;
}

inline Result<ReadyZeroSpeedResolution> ResolveReadyZeroSpeed(
    LogicalAxisId axis,
    const std::shared_ptr<IConfigurationPort>& config_port,
    const char* error_source = "ReadyZeroSpeedResolver") {
    if (!config_port) {
        return Result<ReadyZeroSpeedResolution>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Config port not available", error_source));
    }

    auto homing_result = config_port->GetHomingConfig(static_cast<int>(axis));
    if (homing_result.IsError()) {
        return Result<ReadyZeroSpeedResolution>::Failure(homing_result.GetError());
    }

    auto machine_result = config_port->GetMachineConfig();
    if (machine_result.IsError()) {
        return Result<ReadyZeroSpeedResolution>::Failure(machine_result.GetError());
    }

    return ResolveReadyZeroSpeed(homing_result.Value(), machine_result.Value(), error_source);
}

inline Result<HomingConfig> ApplyUnifiedReadyZeroSpeed(
    const HomingConfig& homing_config,
    const char* error_source = "ReadyZeroSpeedResolver") {
    auto resolution_result = ResolveReadyZeroSpeed(homing_config, error_source);
    if (resolution_result.IsError()) {
        return Result<HomingConfig>::Failure(resolution_result.GetError());
    }

    HomingConfig effective_config = homing_config;
    const float32 speed_mm_s = resolution_result.Value().speed_mm_s;
    effective_config.ready_zero_speed_mm_s = speed_mm_s;
    effective_config.rapid_velocity = speed_mm_s;
    effective_config.locate_velocity = speed_mm_s;
    effective_config.index_velocity = speed_mm_s;
    effective_config.escape_velocity = speed_mm_s;
    return Result<HomingConfig>::Success(std::move(effective_config));
}

}  // namespace Siligen::Domain::Configuration::Services
