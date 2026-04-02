#pragma once

#include "dxf_geometry/application/services/dxf/DxfPbPreparationService.h"
#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "shared/types/Error.h"
#include "shared/types/HardwareConfiguration.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace Siligen::DxfGeometry::Tests::Support {

using Siligen::Application::Services::DXF::DxfPbPreparationService;
using Siligen::Domain::Configuration::Ports::DxfPreprocessConfig;
using Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig;
using Siligen::Domain::Configuration::Ports::DispensingConfig;
using Siligen::Domain::Configuration::Ports::HomingConfig;
using Siligen::Domain::Configuration::Ports::IConfigurationPort;
using Siligen::Domain::Configuration::Ports::MachineConfig;
using Siligen::Domain::Configuration::Ports::SystemConfig;
using Siligen::Domain::Configuration::Ports::ValveSupplyConfig;
using Siligen::Shared::Types::DispenserValveConfig;
using Siligen::Shared::Types::HardwareConfiguration;
using Siligen::Shared::Types::HardwareMode;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::ValveCoordinationConfig;
using Siligen::Shared::Types::VelocityTraceConfig;

inline void SetEnvVar(const std::string& name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

inline void UnsetEnvVar(const std::string& name) {
#ifdef _WIN32
    _putenv_s(name.c_str(), "");
#else
    unsetenv(name.c_str());
#endif
}

class ScopedEnvVar {
   public:
    ScopedEnvVar(std::string name, std::string value) : name_(std::move(name)) {
        if (const char* current = std::getenv(name_.c_str())) {
            original_value_ = std::string(current);
        }
        SetEnvVar(name_, value);
    }

    ScopedEnvVar(const ScopedEnvVar&) = delete;
    ScopedEnvVar& operator=(const ScopedEnvVar&) = delete;

    ~ScopedEnvVar() {
        if (original_value_.has_value()) {
            SetEnvVar(name_, *original_value_);
            return;
        }
        UnsetEnvVar(name_);
    }

   private:
    std::string name_;
    std::optional<std::string> original_value_;
};

class ScopedUnsetEnvVar {
   public:
    explicit ScopedUnsetEnvVar(std::string name) : name_(std::move(name)) {
        if (const char* current = std::getenv(name_.c_str())) {
            original_value_ = std::string(current);
        }
        UnsetEnvVar(name_);
    }

    ScopedUnsetEnvVar(const ScopedUnsetEnvVar&) = delete;
    ScopedUnsetEnvVar& operator=(const ScopedUnsetEnvVar&) = delete;

    ~ScopedUnsetEnvVar() {
        if (original_value_.has_value()) {
            SetEnvVar(name_, *original_value_);
        }
    }

   private:
    std::string name_;
    std::optional<std::string> original_value_;
};

class ScopedTempDir {
   public:
    explicit ScopedTempDir(std::string name) {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() / ("siligen_" + std::move(name) + "_" + std::to_string(now));
        std::filesystem::create_directories(path_);
    }

    ScopedTempDir(const ScopedTempDir&) = delete;
    ScopedTempDir& operator=(const ScopedTempDir&) = delete;

    ~ScopedTempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    [[nodiscard]] const std::filesystem::path& Path() const { return path_; }

   private:
    std::filesystem::path path_;
};

class ScopedCurrentPath {
   public:
    explicit ScopedCurrentPath(const std::filesystem::path& new_path) {
        std::error_code ec;
        original_path_ = std::filesystem::current_path(ec);
        if (ec) {
            throw std::runtime_error("failed to read current path");
        }
        std::filesystem::current_path(new_path, ec);
        if (ec) {
            throw std::runtime_error("failed to switch current path");
        }
    }

    ScopedCurrentPath(const ScopedCurrentPath&) = delete;
    ScopedCurrentPath& operator=(const ScopedCurrentPath&) = delete;

    ~ScopedCurrentPath() {
        std::error_code ec;
        std::filesystem::current_path(original_path_, ec);
    }

   private:
    std::filesystem::path original_path_;
};

inline std::string MinimalDxf() {
    return "0\nSECTION\n2\nENTITIES\n0\nLINE\n10\n0\n20\n0\n11\n10\n21\n0\n0\nENDSEC\n0\nEOF\n";
}

inline void WriteTextFile(const std::filesystem::path& path, const std::string& content) {
    std::error_code ec;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }
    std::ofstream out(path, std::ios::binary);
    out << content;
}

inline std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

