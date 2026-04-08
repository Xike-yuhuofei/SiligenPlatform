#include "application/usecases/motion/homing/HomeAxesUseCase.h"
#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "domain/motion/ports/IHomingPort.h"
#include "domain/supervision/ports/IEventPublisherPort.h"
#include "shared/types/Error.h"
#include "shared/types/HardwareConfiguration.h"
#include "shared/types/Types.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {
class FakeConfigurationPort : public Siligen::Domain::Configuration::Ports::IConfigurationPort {
   public:
    explicit FakeConfigurationPort(int axis_count) {
        hardware_config_.num_axes = axis_count;
    }

    void SetHomingConfigData(int axis, const Siligen::Domain::Configuration::Ports::HomingConfig& config) {
        homing_configs_[axis] = config;
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Configuration::Ports::SystemConfig> LoadConfiguration() override {
        return NotImplemented<Siligen::Domain::Configuration::Ports::SystemConfig>("LoadConfiguration");
    }

    Siligen::Shared::Types::Result<void> SaveConfiguration(
        const Siligen::Domain::Configuration::Ports::SystemConfig& /*config*/) override {
        return NotImplementedVoid("SaveConfiguration");
    }

    Siligen::Shared::Types::Result<void> ReloadConfiguration() override {
        return NotImplementedVoid("ReloadConfiguration");
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Configuration::Ports::DispensingConfig> GetDispensingConfig() const override {
        return NotImplemented<Siligen::Domain::Configuration::Ports::DispensingConfig>("GetDispensingConfig");
    }

    Siligen::Shared::Types::Result<void> SetDispensingConfig(
        const Siligen::Domain::Configuration::Ports::DispensingConfig& /*config*/) override {
        return NotImplementedVoid("SetDispensingConfig");
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Configuration::Ports::DxfPreprocessConfig>
    GetDxfPreprocessConfig() const override {
        return Siligen::Shared::Types::Result<Siligen::Domain::Configuration::Ports::DxfPreprocessConfig>::Success(
            Siligen::Domain::Configuration::Ports::DxfPreprocessConfig());
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig>
    GetDxfTrajectoryConfig() const override {
        return Siligen::Shared::Types::Result<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig>::Success(
            Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig());
    }

    Siligen::Shared::Types::Result<Siligen::Shared::Types::DiagnosticsConfig> GetDiagnosticsConfig() const override {
        return NotImplemented<Siligen::Shared::Types::DiagnosticsConfig>("GetDiagnosticsConfig");
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Configuration::Ports::MachineConfig> GetMachineConfig() const override {
        return NotImplemented<Siligen::Domain::Configuration::Ports::MachineConfig>("GetMachineConfig");
    }

    Siligen::Shared::Types::Result<void> SetMachineConfig(
        const Siligen::Domain::Configuration::Ports::MachineConfig& /*config*/) override {
        return NotImplementedVoid("SetMachineConfig");
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Configuration::Ports::HomingConfig> GetHomingConfig(int axis) const override {
        auto it = homing_configs_.find(axis);
        if (it == homing_configs_.end()) {
            return Siligen::Shared::Types::Result<Siligen::Domain::Configuration::Ports::HomingConfig>::Failure(
                Siligen::Shared::Types::Error(
                    Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER,
                    "Homing config not found",
                    "FakeConfigurationPort"));
        }
        return Siligen::Shared::Types::Result<Siligen::Domain::Configuration::Ports::HomingConfig>::Success(it->second);
    }

    Siligen::Shared::Types::Result<void> SetHomingConfig(
        int /*axis*/, const Siligen::Domain::Configuration::Ports::HomingConfig& /*config*/) override {
        return NotImplementedVoid("SetHomingConfig");
    }

    Siligen::Shared::Types::Result<std::vector<Siligen::Domain::Configuration::Ports::HomingConfig>>
    GetAllHomingConfigs() const override {
        std::vector<Siligen::Domain::Configuration::Ports::HomingConfig> configs;
        configs.reserve(homing_configs_.size());
        for (const auto& entry : homing_configs_) {
            configs.push_back(entry.second);
        }
        return Siligen::Shared::Types::Result<std::vector<Siligen::Domain::Configuration::Ports::HomingConfig>>::Success(
            configs);
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Configuration::Ports::ValveSupplyConfig> GetValveSupplyConfig() const override {
        return NotImplemented<Siligen::Domain::Configuration::Ports::ValveSupplyConfig>("GetValveSupplyConfig");
    }

    Siligen::Shared::Types::Result<Siligen::Shared::Types::DispenserValveConfig> GetDispenserValveConfig() const override {
        return NotImplemented<Siligen::Shared::Types::DispenserValveConfig>("GetDispenserValveConfig");
    }

    Siligen::Shared::Types::Result<Siligen::Shared::Types::ValveCoordinationConfig> GetValveCoordinationConfig() const override {
        return NotImplemented<Siligen::Shared::Types::ValveCoordinationConfig>("GetValveCoordinationConfig");
    }

    Siligen::Shared::Types::Result<Siligen::Shared::Types::VelocityTraceConfig> GetVelocityTraceConfig() const override {
        return NotImplemented<Siligen::Shared::Types::VelocityTraceConfig>("GetVelocityTraceConfig");
    }

    Siligen::Shared::Types::Result<bool> ValidateConfiguration() const override {
        return Siligen::Shared::Types::Result<bool>::Success(true);
    }

    Siligen::Shared::Types::Result<std::vector<std::string>> GetValidationErrors() const override {
        return Siligen::Shared::Types::Result<std::vector<std::string>>::Success({});
    }

    Siligen::Shared::Types::Result<void> BackupConfiguration(const std::string& /*backup_path*/) override {
        return NotImplementedVoid("BackupConfiguration");
    }

    Siligen::Shared::Types::Result<void> RestoreConfiguration(const std::string& /*backup_path*/) override {
        return NotImplementedVoid("RestoreConfiguration");
    }

    Siligen::Shared::Types::Result<Siligen::Shared::Types::HardwareMode> GetHardwareMode() const override {
        return Siligen::Shared::Types::Result<Siligen::Shared::Types::HardwareMode>::Success(
            Siligen::Shared::Types::HardwareMode::Mock);
    }

    Siligen::Shared::Types::Result<Siligen::Shared::Types::HardwareConfiguration> GetHardwareConfiguration() const override {
        return Siligen::Shared::Types::Result<Siligen::Shared::Types::HardwareConfiguration>::Success(hardware_config_);
    }

   private:
    template <typename T>
    Siligen::Shared::Types::Result<T> NotImplemented(const char* method) const {
        return Siligen::Shared::Types::Result<T>::Failure(
            Siligen::Shared::Types::Error(
                Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED,
                std::string(method) + " not implemented",
                "FakeConfigurationPort"));
    }

    Siligen::Shared::Types::Result<void> NotImplementedVoid(const char* method) const {
        return Siligen::Shared::Types::Result<void>::Failure(
            Siligen::Shared::Types::Error(
                Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED,
                std::string(method) + " not implemented",
                "FakeConfigurationPort"));
    }

    Siligen::Shared::Types::HardwareConfiguration hardware_config_;
    std::unordered_map<int, Siligen::Domain::Configuration::Ports::HomingConfig> homing_configs_;
};

class StubHomingPort : public Siligen::Domain::Motion::Ports::IHomingPort {
   public:
    Siligen::Shared::Types::Result<void> HomeAxis(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> StopHoming(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> ResetHomingState(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::HomingStatus> GetHomingStatus(
        Siligen::Shared::Types::LogicalAxisId axis) const {
        Siligen::Domain::Motion::Ports::HomingStatus status;
        status.axis = axis;
        status.state = Siligen::Domain::Motion::Ports::HomingState::HOMED;
        return Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::HomingStatus>::Success(status);
    }

    Siligen::Shared::Types::Result<bool> IsAxisHomed(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) const {
        return Siligen::Shared::Types::Result<bool>::Success(true);
    }

    Siligen::Shared::Types::Result<bool> IsHomingInProgress(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) const {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    Siligen::Shared::Types::Result<void> WaitForHomingComplete(
        Siligen::Shared::Types::LogicalAxisId /*axis*/, int32 /*timeout_ms*/) {
        return Siligen::Shared::Types::Result<void>::Success();
    }
};

class TrackingHomingPort : public Siligen::Domain::Motion::Ports::IHomingPort {
   public:
    void SetAxisHomed(Siligen::Shared::Types::LogicalAxisId axis, bool homed) {
        homed_[AxisIndex(axis)] = homed;
    }

    void SetAxisHoming(Siligen::Shared::Types::LogicalAxisId axis, bool in_progress) {
        in_progress_[AxisIndex(axis)] = in_progress;
    }

    int HomeAxisCalls() const { return home_axis_calls_; }
    int WaitCalls() const { return wait_calls_; }

    Siligen::Shared::Types::Result<void> HomeAxis(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) {
        ++home_axis_calls_;
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> StopHoming(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> ResetHomingState(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::HomingStatus> GetHomingStatus(
        Siligen::Shared::Types::LogicalAxisId axis) const {
        Siligen::Domain::Motion::Ports::HomingStatus status;
        status.axis = axis;
        status.state = Siligen::Domain::Motion::Ports::HomingState::HOMED;
        return Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::HomingStatus>::Success(status);
    }

    Siligen::Shared::Types::Result<bool> IsAxisHomed(
        Siligen::Shared::Types::LogicalAxisId axis) const {
        return Siligen::Shared::Types::Result<bool>::Success(LookupFlag(homed_, axis));
    }

    Siligen::Shared::Types::Result<bool> IsHomingInProgress(
        Siligen::Shared::Types::LogicalAxisId axis) const {
        return Siligen::Shared::Types::Result<bool>::Success(LookupFlag(in_progress_, axis));
    }

    Siligen::Shared::Types::Result<void> WaitForHomingComplete(
        Siligen::Shared::Types::LogicalAxisId /*axis*/, int32 /*timeout_ms*/) {
        ++wait_calls_;
        return Siligen::Shared::Types::Result<void>::Success();
    }

   private:
    static int AxisIndex(Siligen::Shared::Types::LogicalAxisId axis) {
        return static_cast<int>(Siligen::Shared::Types::ToIndex(axis));
    }

    static bool LookupFlag(const std::unordered_map<int, bool>& map,
                           Siligen::Shared::Types::LogicalAxisId axis) {
        auto it = map.find(AxisIndex(axis));
        if (it == map.end()) {
            return false;
        }
        return it->second;
    }

    int home_axis_calls_ = 0;
    int wait_calls_ = 0;
    std::unordered_map<int, bool> homed_;
    std::unordered_map<int, bool> in_progress_;
};

class OrderedHomingPort : public Siligen::Domain::Motion::Ports::IHomingPort {
   public:
    Siligen::Shared::Types::Result<void> HomeAxis(
        Siligen::Shared::Types::LogicalAxisId axis) override {
        call_order.emplace_back("home:" + std::to_string(static_cast<int>(Siligen::Shared::Types::ToIndex(axis))));
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> StopHoming(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) override {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> ResetHomingState(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) override {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::HomingStatus> GetHomingStatus(
        Siligen::Shared::Types::LogicalAxisId axis) const override {
        Siligen::Domain::Motion::Ports::HomingStatus status;
        status.axis = axis;
        status.state = Siligen::Domain::Motion::Ports::HomingState::HOMED;
        return Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::HomingStatus>::Success(status);
    }

    Siligen::Shared::Types::Result<bool> IsAxisHomed(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) const override {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    Siligen::Shared::Types::Result<bool> IsHomingInProgress(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) const override {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    Siligen::Shared::Types::Result<void> WaitForHomingComplete(
        Siligen::Shared::Types::LogicalAxisId axis, int32 /*timeout_ms*/) override {
        call_order.emplace_back("wait:" + std::to_string(static_cast<int>(Siligen::Shared::Types::ToIndex(axis))));
        return Siligen::Shared::Types::Result<void>::Success();
    }

    std::vector<std::string> call_order;
};

class RetryHomingPort : public Siligen::Domain::Motion::Ports::IHomingPort {
   public:
    int HomeAxisCalls() const { return home_axis_calls_; }
    int WaitCalls() const { return wait_calls_; }

    Siligen::Shared::Types::Result<void> HomeAxis(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) {
        ++home_axis_calls_;
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> StopHoming(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> ResetHomingState(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::HomingStatus> GetHomingStatus(
        Siligen::Shared::Types::LogicalAxisId axis) const {
        Siligen::Domain::Motion::Ports::HomingStatus status;
        status.axis = axis;
        if (status_calls_++ == 0) {
            status.state = Siligen::Domain::Motion::Ports::HomingState::FAILED;
        } else {
            status.state = Siligen::Domain::Motion::Ports::HomingState::HOMED;
        }
        return Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::HomingStatus>::Success(status);
    }

    Siligen::Shared::Types::Result<bool> IsAxisHomed(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) const {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    Siligen::Shared::Types::Result<bool> IsHomingInProgress(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) const {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    Siligen::Shared::Types::Result<void> WaitForHomingComplete(
        Siligen::Shared::Types::LogicalAxisId /*axis*/, int32 /*timeout_ms*/) {
        ++wait_calls_;
        return Siligen::Shared::Types::Result<void>::Success();
    }

   private:
    int home_axis_calls_ = 0;
    int wait_calls_ = 0;
    mutable int status_calls_ = 0;
};

class FailingWaitHomingPort : public Siligen::Domain::Motion::Ports::IHomingPort {
   public:
    int StopCalls() const { return stop_calls_; }
    int ResetCalls() const { return reset_calls_; }

    Siligen::Shared::Types::Result<void> HomeAxis(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) override {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> StopHoming(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) override {
        ++stop_calls_;
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> ResetHomingState(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) override {
        ++reset_calls_;
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::HomingStatus> GetHomingStatus(
        Siligen::Shared::Types::LogicalAxisId axis) const override {
        Siligen::Domain::Motion::Ports::HomingStatus status;
        status.axis = axis;
        status.state = Siligen::Domain::Motion::Ports::HomingState::NOT_HOMED;
        return Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::HomingStatus>::Success(status);
    }

    Siligen::Shared::Types::Result<bool> IsAxisHomed(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) const override {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    Siligen::Shared::Types::Result<bool> IsHomingInProgress(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) const override {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    Siligen::Shared::Types::Result<void> WaitForHomingComplete(
        Siligen::Shared::Types::LogicalAxisId /*axis*/, int32 /*timeout_ms*/) override {
        return Siligen::Shared::Types::Result<void>::Failure(
            Siligen::Shared::Types::Error(
                Siligen::Shared::Types::ErrorCode::TIMEOUT,
                "Homing timeout",
                "FailingWaitHomingPort"));
    }

   private:
    int stop_calls_ = 0;
    int reset_calls_ = 0;
};

class LateSuccessAfterWaitFailureHomingPort : public Siligen::Domain::Motion::Ports::IHomingPort {
   public:
    int StopCalls() const { return stop_calls_; }
    int ResetCalls() const { return reset_calls_; }

    Siligen::Shared::Types::Result<void> HomeAxis(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) override {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> StopHoming(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) override {
        ++stop_calls_;
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> ResetHomingState(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) override {
        ++reset_calls_;
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::HomingStatus> GetHomingStatus(
        Siligen::Shared::Types::LogicalAxisId axis) const override {
        Siligen::Domain::Motion::Ports::HomingStatus status;
        status.axis = axis;
        status.state = Siligen::Domain::Motion::Ports::HomingState::HOMED;
        return Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::HomingStatus>::Success(status);
    }

    Siligen::Shared::Types::Result<bool> IsAxisHomed(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) const override {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    Siligen::Shared::Types::Result<bool> IsHomingInProgress(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) const override {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    Siligen::Shared::Types::Result<void> WaitForHomingComplete(
        Siligen::Shared::Types::LogicalAxisId /*axis*/, int32 /*timeout_ms*/) override {
        return Siligen::Shared::Types::Result<void>::Failure(
            Siligen::Shared::Types::Error(
                Siligen::Shared::Types::ErrorCode::HARDWARE_ERROR,
                "Homing failed",
                "LateSuccessAfterWaitFailureHomingPort"));
    }

   private:
    int stop_calls_ = 0;
    int reset_calls_ = 0;
};

class RestartFailureHomingPort : public Siligen::Domain::Motion::Ports::IHomingPort {
   public:
    int HomeCalls() const { return home_calls_; }
    int StopCalls() const { return stop_calls_; }
    int ResetCalls() const { return reset_calls_; }

    Siligen::Shared::Types::Result<void> HomeAxis(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) override {
        ++home_calls_;
        if (home_calls_ >= 2) {
            return Siligen::Shared::Types::Result<void>::Failure(
                Siligen::Shared::Types::Error(
                    Siligen::Shared::Types::ErrorCode::HARDWARE_OPERATION_FAILED,
                    "restart homing failed",
                    "RestartFailureHomingPort"));
        }
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> StopHoming(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) override {
        ++stop_calls_;
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> ResetHomingState(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) override {
        ++reset_calls_;
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::HomingStatus> GetHomingStatus(
        Siligen::Shared::Types::LogicalAxisId axis) const override {
        Siligen::Domain::Motion::Ports::HomingStatus status;
        status.axis = axis;
        status.state = Siligen::Domain::Motion::Ports::HomingState::NOT_HOMED;
        return Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::HomingStatus>::Success(status);
    }

    Siligen::Shared::Types::Result<bool> IsAxisHomed(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) const override {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    Siligen::Shared::Types::Result<bool> IsHomingInProgress(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) const override {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    Siligen::Shared::Types::Result<void> WaitForHomingComplete(
        Siligen::Shared::Types::LogicalAxisId /*axis*/, int32 /*timeout_ms*/) override {
        return Siligen::Shared::Types::Result<void>::Failure(
            Siligen::Shared::Types::Error(
                Siligen::Shared::Types::ErrorCode::TIMEOUT,
                "Homing timeout",
                "RestartFailureHomingPort"));
    }

   private:
    int home_calls_ = 0;
    int stop_calls_ = 0;
    int reset_calls_ = 0;
};

class SequencedMotionStatePort : public Siligen::Domain::Motion::Ports::IMotionStatePort {
   public:
    void SetAxisVelocitySequence(Siligen::Shared::Types::LogicalAxisId axis,
                                 std::vector<Siligen::Shared::Types::float32> samples) {
        velocity_samples_[AxisIndex(axis)] = std::move(samples);
    }

    Siligen::Shared::Types::Result<Siligen::Shared::Types::Point2D> GetCurrentPosition() const override {
        return Siligen::Shared::Types::Result<Siligen::Shared::Types::Point2D>::Success(
            Siligen::Shared::Types::Point2D{});
    }

    Siligen::Shared::Types::Result<Siligen::Shared::Types::float32> GetAxisPosition(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) const override {
        return Siligen::Shared::Types::Result<Siligen::Shared::Types::float32>::Success(0.0f);
    }

    Siligen::Shared::Types::Result<Siligen::Shared::Types::float32> GetAxisVelocity(
        Siligen::Shared::Types::LogicalAxisId axis) const override {
        const auto axis_index = AxisIndex(axis);
        auto it = velocity_samples_.find(axis_index);
        if (it == velocity_samples_.end() || it->second.empty()) {
            return Siligen::Shared::Types::Result<Siligen::Shared::Types::float32>::Success(0.0f);
        }

        auto& cursor = sample_cursor_[axis_index];
        const auto& samples = it->second;
        const auto capped_cursor = std::min(cursor, samples.size() - 1);
        const auto value = samples[capped_cursor];
        if (cursor + 1 < samples.size()) {
            ++cursor;
        }
        return Siligen::Shared::Types::Result<Siligen::Shared::Types::float32>::Success(value);
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::MotionStatus> GetAxisStatus(
        Siligen::Shared::Types::LogicalAxisId axis) const override {
        Siligen::Domain::Motion::Ports::MotionStatus status;
        status.state = Siligen::Domain::Motion::Ports::MotionState::IDLE;
        status.enabled = true;
        status.velocity = PeekVelocity(axis);
        return Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::MotionStatus>::Success(status);
    }

    Siligen::Shared::Types::Result<bool> IsAxisMoving(
        Siligen::Shared::Types::LogicalAxisId axis) const override {
        return Siligen::Shared::Types::Result<bool>::Success(PeekVelocity(axis) > 0.1f);
    }

    Siligen::Shared::Types::Result<bool> IsAxisInPosition(
        Siligen::Shared::Types::LogicalAxisId /*axis*/) const override {
        return Siligen::Shared::Types::Result<bool>::Success(true);
    }

    Siligen::Shared::Types::Result<std::vector<Siligen::Domain::Motion::Ports::MotionStatus>> GetAllAxesStatus()
        const override {
        return Siligen::Shared::Types::Result<std::vector<Siligen::Domain::Motion::Ports::MotionStatus>>::Success({});
    }

   private:
    static int AxisIndex(Siligen::Shared::Types::LogicalAxisId axis) {
        return static_cast<int>(Siligen::Shared::Types::ToIndex(axis));
    }

    Siligen::Shared::Types::float32 PeekVelocity(Siligen::Shared::Types::LogicalAxisId axis) const {
        const auto axis_index = AxisIndex(axis);
        auto it = velocity_samples_.find(axis_index);
        if (it == velocity_samples_.end() || it->second.empty()) {
            return 0.0f;
        }
        auto cursor_it = sample_cursor_.find(axis_index);
        const auto cursor = cursor_it == sample_cursor_.end() ? static_cast<size_t>(0) : cursor_it->second;
        return it->second[std::min(cursor, it->second.size() - 1)];
    }

    mutable std::unordered_map<int, size_t> sample_cursor_;
    std::unordered_map<int, std::vector<Siligen::Shared::Types::float32>> velocity_samples_;
};

class StubEventPublisherPort : public Siligen::Domain::System::Ports::IEventPublisherPort {
   public:
    Siligen::Shared::Types::Result<void> Publish(const Siligen::Domain::System::Ports::DomainEvent& /*event*/) override {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> PublishAsync(const Siligen::Domain::System::Ports::DomainEvent& /*event*/) override {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<int32> Subscribe(
        Siligen::Domain::System::Ports::EventType /*type*/,
        Siligen::Domain::System::Ports::EventHandler /*handler*/) override {
        return Siligen::Shared::Types::Result<int32>::Success(1);
    }

    Siligen::Shared::Types::Result<void> Unsubscribe(int32 /*subscription_id*/) override {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> UnsubscribeAll(
        Siligen::Domain::System::Ports::EventType /*type*/) override {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<std::vector<Siligen::Domain::System::Ports::DomainEvent*>> GetEventHistory(
        Siligen::Domain::System::Ports::EventType /*type*/, int32 /*max_count*/) const override {
        return Siligen::Shared::Types::Result<std::vector<Siligen::Domain::System::Ports::DomainEvent*>>::Success({});
    }

    Siligen::Shared::Types::Result<void> ClearEventHistory() override {
        return Siligen::Shared::Types::Result<void>::Success();
    }
};

}  // namespace

TEST(HomeAxesUseCaseTest, ExecutesHomingForConfiguredAxes) {
    auto config_port = std::make_shared<FakeConfigurationPort>(1);
    Siligen::Domain::Configuration::Ports::HomingConfig homing_config;
    homing_config.axis = 0;
    homing_config.mode = 1;
    homing_config.rapid_velocity = 10.0f;
    homing_config.locate_velocity = 5.0f;
    config_port->SetHomingConfigData(0, homing_config);

    auto homing_port = std::make_shared<StubHomingPort>();
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port;
    auto event_port = std::make_shared<StubEventPublisherPort>();

    Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase use_case(
        homing_port, config_port, motion_connection_port, event_port);

    Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest request;
    request.home_all_axes = true;
    request.wait_for_completion = true;
    request.timeout_ms = 1000;

    auto result = use_case.Execute(request);
    ASSERT_TRUE(result.IsSuccess());

    const auto& response = result.Value();
    EXPECT_EQ(response.successfully_homed_axes.size(), 1u);
    EXPECT_TRUE(response.failed_axes.empty());
    EXPECT_TRUE(response.all_completed);
    ASSERT_EQ(response.axis_results.size(), 1u);
    EXPECT_EQ(response.axis_results[0].axis, Siligen::Shared::Types::LogicalAxisId::X);
    EXPECT_TRUE(response.axis_results[0].success);
    EXPECT_EQ(response.axis_results[0].message, "Already homed");
}

TEST(HomeAxesUseCaseTest, RejectsInvalidRequest) {
    auto config_port = std::make_shared<FakeConfigurationPort>(1);
    Siligen::Domain::Configuration::Ports::HomingConfig homing_config;
    homing_config.axis = 0;
    config_port->SetHomingConfigData(0, homing_config);

    auto homing_port = std::make_shared<StubHomingPort>();
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port;
    auto event_port = std::make_shared<StubEventPublisherPort>();

    Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase use_case(
        homing_port, config_port, motion_connection_port, event_port);

    Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest request;
    request.home_all_axes = true;
    request.axes = {Siligen::Shared::Types::LogicalAxisId::X};

    auto result = use_case.Execute(request);
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER);
}

TEST(HomeAxesUseCaseTest, SkipsAlreadyHomedAxes) {
    auto config_port = std::make_shared<FakeConfigurationPort>(1);
    Siligen::Domain::Configuration::Ports::HomingConfig homing_config;
    homing_config.axis = 0;
    config_port->SetHomingConfigData(0, homing_config);

    auto homing_port = std::make_shared<TrackingHomingPort>();
    homing_port->SetAxisHomed(Siligen::Shared::Types::LogicalAxisId::X, true);
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port;
    auto event_port = std::make_shared<StubEventPublisherPort>();

    Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase use_case(
        homing_port, config_port, motion_connection_port, event_port);

    Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest request;
    request.home_all_axes = true;
    request.wait_for_completion = true;
    request.timeout_ms = 1000;

    auto result = use_case.Execute(request);
    ASSERT_TRUE(result.IsSuccess());

    const auto& response = result.Value();
    EXPECT_EQ(homing_port->HomeAxisCalls(), 0);
    EXPECT_EQ(homing_port->WaitCalls(), 0);
    EXPECT_EQ(response.successfully_homed_axes.size(), 1u);
    EXPECT_TRUE(response.failed_axes.empty());
    EXPECT_TRUE(response.all_completed);
    ASSERT_EQ(response.axis_results.size(), 1u);
    EXPECT_EQ(response.axis_results[0].message, "Already homed");
}

TEST(HomeAxesUseCaseTest, ForcesRehomeWhenRequested) {
    auto config_port = std::make_shared<FakeConfigurationPort>(1);
    Siligen::Domain::Configuration::Ports::HomingConfig homing_config;
    homing_config.axis = 0;
    config_port->SetHomingConfigData(0, homing_config);

    auto homing_port = std::make_shared<TrackingHomingPort>();
    homing_port->SetAxisHomed(Siligen::Shared::Types::LogicalAxisId::X, true);
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port;
    auto event_port = std::make_shared<StubEventPublisherPort>();

    Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase use_case(
        homing_port, config_port, motion_connection_port, event_port);

    Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest request;
    request.home_all_axes = true;
    request.force_rehome = true;
    request.wait_for_completion = true;
    request.timeout_ms = 1000;

    auto result = use_case.Execute(request);
    ASSERT_TRUE(result.IsSuccess());

    const auto& response = result.Value();
    EXPECT_EQ(homing_port->HomeAxisCalls(), 1);
    EXPECT_EQ(homing_port->WaitCalls(), 1);
    EXPECT_TRUE(response.failed_axes.empty());
    EXPECT_TRUE(response.all_completed);
}

TEST(HomeAxesUseCaseTest, DoesNotRestartWhenHomingInProgress) {
    auto config_port = std::make_shared<FakeConfigurationPort>(1);
    Siligen::Domain::Configuration::Ports::HomingConfig homing_config;
    homing_config.axis = 0;
    config_port->SetHomingConfigData(0, homing_config);

    auto homing_port = std::make_shared<TrackingHomingPort>();
    homing_port->SetAxisHomed(Siligen::Shared::Types::LogicalAxisId::X, false);
    homing_port->SetAxisHoming(Siligen::Shared::Types::LogicalAxisId::X, true);
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port;
    auto event_port = std::make_shared<StubEventPublisherPort>();

    Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase use_case(
        homing_port, config_port, motion_connection_port, event_port);

    Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest request;
    request.home_all_axes = true;
    request.wait_for_completion = true;
    request.timeout_ms = 1000;

    auto result = use_case.Execute(request);
    ASSERT_TRUE(result.IsSuccess());

    const auto& response = result.Value();
    EXPECT_EQ(homing_port->HomeAxisCalls(), 0);
    EXPECT_EQ(homing_port->WaitCalls(), 1);
    EXPECT_EQ(response.successfully_homed_axes.size(), 1u);
    EXPECT_TRUE(response.failed_axes.empty());
    EXPECT_TRUE(response.all_completed);
}

TEST(HomeAxesUseCaseTest, StartsAllRequestedAxesBeforeWaitingForCompletion) {
    auto config_port = std::make_shared<FakeConfigurationPort>(2);
    Siligen::Domain::Configuration::Ports::HomingConfig x_config;
    x_config.axis = 0;
    config_port->SetHomingConfigData(0, x_config);
    Siligen::Domain::Configuration::Ports::HomingConfig y_config;
    y_config.axis = 1;
    config_port->SetHomingConfigData(1, y_config);

    auto homing_port = std::make_shared<OrderedHomingPort>();
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port;
    auto event_port = std::make_shared<StubEventPublisherPort>();

    Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase use_case(
        homing_port, config_port, motion_connection_port, event_port);

    Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest request;
    request.home_all_axes = true;
    request.wait_for_completion = true;
    request.timeout_ms = 1000;

    auto result = use_case.Execute(request);
    ASSERT_TRUE(result.IsSuccess());

    EXPECT_EQ(homing_port->call_order,
              (std::vector<std::string>{"home:0", "home:1", "wait:0", "wait:1"}));
    EXPECT_EQ(result.Value().successfully_homed_axes.size(), 2u);
    EXPECT_TRUE(result.Value().failed_axes.empty());
    EXPECT_TRUE(result.Value().all_completed);
    ASSERT_EQ(result.Value().axis_results.size(), 2u);
}

TEST(HomeAxesUseCaseTest, RetriesWhenHomingVerificationFails) {
    auto config_port = std::make_shared<FakeConfigurationPort>(1);
    Siligen::Domain::Configuration::Ports::HomingConfig homing_config;
    homing_config.axis = 0;
    homing_config.mode = 1;
    homing_config.rapid_velocity = 10.0f;
    homing_config.locate_velocity = 5.0f;
    homing_config.retry_count = 1;
    config_port->SetHomingConfigData(0, homing_config);

    auto homing_port = std::make_shared<RetryHomingPort>();
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port;
    auto event_port = std::make_shared<StubEventPublisherPort>();

    Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase use_case(
        homing_port, config_port, motion_connection_port, event_port);

    Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest request;
    request.home_all_axes = true;
    request.wait_for_completion = true;
    request.timeout_ms = 1000;

    auto result = use_case.Execute(request);
    ASSERT_TRUE(result.IsSuccess());

    const auto& response = result.Value();
    EXPECT_EQ(homing_port->HomeAxisCalls(), 2);
    EXPECT_EQ(homing_port->WaitCalls(), 2);
    EXPECT_TRUE(response.failed_axes.empty());
    EXPECT_TRUE(response.all_completed);
}

TEST(HomeAxesUseCaseTest, WaitsForAxisToSettleAfterHomingBeforeReturningSuccess) {
    auto config_port = std::make_shared<FakeConfigurationPort>(1);
    Siligen::Domain::Configuration::Ports::HomingConfig homing_config;
    homing_config.axis = 0;
    homing_config.settle_time_ms = 60;
    config_port->SetHomingConfigData(0, homing_config);

    auto homing_port = std::make_shared<TrackingHomingPort>();
    homing_port->SetAxisHomed(Siligen::Shared::Types::LogicalAxisId::X, false);
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>();
    motion_state_port->SetAxisVelocitySequence(
        Siligen::Shared::Types::LogicalAxisId::X,
        {5.0f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port;
    auto event_port = std::make_shared<StubEventPublisherPort>();

    Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase use_case(
        homing_port, config_port, motion_connection_port, event_port, motion_state_port);

    Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest request;
    request.home_all_axes = true;
    request.wait_for_completion = true;
    request.timeout_ms = 500;

    const auto started = std::chrono::steady_clock::now();
    auto result = use_case.Execute(request);
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_GE(elapsed_ms, 100);
    EXPECT_TRUE(result.Value().all_completed);
}

TEST(HomeAxesUseCaseTest, FailsWhenAxisDoesNotSettleBeforeTimeout) {
    auto config_port = std::make_shared<FakeConfigurationPort>(1);
    Siligen::Domain::Configuration::Ports::HomingConfig homing_config;
    homing_config.axis = 0;
    homing_config.settle_time_ms = 80;
    config_port->SetHomingConfigData(0, homing_config);

    auto homing_port = std::make_shared<TrackingHomingPort>();
    homing_port->SetAxisHomed(Siligen::Shared::Types::LogicalAxisId::X, false);
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>();
    motion_state_port->SetAxisVelocitySequence(
        Siligen::Shared::Types::LogicalAxisId::X,
        {5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f});
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port;
    auto event_port = std::make_shared<StubEventPublisherPort>();

    Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase use_case(
        homing_port, config_port, motion_connection_port, event_port, motion_state_port);

    Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest request;
    request.home_all_axes = true;
    request.wait_for_completion = true;
    request.timeout_ms = 120;

    auto result = use_case.Execute(request);
    ASSERT_TRUE(result.IsSuccess());
    ASSERT_EQ(result.Value().failed_axes.size(), 1u);
    EXPECT_FALSE(result.Value().all_completed);
    ASSERT_EQ(result.Value().error_messages.size(), 1u);
    EXPECT_NE(result.Value().error_messages.front().find("did not settle after homing"), std::string::npos);
}

TEST(HomeAxesUseCaseTest, StopsAndResetsAxisWhenHomingUltimatelyFails) {
    auto config_port = std::make_shared<FakeConfigurationPort>(1);
    Siligen::Domain::Configuration::Ports::HomingConfig homing_config;
    homing_config.axis = 0;
    homing_config.retry_count = 0;
    config_port->SetHomingConfigData(0, homing_config);

    auto homing_port = std::make_shared<FailingWaitHomingPort>();
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port;
    auto event_port = std::make_shared<StubEventPublisherPort>();

    Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase use_case(
        homing_port, config_port, motion_connection_port, event_port);

    Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest request;
    request.home_all_axes = true;
    request.wait_for_completion = true;
    request.timeout_ms = 100;

    auto result = use_case.Execute(request);
    ASSERT_TRUE(result.IsSuccess());
    EXPECT_FALSE(result.Value().all_completed);
    EXPECT_EQ(result.Value().failed_axes.size(), 1u);
    EXPECT_EQ(homing_port->StopCalls(), 1);
    EXPECT_EQ(homing_port->ResetCalls(), 1);
    ASSERT_EQ(result.Value().axis_results.size(), 1u);
    EXPECT_FALSE(result.Value().axis_results[0].success);
    EXPECT_EQ(result.Value().axis_results[0].message, "Homing timeout");
}

TEST(HomeAxesUseCaseTest, TreatsLateHomedStatusAsSuccessAfterWaitFailure) {
    auto config_port = std::make_shared<FakeConfigurationPort>(1);
    Siligen::Domain::Configuration::Ports::HomingConfig homing_config;
    homing_config.axis = 0;
    homing_config.retry_count = 0;
    config_port->SetHomingConfigData(0, homing_config);

    auto homing_port = std::make_shared<LateSuccessAfterWaitFailureHomingPort>();
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port;
    auto event_port = std::make_shared<StubEventPublisherPort>();

    Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase use_case(
        homing_port, config_port, motion_connection_port, event_port);

    Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest request;
    request.home_all_axes = true;
    request.wait_for_completion = true;
    request.timeout_ms = 100;

    auto result = use_case.Execute(request);
    ASSERT_TRUE(result.IsSuccess());
    EXPECT_TRUE(result.Value().all_completed);
    EXPECT_TRUE(result.Value().failed_axes.empty());
    EXPECT_EQ(result.Value().successfully_homed_axes.size(), 1u);
    EXPECT_TRUE(result.Value().error_messages.empty());
    EXPECT_EQ(homing_port->StopCalls(), 0);
    EXPECT_EQ(homing_port->ResetCalls(), 0);
}

TEST(HomeAxesUseCaseTest, StopsAndResetsAxisWhenRetryStartFails) {
    auto config_port = std::make_shared<FakeConfigurationPort>(1);
    Siligen::Domain::Configuration::Ports::HomingConfig homing_config;
    homing_config.axis = 0;
    homing_config.retry_count = 1;
    config_port->SetHomingConfigData(0, homing_config);

    auto homing_port = std::make_shared<RestartFailureHomingPort>();
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port;
    auto event_port = std::make_shared<StubEventPublisherPort>();

    Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase use_case(
        homing_port, config_port, motion_connection_port, event_port);

    Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest request;
    request.home_all_axes = true;
    request.wait_for_completion = true;
    request.timeout_ms = 100;

    auto result = use_case.Execute(request);
    ASSERT_TRUE(result.IsSuccess());
    EXPECT_FALSE(result.Value().all_completed);
    EXPECT_EQ(result.Value().failed_axes.size(), 1u);
    EXPECT_EQ(homing_port->HomeCalls(), 2);
    EXPECT_EQ(homing_port->StopCalls(), 2);
    EXPECT_EQ(homing_port->ResetCalls(), 2);
}
