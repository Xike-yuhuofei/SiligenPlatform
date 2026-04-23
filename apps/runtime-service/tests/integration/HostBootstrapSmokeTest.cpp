#include "runtime_process_bootstrap/ContainerBootstrap.h"
#include "container/ApplicationContainer.h"
#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "runtime_execution/contracts/safety/IInterlockSignalPort.h"
#include "siligen/device/adapters/drivers/multicard/MockMultiCardWrapper.h"
#include "dispense_packaging/contracts/ITaskSchedulerPort.h"
#include "runtime_execution/contracts/motion/IMotionRuntimePort.h"
#include "dispense_packaging/contracts/IValvePort.h"
#include "siligen/device/contracts/commands/device_commands.h"
#include "siligen/device/contracts/ports/device_ports.h"
#include "runtime_process_bootstrap/WorkspaceAssetPaths.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>

namespace {

using Siligen::Apps::Runtime::BuildContainer;
using Siligen::Application::Container::LogMode;
using Siligen::Infrastructure::Configuration::CanonicalizeOrKeep;
using Siligen::Infrastructure::Configuration::kCanonicalMachineConfigRelativePath;

class ScopedCurrentPath {
public:
    explicit ScopedCurrentPath(const std::filesystem::path& target)
        : previous_(std::filesystem::current_path()) {
        std::filesystem::current_path(target);
    }

    ~ScopedCurrentPath() {
        std::error_code ec;
        std::filesystem::current_path(previous_, ec);
    }

private:
    std::filesystem::path previous_;
};

class ScopedTempWorkspace {
public:
    explicit ScopedTempWorkspace(const std::string& suffix) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = std::filesystem::temp_directory_path() /
                ("siligen_runtime_host_smoke_" + suffix + "_" + std::to_string(stamp));

        std::error_code ec;
        std::filesystem::create_directories(root_ / "cmake", ec);
        std::filesystem::create_directories(root_ / "apps/runtime-service", ec);

        WriteFile("WORKSPACE.md", "# temp workspace\n");
        WriteFile("AGENTS.md", "# temp agents\n");
        WriteFile("cmake/workspace-layout.env", "SILIGEN_APPS_ROOT=apps\n");
    }

