#include "runtime_execution/application/usecases/dispensing/valve/ValveCommandUseCase.h"
#include "runtime_execution/application/usecases/dispensing/valve/ValveQueryUseCase.h"

#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "siligen/device/contracts/ports/device_ports.h"
#include "shared/types/Error.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace {

using Siligen::Application::UseCases::Dispensing::Valve::PurgeDispenserRequest;
using Siligen::Application::UseCases::Dispensing::Valve::ValveCommandUseCase;
using Siligen::Application::UseCases::Dispensing::Valve::ValveQueryUseCase;
using Siligen::Device::Contracts::Ports::DeviceConnectionPort;
using Siligen::Device::Contracts::State::DeviceConnectionSnapshot;
using Siligen::Device::Contracts::State::DeviceConnectionState;
using Siligen::Domain::Configuration::Ports::IConfigurationPort;
using Siligen::Domain::Dispensing::Ports::DispenserValveParams;
using Siligen::Domain::Dispensing::Ports::DispenserValveState;
using Siligen::Domain::Dispensing::Ports::DispenserValveStatus;
using Siligen::Domain::Dispensing::Ports::IValvePort;
using Siligen::Domain::Dispensing::Ports::PositionTriggeredDispenserParams;
using Siligen::Domain::Dispensing::Ports::SupplyValveState;
using Siligen::Domain::Dispensing::Ports::SupplyValveStatusDetail;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

class FakeConnectionPort final : public DeviceConnectionPort {
   public:
    Result<void> Connect(const Siligen::Device::Contracts::Commands::DeviceConnection&) override {
        return Result<void>::Success();
    }

    Result<void> Disconnect() override {
        return Result<void>::Success();
    }

    Result<DeviceConnectionSnapshot> ReadConnection() const override {
        DeviceConnectionSnapshot snapshot;
        snapshot.state = DeviceConnectionState::Connected;
        return Result<DeviceConnectionSnapshot>::Success(snapshot);
    }

    bool IsConnected() const override {
        return true;
    }

    Result<void> Reconnect() override {
        return Result<void>::Success();
    }

    void SetConnectionStateCallback(
        std::function<void(const DeviceConnectionSnapshot&)>) override {}

    Result<void> StartStatusMonitoring(std::uint32_t) override {
        return Result<void>::Success();
    }

    void StopStatusMonitoring() override {}

    std::string GetLastError() const override {
        return {};
    }

    void ClearError() override {}

    Result<void> StartHeartbeat(const Siligen::Device::Contracts::State::HeartbeatSnapshot&) override {
        return Result<void>::Success();
    }

    void StopHeartbeat() override {}

    Siligen::Device::Contracts::State::HeartbeatSnapshot ReadHeartbeat() const override {
        return {};
    }

    Result<bool> Ping() const override {
        return Result<bool>::Success(true);
    }
};

class FakeConfigurationPort final : public IConfigurationPort {
   public:
    FakeConfigurationPort() {
        dispensing_.supply_stabilization_ms = 10;
    }

    Result<Siligen::Domain::Configuration::Ports::SystemConfig> LoadConfiguration() override {
        return NotImplemented<Siligen::Domain::Configuration::Ports::SystemConfig>("LoadConfiguration");
    }

    Result<void> SaveConfiguration(const Siligen::Domain::Configuration::Ports::SystemConfig&) override {
        return Result<void>::Success();
    }

    Result<void> ReloadConfiguration() override {
        return Result<void>::Success();
    }

    Result<Siligen::Domain::Configuration::Ports::DispensingConfig> GetDispensingConfig() const override {
        return Result<Siligen::Domain::Configuration::Ports::DispensingConfig>::Success(dispensing_);
    }

    Result<void> SetDispensingConfig(const Siligen::Domain::Configuration::Ports::DispensingConfig&) override {
        return Result<void>::Success();
    }

    Result<Siligen::Domain::Configuration::Ports::DxfImportConfig> GetDxfImportConfig() const override {
        return Result<Siligen::Domain::Configuration::Ports::DxfImportConfig>::Success({});
    }

    Result<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig> GetDxfTrajectoryConfig() const override {
        return Result<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig>::Success({});
    }

