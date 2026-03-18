#include "application/usecases/motion/homing/HomeAxesUseCase.h"
#include "domain/configuration/ports/IConfigurationPort.h"
#include "domain/motion/ports/IHomingPort.h"
#include "domain/system/ports/IEventPublisherPort.h"
#include "shared/types/Error.h"
#include "shared/types/HardwareConfiguration.h"
#include "shared/types/Types.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
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