    ~ScopedTempWorkspace() {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    [[nodiscard]] const std::filesystem::path& root() const noexcept {
        return root_;
    }

    std::filesystem::path WriteFile(
        const std::filesystem::path& relative_path,
        const std::string& content) const {
        const auto absolute_path = root_ / relative_path;
        std::error_code ec;
        std::filesystem::create_directories(absolute_path.parent_path(), ec);
        std::ofstream out(absolute_path, std::ios::binary);
        out << content;
        out.close();
        return absolute_path;
    }

private:
    std::filesystem::path root_;
};

std::string BuildMockMachineIni() {
    return std::string(
        "[Dispensing]\n"
        "pressure=50.0\n"
        "flow_rate=1.0\n"
        "dispensing_time=0.015\n"
        "wait_time=0.05\n"
        "supply_stabilization_ms=500\n"
        "retract_height=1.0\n"
        "approach_height=2.0\n"
        "mode=2\n"
        "strategy=baseline\n"
        "subsegment_count=8\n"
        "dispense_only_cruise=false\n"
        "open_comp_ms=0\n"
        "close_comp_ms=0\n"
        "retract_enabled=false\n"
        "corner_pulse_scale=1.0\n"
        "curvature_speed_factor=0.8\n"
        "start_speed_factor=0.5\n"
        "end_speed_factor=0.5\n"
        "corner_speed_factor=0.6\n"
        "rapid_speed_factor=1.0\n"
        "trajectory_sample_dt=0.01\n"
        "trajectory_sample_ds=0.0\n"
        "temperature_target_c=0.0\n"
        "temperature_tolerance_c=0.0\n"
        "viscosity_target_cps=0.0\n"
        "viscosity_tolerance_pct=0.0\n"
        "air_dry_required=true\n"
        "air_oil_free_required=true\n"
        "suck_back_enabled=false\n"
        "suck_back_ms=0.0\n"
        "vision_feedback_enabled=false\n"
        "dot_diameter_target_mm=1.0\n"
        "dot_edge_gap_mm=2.0\n"
        "dot_diameter_tolerance_mm=0.2\n"
        "syringe_level_min_pct=20.0\n"
        "\n"
        "[Machine]\n"
        "work_area_width=480.0\n"
        "work_area_height=480.0\n"
        "z_axis_range=100.0\n"
        "max_speed=300.0\n"
        "max_acceleration=1000.0\n"
        "positioning_tolerance=0.1\n"
        "pulse_per_mm=200\n"
        "x_min=0.0\n"
        "x_max=480.0\n"
        "y_min=0.0\n"
        "y_max=480.0\n"
        "z_min=-50.0\n"
        "z_max=50.0\n"
        "\n"
        "[DXF]\n"
        "offset_x=0.0\n"
        "offset_y=0.0\n"
        "\n"
        "[Hardware]\n"
        "mode=Mock\n"
        "pulse_per_mm=200\n"
        "max_velocity_mm_s=500.0\n"
        "max_acceleration_mm_s2=2000.0\n"
        "max_deceleration_mm_s2=2000.0\n"
        "position_tolerance_mm=0.1\n"
        "soft_limit_positive_mm=480.0\n"
        "soft_limit_negative_mm=0.0\n"
        "response_timeout_ms=5000\n"
        "motion_timeout_ms=30000\n"
        "num_axes=2\n"
        "axis1_encoder_enabled=false\n"
        "axis2_encoder_enabled=false\n"
        "\n"
        "[Homing_Axis1]\n"
        "mode=0\n"
        "direction=-1\n"
        "home_input_source=3\n"
        "home_input_bit=0\n"
        "home_active_low=false\n"
        "home_debounce_ms=0\n"
        "ready_zero_speed_mm_s=8.0\n"
        "rapid_velocity=50.0\n"
        "locate_velocity=10.0\n"
        "index_velocity=5.0\n"
        "acceleration=100.0\n"
        "deceleration=100.0\n"
        "jerk=0.0\n"
        "offset=0.0\n"
        "search_distance=200.0\n"
        "escape_distance=5.0\n"
        "escape_velocity=10.0\n"
        "home_backoff_enabled=true\n"
        "timeout_ms=80000\n"
        "settle_time_ms=50\n"
        "escape_timeout_ms=5000\n"
        "retry_count=1\n"
        "enable_escape=true\n"
        "enable_limit_switch=false\n"
        "enable_index=false\n"
        "escape_max_attempts=1\n"
        "\n"
        "[Homing_Axis2]\n"
        "mode=0\n"
        "direction=-1\n"
        "home_input_source=3\n"
        "home_input_bit=1\n"
        "home_active_low=false\n"
        "home_debounce_ms=0\n"
        "ready_zero_speed_mm_s=8.0\n"
        "rapid_velocity=50.0\n"
        "locate_velocity=10.0\n"
        "index_velocity=5.0\n"
        "acceleration=100.0\n"
        "deceleration=100.0\n"
        "jerk=0.0\n"
        "offset=0.0\n"
        "search_distance=200.0\n"
        "escape_distance=5.0\n"
        "escape_velocity=10.0\n"
        "home_backoff_enabled=true\n"
        "timeout_ms=80000\n"
        "settle_time_ms=50\n"
        "escape_timeout_ms=5000\n"
        "retry_count=1\n"
        "enable_escape=true\n"
        "enable_limit_switch=false\n"
        "enable_index=false\n"
        "escape_max_attempts=1\n"
        "\n"
        "[ValveSupply]\n"
        "do_bit_index=0\n"
        "do_card_index=0\n"
        "timeout_ms=100\n"
        "fail_closed=true\n"
        "\n"
        "[ValveDispenser]\n"
        "cmp_channel=2\n"
        "pulse_type=1\n"
        "cmp_axis_mask=3\n"
        "min_count=1\n"
        "max_count=100\n"
        "min_interval_ms=10\n"
        "max_interval_ms=1000\n"
        "min_duration_ms=5\n"
        "max_duration_ms=500\n"
        "abs_position_flag=1\n"
        "\n"
        "[Safety]\n"
        "emergency_stop_enabled=true\n"
        "\n"
        "[Interlock]\n"
        "enabled=true\n"
        "emergency_stop_input=7\n"
        "emergency_stop_active_low=false\n"
        "safety_door_input=-1\n"
        "poll_interval_ms=10\n"
    );
}

}  // namespace