    Result<Siligen::Shared::Types::DiagnosticsConfig> GetDiagnosticsConfig() const override {
        return NotImplemented<Siligen::Shared::Types::DiagnosticsConfig>("GetDiagnosticsConfig");
    }

    Result<Siligen::Domain::Configuration::Ports::MachineConfig> GetMachineConfig() const override {
        return NotImplemented<Siligen::Domain::Configuration::Ports::MachineConfig>("GetMachineConfig");
    }

    Result<void> SetMachineConfig(const Siligen::Domain::Configuration::Ports::MachineConfig&) override {
        return Result<void>::Success();
    }

    Result<Siligen::Domain::Configuration::Ports::HomingConfig> GetHomingConfig(int) const override {
        return NotImplemented<Siligen::Domain::Configuration::Ports::HomingConfig>("GetHomingConfig");
    }

    Result<void> SetHomingConfig(int, const Siligen::Domain::Configuration::Ports::HomingConfig&) override {
        return Result<void>::Success();
    }

    Result<std::vector<Siligen::Domain::Configuration::Ports::HomingConfig>> GetAllHomingConfigs() const override {
        return Result<std::vector<Siligen::Domain::Configuration::Ports::HomingConfig>>::Success({});
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
        return Result<void>::Success();
    }

    Result<void> RestoreConfiguration(const std::string&) override {
        return Result<void>::Success();
    }

    Result<Siligen::Shared::Types::HardwareMode> GetHardwareMode() const override {
        return Result<Siligen::Shared::Types::HardwareMode>::Success(Siligen::Shared::Types::HardwareMode::Mock);
    }

    Result<Siligen::Shared::Types::HardwareConfiguration> GetHardwareConfiguration() const override {
        return Result<Siligen::Shared::Types::HardwareConfiguration>::Success({});
    }

   private:
    template <typename T>
    Result<T> NotImplemented(const char* method) const {
        return Result<T>::Failure(
            Error(ErrorCode::NOT_IMPLEMENTED, std::string(method) + " not implemented"));
    }

    Siligen::Domain::Configuration::Ports::DispensingConfig dispensing_{};
};

class FakeValvePort final : public IValvePort {
   public:
    Result<DispenserValveState> StartDispenser(const DispenserValveParams&) noexcept override {
        return Result<DispenserValveState>::Success(dispenser_state_);
    }

    Result<DispenserValveState> OpenDispenser() noexcept override {
        return Result<DispenserValveState>::Success(dispenser_state_);
    }

    Result<void> CloseDispenser() noexcept override {
        return Result<void>::Success();
    }

    Result<DispenserValveState> StartPositionTriggeredDispenser(const PositionTriggeredDispenserParams&) noexcept override {
        return Result<DispenserValveState>::Success(dispenser_state_);
    }

    Result<void> StopDispenser() noexcept override {
        return Result<void>::Success();
    }

    Result<void> PauseDispenser() noexcept override {
        return Result<void>::Success();
    }

    Result<void> ResumeDispenser() noexcept override {
        return Result<void>::Success();
    }

    Result<DispenserValveState> GetDispenserStatus() noexcept override {
        if (dispenser_status_should_fail_) {
            return Result<DispenserValveState>::Failure(
                Error(ErrorCode::HARDWARE_ERROR, "dispenser status read failure", "FakeValvePort"));
        }
        return Result<DispenserValveState>::Success(dispenser_state_);
    }

    Result<SupplyValveState> OpenSupply() noexcept override {
        supply_status_detail_.state = SupplyValveState::Open;
        return Result<SupplyValveState>::Success(SupplyValveState::Open);
    }

    Result<SupplyValveState> CloseSupply() noexcept override {
        supply_status_detail_.state = SupplyValveState::Closed;
        return Result<SupplyValveState>::Success(SupplyValveState::Closed);
    }

    Result<SupplyValveStatusDetail> GetSupplyStatus() noexcept override {
        if (supply_status_should_fail_) {
            return Result<SupplyValveStatusDetail>::Failure(
                Error(ErrorCode::HARDWARE_ERROR, "supply status read failure", "FakeValvePort"));
        }
        return Result<SupplyValveStatusDetail>::Success(supply_status_detail_);
    }