inline std::filesystem::path PbPathFor(std::filesystem::path dxf_path) {
    dxf_path.replace_extension(".pb");
    return dxf_path;
}

inline std::filesystem::path ArgsPathFor(std::filesystem::path pb_path) {
    pb_path.replace_extension(".args.txt");
    return pb_path;
}

class FakeConfigurationPort final : public IConfigurationPort {
   public:
    DxfPreprocessConfig preprocess_config{};
    DxfTrajectoryConfig trajectory_config{};
    DispensingConfig dispensing_config{};
    MachineConfig machine_config{};
    HardwareConfiguration hardware_config{};
    std::vector<HomingConfig> homing_configs{};

    FakeConfigurationPort() {
        hardware_config.num_axes = 2;
        machine_config.max_speed = 80.0f;
        machine_config.max_acceleration = 200.0f;
    }

    Result<SystemConfig> LoadConfiguration() override {
        SystemConfig config;
        config.dispensing = dispensing_config;
        config.machine = machine_config;
        config.homing_configs = homing_configs;
        return Result<SystemConfig>::Success(config);
    }

    Result<void> SaveConfiguration(const SystemConfig&) override { return Result<void>::Success(); }
    Result<void> ReloadConfiguration() override { return Result<void>::Success(); }
    Result<DispensingConfig> GetDispensingConfig() const override { return Result<DispensingConfig>::Success(dispensing_config); }
    Result<void> SetDispensingConfig(const DispensingConfig& config) override {
        dispensing_config = config;
        return Result<void>::Success();
    }
    Result<DxfPreprocessConfig> GetDxfPreprocessConfig() const override {
        return Result<DxfPreprocessConfig>::Success(preprocess_config);
    }
    Result<DxfTrajectoryConfig> GetDxfTrajectoryConfig() const override {
        return Result<DxfTrajectoryConfig>::Success(trajectory_config);
    }
    Result<Siligen::Shared::Types::DiagnosticsConfig> GetDiagnosticsConfig() const override {
        return Result<Siligen::Shared::Types::DiagnosticsConfig>::Success({});
    }
    Result<MachineConfig> GetMachineConfig() const override { return Result<MachineConfig>::Success(machine_config); }
    Result<void> SetMachineConfig(const MachineConfig& config) override {
        machine_config = config;
        return Result<void>::Success();
    }
    Result<HomingConfig> GetHomingConfig(int axis) const override {
        if (axis >= 0 && axis < static_cast<int>(homing_configs.size())) {
            return Result<HomingConfig>::Success(homing_configs[static_cast<size_t>(axis)]);
        }
        HomingConfig config;
        config.axis = axis;
        return Result<HomingConfig>::Success(config);
    }
    Result<void> SetHomingConfig(int axis, const HomingConfig& config) override {
        if (axis >= 0 && axis < static_cast<int>(homing_configs.size())) {
            homing_configs[static_cast<size_t>(axis)] = config;
            return Result<void>::Success();
        }
        return Result<void>::Success();
    }
    Result<std::vector<HomingConfig>> GetAllHomingConfigs() const override {
        return Result<std::vector<HomingConfig>>::Success(homing_configs);
    }
    Result<ValveSupplyConfig> GetValveSupplyConfig() const override { return Result<ValveSupplyConfig>::Success({}); }
    Result<DispenserValveConfig> GetDispenserValveConfig() const override {
        return Result<DispenserValveConfig>::Success({});
    }
    Result<ValveCoordinationConfig> GetValveCoordinationConfig() const override {
        return Result<ValveCoordinationConfig>::Success({});
    }
    Result<VelocityTraceConfig> GetVelocityTraceConfig() const override {
        return Result<VelocityTraceConfig>::Success({});
    }
    Result<bool> ValidateConfiguration() const override { return Result<bool>::Success(true); }
    Result<std::vector<std::string>> GetValidationErrors() const override {
        return Result<std::vector<std::string>>::Success({});
    }
    Result<void> BackupConfiguration(const std::string&) override { return Result<void>::Success(); }
    Result<void> RestoreConfiguration(const std::string&) override { return Result<void>::Success(); }
    Result<HardwareMode> GetHardwareMode() const override { return Result<HardwareMode>::Success(HardwareMode::Mock); }
    Result<HardwareConfiguration> GetHardwareConfiguration() const override {
        return Result<HardwareConfiguration>::Success(hardware_config);
    }
};

}  // namespace Siligen::DxfGeometry::Tests::Support