TEST(RuntimeExecutionIntegrationHostBootstrapSmokeTest, BuildsContainerFromCanonicalMockConfigWithoutHardware) {
    using DeviceConnection = Siligen::Device::Contracts::Commands::DeviceConnection;
    using DeviceConnectionPort = Siligen::Device::Contracts::Ports::DeviceConnectionPort;
    using DispenserDevicePort = Siligen::Device::Contracts::Ports::DispenserDevicePort;
    using MotionDevicePort = Siligen::Device::Contracts::Ports::MotionDevicePort;

    ScopedTempWorkspace workspace("canonical_mock_config");
    const auto expected_config_path =
        workspace.WriteFile(kCanonicalMachineConfigRelativePath, BuildMockMachineIni());
    const auto runtime_service_dir = workspace.root() / "apps" / "runtime-service";

    ScopedCurrentPath scoped_cwd(runtime_service_dir);

    std::string resolved_config_path;
    auto container = BuildContainer(
        kCanonicalMachineConfigRelativePath,
        LogMode::Silent,
        "runtime_host_smoke.log",
        1,
        &resolved_config_path);

    ASSERT_NE(container, nullptr);
    EXPECT_EQ(
        CanonicalizeOrKeep(std::filesystem::path(resolved_config_path)),
        CanonicalizeOrKeep(expected_config_path));

    EXPECT_NE(
        container->ResolvePort<Siligen::Domain::Configuration::Ports::IConfigurationPort>(),
        nullptr);
    EXPECT_NE(
        container->ResolvePort<Siligen::Device::Contracts::Ports::DeviceConnectionPort>(),
        nullptr);
    EXPECT_NE(container->ResolvePort<MotionDevicePort>(), nullptr);
    EXPECT_NE(
        container->ResolvePort<Siligen::RuntimeExecution::Contracts::Motion::IMotionRuntimePort>(),
        nullptr);
    EXPECT_NE(
        container->ResolvePort<Siligen::Domain::Dispensing::Ports::IValvePort>(),
        nullptr);
    EXPECT_NE(container->ResolvePort<DispenserDevicePort>(), nullptr);
    EXPECT_NE(
        container->ResolvePort<Siligen::Domain::Dispensing::Ports::ITaskSchedulerPort>(),
        nullptr);
    EXPECT_NE(container->GetMultiCardInstance(), nullptr);

    auto connection_port = container->ResolvePort<DeviceConnectionPort>();
    auto motion_device_port = container->ResolvePort<MotionDevicePort>();
    auto dispenser_device_port = container->ResolvePort<DispenserDevicePort>();
    ASSERT_NE(connection_port, nullptr);
    ASSERT_NE(motion_device_port, nullptr);
    ASSERT_NE(dispenser_device_port, nullptr);

    const auto motion_capabilities_result = motion_device_port->DescribeCapabilities();
    ASSERT_TRUE(motion_capabilities_result.IsSuccess()) << motion_capabilities_result.GetError().GetMessage();
    EXPECT_TRUE(motion_capabilities_result.Value().mock_backend);
    EXPECT_TRUE(motion_capabilities_result.Value().trigger.supports_in_motion_position_trigger);
    EXPECT_TRUE(motion_capabilities_result.Value().trigger.supports_in_motion_time_trigger);

    const auto dispenser_capability_result = dispenser_device_port->DescribeCapability();
    ASSERT_TRUE(dispenser_capability_result.IsSuccess()) << dispenser_capability_result.GetError().GetMessage();
    EXPECT_TRUE(dispenser_capability_result.Value().supports_pause);
    EXPECT_TRUE(dispenser_capability_result.Value().supports_resume);
    EXPECT_TRUE(dispenser_capability_result.Value().supports_continuous_mode);
    EXPECT_TRUE(dispenser_capability_result.Value().supports_in_motion_pulse_shot);
    EXPECT_FALSE(dispenser_capability_result.Value().supports_prime);

    DeviceConnection connection;
    connection.local_ip = "192.168.10.10";
    connection.card_ip = "192.168.10.20";
    connection.local_port = 5000;
    connection.card_port = 5000;
    connection.timeout_ms = 3000;

    const auto connect_result = connection_port->Connect(connection);
    ASSERT_TRUE(connect_result.IsSuccess()) << connect_result.GetError().GetMessage();

    const auto motion_state_result = motion_device_port->ReadState();
    ASSERT_TRUE(motion_state_result.IsSuccess()) << motion_state_result.GetError().GetMessage();
    EXPECT_TRUE(motion_state_result.Value().connected);
    EXPECT_EQ(motion_state_result.Value().axes.size(), 2U);

    const auto disconnect_result = connection_port->Disconnect();
    ASSERT_TRUE(disconnect_result.IsSuccess()) << disconnect_result.GetError().GetMessage();
}

