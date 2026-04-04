#include "domain/dispensing/domain-services/PurgeDispenserProcess.h"

#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "shared/types/Error.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

class FakeConfigurationPort : public Siligen::Domain::Configuration::Ports::IConfigurationPort {
   public:
    explicit FakeConfigurationPort(int32_t stabilization_ms) {
        dispensing_.supply_stabilization_ms = stabilization_ms;
    }

    Result<Siligen::Domain::Configuration::Ports::SystemConfig> LoadConfiguration() override {
        return NotImplemented<Siligen::Domain::Configuration::Ports::SystemConfig>("LoadConfiguration");
    }

    Result<void> SaveConfiguration(const Siligen::Domain::Configuration::Ports::SystemConfig&) override {
        return NotImplementedVoid("SaveConfiguration");
    }

    Result<void> ReloadConfiguration() override {
        return NotImplementedVoid("ReloadConfiguration");
    }

    Result<Siligen::Domain::Configuration::Ports::DispensingConfig> GetDispensingConfig() const override {
        return Result<Siligen::Domain::Configuration::Ports::DispensingConfig>::Success(dispensing_);
    }

    Result<void> SetDispensingConfig(const Siligen::Domain::Configuration::Ports::DispensingConfig&) override {
        return NotImplementedVoid("SetDispensingConfig");
    }

    Result<Siligen::Domain::Configuration::Ports::DxfPreprocessConfig> GetDxfPreprocessConfig() const override {
        return Result<Siligen::Domain::Configuration::Ports::DxfPreprocessConfig>::Success(
            Siligen::Domain::Configuration::Ports::DxfPreprocessConfig());
    }

    Result<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig> GetDxfTrajectoryConfig() const override {
        return Result<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig>::Success(
            Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig());
    }

    Result<Siligen::Shared::Types::DiagnosticsConfig> GetDiagnosticsConfig() const override {
        return NotImplemented<Siligen::Shared::Types::DiagnosticsConfig>("GetDiagnosticsConfig");
    }

    Result<Siligen::Domain::Configuration::Ports::MachineConfig> GetMachineConfig() const override {
        return NotImplemented<Siligen::Domain::Configuration::Ports::MachineConfig>("GetMachineConfig");
    }

    Result<void> SetMachineConfig(const Siligen::Domain::Configuration::Ports::MachineConfig&) override {
        return NotImplementedVoid("SetMachineConfig");
    }

    Result<Siligen::Domain::Configuration::Ports::HomingConfig> GetHomingConfig(int) const override {
        return NotImplemented<Siligen::Domain::Configuration::Ports::HomingConfig>("GetHomingConfig");
    }

    Result<void> SetHomingConfig(int, const Siligen::Domain::Configuration::Ports::HomingConfig&) override {
        return NotImplementedVoid("SetHomingConfig");
    }

    Result<std::vector<Siligen::Domain::Configuration::Ports::HomingConfig>> GetAllHomingConfigs() const override {
        return NotImplemented<std::vector<Siligen::Domain::Configuration::Ports::HomingConfig>>("GetAllHomingConfigs");
    }

    Result<Siligen::Domain::Configuration::Ports::ValveSupplyConfig> GetValveSupplyConfig() const override {
        return NotImplemented<Siligen::Domain::Configuration::Ports::ValveSupplyConfig>("GetValveSupplyConfig");
    }

    Result<Siligen::Shared::Types::DispenserValveConfig> GetDispenserValveConfig() const override {
        return NotImplemented<Siligen::Shared::Types::DispenserValveConfig>("GetDispenserValveConfig");
    }

    Result<Siligen::Shared::Types::ValveCoordinationConfig> GetValveCoordinationConfig() const override {
        return NotImplemented<Siligen::Shared::Types::ValveCoordinationConfig>("GetValveCoordinationConfig");
    }

    Result<Siligen::Shared::Types::VelocityTraceConfig> GetVelocityTraceConfig() const override {
        return NotImplemented<Siligen::Shared::Types::VelocityTraceConfig>("GetVelocityTraceConfig");
    }

    Result<bool> ValidateConfiguration() const override {
        return Result<bool>::Success(true);
    }

    Result<std::vector<std::string>> GetValidationErrors() const override {
        return Result<std::vector<std::string>>::Success({});
    }