    DispenserValveState dispenser_state_{};
    SupplyValveStatusDetail supply_status_detail_{};
    bool dispenser_status_should_fail_{false};
    bool supply_status_should_fail_{false};
};

DispenserValveParams BuildValidDispenserParams() {
    DispenserValveParams params;
    params.count = 1;
    params.intervalMs = 1000;
    params.durationMs = 100;
    return params;
}

}  // namespace

TEST(ValveUseCasesTest, StopDispenserRejectsWhenValvePortUnavailable) {
    auto connection_port = std::make_shared<FakeConnectionPort>();
    ValveCommandUseCase use_case(nullptr, std::make_shared<FakeConfigurationPort>(), connection_port);

    const auto result = use_case.StopDispenser();

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::PORT_NOT_INITIALIZED);
}

TEST(ValveUseCasesTest, PurgeDispenserRejectsWhenValvePortUnavailable) {
    auto connection_port = std::make_shared<FakeConnectionPort>();
    ValveCommandUseCase use_case(nullptr, std::make_shared<FakeConfigurationPort>(), connection_port);
    PurgeDispenserRequest request;
    request.manage_supply = false;
    request.wait_for_completion = false;

    const auto result = use_case.PurgeDispenser(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::PORT_NOT_INITIALIZED);
}

TEST(ValveUseCasesTest, QueryRejectsWhenValvePortUnavailable) {
    ValveQueryUseCase use_case(nullptr);

    const auto dispenser_result = use_case.GetDispenserStatus();
    ASSERT_TRUE(dispenser_result.IsError());
    EXPECT_EQ(dispenser_result.GetError().GetCode(), ErrorCode::PORT_NOT_INITIALIZED);

    const auto supply_result = use_case.GetSupplyStatus();
    ASSERT_TRUE(supply_result.IsError());
    EXPECT_EQ(supply_result.GetError().GetCode(), ErrorCode::PORT_NOT_INITIALIZED);
}

TEST(ValveUseCasesTest, StartDispenserRejectsWhenSupplyValveNotOpen) {
    auto connection_port = std::make_shared<FakeConnectionPort>();
    auto valve_port = std::make_shared<FakeValvePort>();
    valve_port->supply_status_detail_.state = SupplyValveState::Closed;
    valve_port->dispenser_state_.status = DispenserValveStatus::Idle;

    ValveCommandUseCase use_case(valve_port, std::make_shared<FakeConfigurationPort>(), connection_port);
    auto result = use_case.StartDispenser(BuildValidDispenserParams());

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(result.GetError().GetMessage().find("failure_stage=check_valve_safety_supply_open"), std::string::npos);
}

TEST(ValveUseCasesTest, OpenSupplyValveRejectsWhenDispenserStatusHasError) {
    auto connection_port = std::make_shared<FakeConnectionPort>();
    auto valve_port = std::make_shared<FakeValvePort>();
    valve_port->dispenser_state_.status = DispenserValveStatus::Error;
    valve_port->dispenser_state_.errorMessage = "dispenser_error";
    valve_port->supply_status_detail_.state = SupplyValveState::Closed;

    ValveCommandUseCase use_case(valve_port, std::make_shared<FakeConfigurationPort>(), connection_port);
    auto result = use_case.OpenSupplyValve();

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::HARDWARE_ERROR);
    EXPECT_NE(result.GetError().GetMessage().find("failure_stage=check_valve_safety_dispenser_status"), std::string::npos);
}

TEST(ValveUseCasesTest, ResumeDispenserRejectsWhenSupplyValveNotOpen) {
    auto connection_port = std::make_shared<FakeConnectionPort>();
    auto valve_port = std::make_shared<FakeValvePort>();
    valve_port->dispenser_state_.status = DispenserValveStatus::Paused;
    valve_port->supply_status_detail_.state = SupplyValveState::Closed;

    ValveCommandUseCase use_case(valve_port, std::make_shared<FakeConfigurationPort>(), connection_port);
    auto result = use_case.ResumeDispenser();

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(result.GetError().GetMessage().find("failure_stage=check_valve_safety_supply_open"), std::string::npos);
}