TEST(RuntimeExecutionIntegrationHostBootstrapSmokeTest, InterlockSignalsFollowConnectionLifecycle) {
    using DeviceConnection = Siligen::Device::Contracts::Commands::DeviceConnection;
    using DeviceConnectionPort = Siligen::Device::Contracts::Ports::DeviceConnectionPort;
    using IInterlockSignalPort = Siligen::Domain::Safety::Ports::IInterlockSignalPort;

    ScopedTempWorkspace workspace("interlock_lifecycle");
    workspace.WriteFile(kCanonicalMachineConfigRelativePath, BuildMockMachineIni());
    const auto runtime_service_dir = workspace.root() / "apps" / "runtime-service";

    ScopedCurrentPath scoped_cwd(runtime_service_dir);

    auto container = BuildContainer(
        kCanonicalMachineConfigRelativePath,
        LogMode::Silent,
        "runtime_host_smoke.log",
        1);

    ASSERT_NE(container, nullptr);

    auto connection_port = container->ResolvePort<DeviceConnectionPort>();
    auto interlock_port = container->ResolvePort<IInterlockSignalPort>();
    ASSERT_NE(connection_port, nullptr);
    ASSERT_NE(interlock_port, nullptr);

    const auto initial_signals = interlock_port->ReadSignals();
    EXPECT_TRUE(initial_signals.IsError());

    DeviceConnection connection;
    connection.local_ip = "192.168.10.10";
    connection.card_ip = "192.168.10.20";
    connection.local_port = 5000;
    connection.card_port = 5000;
    connection.timeout_ms = 3000;

    const auto connect_result = connection_port->Connect(connection);
    ASSERT_TRUE(connect_result.IsSuccess()) << connect_result.GetError().GetMessage();

    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    const auto connected_signals = interlock_port->ReadSignals();
    ASSERT_TRUE(connected_signals.IsSuccess()) << connected_signals.GetError().GetMessage();
    EXPECT_FALSE(connected_signals.Value().emergency_stop_triggered);
    EXPECT_FALSE(connected_signals.Value().safety_door_open);

    const auto disconnect_result = connection_port->Disconnect();
    ASSERT_TRUE(disconnect_result.IsSuccess()) << disconnect_result.GetError().GetMessage();

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    const auto disconnected_signals = interlock_port->ReadSignals();
    EXPECT_TRUE(disconnected_signals.IsError());
}

TEST(RuntimeExecutionIntegrationHostBootstrapSmokeTest, InterlockMonitorStopsAfterPassiveDisconnect) {
    using DeviceConnection = Siligen::Device::Contracts::Commands::DeviceConnection;
    using DeviceConnectionPort = Siligen::Device::Contracts::Ports::DeviceConnectionPort;
    using DeviceConnectionState = Siligen::Device::Contracts::State::DeviceConnectionState;
    using IInterlockSignalPort = Siligen::Domain::Safety::Ports::IInterlockSignalPort;
    using MockMultiCardWrapper = Siligen::Infrastructure::Hardware::MockMultiCardWrapper;

    ScopedTempWorkspace workspace("interlock_passive_disconnect");
    workspace.WriteFile(kCanonicalMachineConfigRelativePath, BuildMockMachineIni());
    const auto runtime_service_dir = workspace.root() / "apps" / "runtime-service";

    ScopedCurrentPath scoped_cwd(runtime_service_dir);

    auto container = BuildContainer(
        kCanonicalMachineConfigRelativePath,
        LogMode::Silent,
        "runtime_host_smoke.log",
        1);

    ASSERT_NE(container, nullptr);

    auto connection_port = container->ResolvePort<DeviceConnectionPort>();
    auto interlock_port = container->ResolvePort<IInterlockSignalPort>();
    ASSERT_NE(connection_port, nullptr);
    ASSERT_NE(interlock_port, nullptr);

    auto* mock_wrapper = static_cast<MockMultiCardWrapper*>(container->GetMultiCardInstance());
    ASSERT_NE(mock_wrapper, nullptr);

    DeviceConnection connection;
    connection.local_ip = "192.168.10.10";
    connection.card_ip = "192.168.10.20";
    connection.local_port = 5000;
    connection.card_port = 5000;
    connection.timeout_ms = 3000;

    const auto connect_result = connection_port->Connect(connection);
    ASSERT_TRUE(connect_result.IsSuccess()) << connect_result.GetError().GetMessage();

    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    ASSERT_TRUE(interlock_port->ReadSignals().IsSuccess());

    mock_wrapper->ResetConnectionSimulation();
    mock_wrapper->SimulateDisconnect();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1800);
    bool interlock_stopped = false;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto signals_result = interlock_port->ReadSignals();
        if (signals_result.IsError()) {
            interlock_stopped = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    ASSERT_TRUE(interlock_stopped);

    const auto connection_snapshot = connection_port->ReadConnection();
    ASSERT_TRUE(connection_snapshot.IsSuccess()) << connection_snapshot.GetError().GetMessage();
    EXPECT_EQ(connection_snapshot.Value().state, DeviceConnectionState::Disconnected);
}