    Result<void> BackupConfiguration(const std::string&) override {
        return NotImplementedVoid("BackupConfiguration");
    }

    Result<void> RestoreConfiguration(const std::string&) override {
        return NotImplementedVoid("RestoreConfiguration");
    }

    Result<Siligen::Shared::Types::HardwareMode> GetHardwareMode() const override {
        return Result<Siligen::Shared::Types::HardwareMode>::Success(Siligen::Shared::Types::HardwareMode::Mock);
    }

    Result<Siligen::Shared::Types::HardwareConfiguration> GetHardwareConfiguration() const override {
        return Result<Siligen::Shared::Types::HardwareConfiguration>::Success(
            Siligen::Shared::Types::HardwareConfiguration());
    }

   private:
    template <typename T>
    Result<T> NotImplemented(const char* method) const {
        return Result<T>::Failure(
            Error(ErrorCode::NOT_IMPLEMENTED, std::string(method) + " not implemented"));
    }

    Result<void> NotImplementedVoid(const char* method) const {
        return Result<void>::Failure(
            Error(ErrorCode::NOT_IMPLEMENTED, std::string(method) + " not implemented"));
    }

    Siligen::Domain::Configuration::Ports::DispensingConfig dispensing_;
};

class TrackingValvePort : public Siligen::Domain::Dispensing::Ports::IValvePort {
   public:
    void SetStatusSequence(const std::vector<Siligen::Domain::Dispensing::Ports::DispenserValveState>& states) {
        status_sequence_ = states;
        status_index_ = 0;
    }

    int OpenSupplyCalls() const { return open_supply_calls_; }
    int CloseSupplyCalls() const { return close_supply_calls_; }
    int OpenDispenserCalls() const { return open_dispenser_calls_; }
    int CloseDispenserCalls() const { return close_dispenser_calls_; }

    std::chrono::steady_clock::time_point SupplyOpenTime() const { return supply_open_time_; }
    std::chrono::steady_clock::time_point DispenserOpenTime() const { return dispenser_open_time_; }

    Result<Siligen::Domain::Dispensing::Ports::DispenserValveState> StartDispenser(
        const Siligen::Domain::Dispensing::Ports::DispenserValveParams&) noexcept override {
        return NotImplemented<Siligen::Domain::Dispensing::Ports::DispenserValveState>("StartDispenser");
    }

    Result<Siligen::Domain::Dispensing::Ports::DispenserValveState> OpenDispenser() noexcept override {
        ++open_dispenser_calls_;
        dispenser_open_time_ = std::chrono::steady_clock::now();
        Siligen::Domain::Dispensing::Ports::DispenserValveState state;
        state.status = Siligen::Domain::Dispensing::Ports::DispenserValveStatus::Running;
        return Result<Siligen::Domain::Dispensing::Ports::DispenserValveState>::Success(state);
    }

    Result<void> CloseDispenser() noexcept override {
        ++close_dispenser_calls_;
        return Result<void>::Success();
    }

    Result<Siligen::Domain::Dispensing::Ports::DispenserValveState> StartPositionTriggeredDispenser(
        const Siligen::Domain::Dispensing::Ports::PositionTriggeredDispenserParams&) noexcept override {
        return NotImplemented<Siligen::Domain::Dispensing::Ports::DispenserValveState>("StartPositionTriggeredDispenser");
    }

    Result<void> StopDispenser() noexcept override {
        return NotImplementedVoid("StopDispenser");
    }

    Result<void> PauseDispenser() noexcept override {
        return NotImplementedVoid("PauseDispenser");
    }

    Result<void> ResumeDispenser() noexcept override {
        return NotImplementedVoid("ResumeDispenser");
    }

    Result<Siligen::Domain::Dispensing::Ports::DispenserValveState> GetDispenserStatus() noexcept override {
        if (!status_sequence_.empty()) {
            auto index = status_index_ < status_sequence_.size() ? status_index_ : status_sequence_.size() - 1;
            auto state = status_sequence_[index];
            if (status_index_ + 1 < status_sequence_.size()) {
                ++status_index_;
            }
            return Result<Siligen::Domain::Dispensing::Ports::DispenserValveState>::Success(state);
        }
        Siligen::Domain::Dispensing::Ports::DispenserValveState state;
        state.status = Siligen::Domain::Dispensing::Ports::DispenserValveStatus::Running;
        return Result<Siligen::Domain::Dispensing::Ports::DispenserValveState>::Success(state);
    }

    Result<Siligen::Domain::Dispensing::Ports::SupplyValveState> OpenSupply() noexcept override {
        ++open_supply_calls_;
        supply_open_time_ = std::chrono::steady_clock::now();
        return Result<Siligen::Domain::Dispensing::Ports::SupplyValveState>::Success(
            Siligen::Domain::Dispensing::Ports::SupplyValveState::Open);
    }

    Result<Siligen::Domain::Dispensing::Ports::SupplyValveState> CloseSupply() noexcept override {
        ++close_supply_calls_;
        return Result<Siligen::Domain::Dispensing::Ports::SupplyValveState>::Success(
            Siligen::Domain::Dispensing::Ports::SupplyValveState::Closed);
    }

    Result<Siligen::Domain::Dispensing::Ports::SupplyValveStatusDetail> GetSupplyStatus() noexcept override {
        Siligen::Domain::Dispensing::Ports::SupplyValveStatusDetail detail;
        detail.state = Siligen::Domain::Dispensing::Ports::SupplyValveState::Open;
        return Result<Siligen::Domain::Dispensing::Ports::SupplyValveStatusDetail>::Success(detail);
    }

   private:
    template <typename T>
    Result<T> NotImplemented(const char* method) const {
        return Result<T>::Failure(
            Error(ErrorCode::NOT_IMPLEMENTED, std::string(method) + " not implemented"));
    }

    Result<void> NotImplementedVoid(const char* method) const {
        return Result<void>::Failure(
            Error(ErrorCode::NOT_IMPLEMENTED, std::string(method) + " not implemented"));
    }

    int open_supply_calls_ = 0;
    int close_supply_calls_ = 0;
    int open_dispenser_calls_ = 0;
    int close_dispenser_calls_ = 0;
    size_t status_index_ = 0;
    std::vector<Siligen::Domain::Dispensing::Ports::DispenserValveState> status_sequence_;
    std::chrono::steady_clock::time_point supply_open_time_;
    std::chrono::steady_clock::time_point dispenser_open_time_;
};

}  // namespace

TEST(PurgeDispenserProcessTest, UsesConfigStabilizationDelay) {
    auto valve_port = std::make_shared<TrackingValvePort>();
    auto config_port = std::make_shared<FakeConfigurationPort>(10);

    Siligen::Domain::Dispensing::DomainServices::PurgeDispenserProcess process(valve_port, config_port);
    Siligen::Domain::Dispensing::DomainServices::PurgeDispenserRequest request;
    request.manage_supply = true;
    request.wait_for_completion = false;
    request.supply_stabilization_ms = 0;

    auto result = process.Execute(request);
    ASSERT_TRUE(result.IsSuccess());
    ASSERT_GT(valve_port->OpenSupplyCalls(), 0);
    ASSERT_GT(valve_port->OpenDispenserCalls(), 0);

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        valve_port->DispenserOpenTime() - valve_port->SupplyOpenTime()).count();
    EXPECT_GE(elapsed, 10);
}

TEST(PurgeDispenserProcessTest, ClosesValvesOnTimeout) {
    auto valve_port = std::make_shared<TrackingValvePort>();
    auto config_port = std::make_shared<FakeConfigurationPort>(0);

    Siligen::Domain::Dispensing::Ports::DispenserValveState running_state;
    running_state.status = Siligen::Domain::Dispensing::Ports::DispenserValveStatus::Running;
    valve_port->SetStatusSequence({running_state});

    Siligen::Domain::Dispensing::DomainServices::PurgeDispenserProcess process(valve_port, config_port);
    Siligen::Domain::Dispensing::DomainServices::PurgeDispenserRequest request;
    request.manage_supply = true;
    request.wait_for_completion = true;
    request.supply_stabilization_ms = 1;
    request.poll_interval_ms = 10;
    request.timeout_ms = 100;

    auto result = process.Execute(request);
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::TIMEOUT);
    EXPECT_EQ(valve_port->CloseDispenserCalls(), 1);
    EXPECT_EQ(valve_port->CloseSupplyCalls(), 1);
}