TEST(RuntimeExecutionIntegrationHostBootstrapSmokeTest, InterlockMonitorStopsAfterPassiveDisconnectWhenHeartbeatDegradesFirst) {
    using DeviceConnection = Siligen::Device::Contracts::Commands::DeviceConnection;
    using DeviceConnectionPort = Siligen::Device::Contracts::Ports::DeviceConnectionPort;
    using DeviceConnectionState = Siligen::Device::Contracts::State::DeviceConnectionState;
    using HeartbeatSnapshot = Siligen::Device::Contracts::State::HeartbeatSnapshot;
    using IInterlockSignalPort = Siligen::Domain::Safety::Ports::IInterlockSignalPort;
    using MockMultiCardWrapper = Siligen::Infrastructure::Hardware::MockMultiCardWrapper;

    ScopedTempWorkspace workspace("interlock_passive_disconnect_heartbeat");
    workspace.WriteFile(kCanonicalMachineConfigRelativePath, BuildMockMachineIni());
    const auto runtime_service_dir = workspace.root() / "apps" / "runtime-service";

    ScopedCurrentPath scoped_cwd(runtime_service_dir);

    auto container = BuildContainer(
        kCanonicalMachineConfigRelativePath,
        LogMode::Silent,
        "runtime_host_smoke.log",
        1);

    ASSERT_NE(container, nullptr);

    auto connection_port = container->ResolvePort<DeviceConnectionPort>();
    auto interlock_port = container->ResolvePort<IInterlockSignalPort>();
    ASSERT_NE(connection_port, nullptr);
    ASSERT_NE(interlock_port, nullptr);

    auto* mock_wrapper = static_cast<MockMultiCardWrapper*>(container->GetMultiCardInstance());
    ASSERT_NE(mock_wrapper, nullptr);

    DeviceConnection connection;
    connection.local_ip = "192.168.10.10";
    connection.card_ip = "192.168.10.20";
    connection.local_port = 5000;
    connection.card_port = 5000;
    connection.timeout_ms = 3000;

    const auto connect_result = connection_port->Connect(connection);
    ASSERT_TRUE(connect_result.IsSuccess()) << connect_result.GetError().GetMessage();

    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    ASSERT_TRUE(interlock_port->ReadSignals().IsSuccess());

    HeartbeatSnapshot heartbeat_config;
    heartbeat_config.interval_ms = 100;
    heartbeat_config.timeout_ms = 100;
    heartbeat_config.failure_threshold = 1;
    const auto heartbeat_result = connection_port->StartHeartbeat(heartbeat_config);
    ASSERT_TRUE(heartbeat_result.IsSuccess()) << heartbeat_result.GetError().GetMessage();

    const auto monitoring_result = connection_port->StartStatusMonitoring(250);
    ASSERT_TRUE(monitoring_result.IsSuccess()) << monitoring_result.GetError().GetMessage();

    mock_wrapper->ResetConnectionSimulation();
    mock_wrapper->SimulateDisconnect();

    const auto degraded_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1200);
    bool degraded_seen = false;
    while (std::chrono::steady_clock::now() < degraded_deadline) {
        if (connection_port->ReadHeartbeat().is_degraded) {
            degraded_seen = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ASSERT_TRUE(degraded_seen);

    const auto disconnect_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1800);
    bool interlock_stopped = false;
    bool connection_disconnected = false;
    while (std::chrono::steady_clock::now() < disconnect_deadline) {
        if (!interlock_stopped && interlock_port->ReadSignals().IsError()) {
            interlock_stopped = true;
        }

        const auto connection_snapshot = connection_port->ReadConnection();
        ASSERT_TRUE(connection_snapshot.IsSuccess()) << connection_snapshot.GetError().GetMessage();
        if (connection_snapshot.Value().state == DeviceConnectionState::Disconnected) {
            connection_disconnected = true;
        }

        if (interlock_stopped && connection_disconnected) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    connection_port->StopHeartbeat();
    connection_port->StopStatusMonitoring();

    ASSERT_TRUE(interlock_stopped);
    ASSERT_TRUE(connection_disconnected);
}
