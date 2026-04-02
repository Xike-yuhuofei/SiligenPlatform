#include "runtime_process_bootstrap/ContainerBootstrap.h"
#include "container/ApplicationContainer.h"
#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "runtime_execution/contracts/dispensing/ITaskSchedulerPort.h"
#include "runtime_execution/contracts/motion/IMotionRuntimePort.h"
#include "domain/dispensing/ports/IValvePort.h"
#include "siligen/device/contracts/ports/device_ports.h"
#include "runtime_process_bootstrap/WorkspaceAssetPaths.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

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
        "timeout_ms=30000\n"
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
        "timeout_ms=30000\n"
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
    );
}

}  // namespace

TEST(RuntimeExecutionIntegrationHostBootstrapSmokeTest, BuildsContainerFromCanonicalMockConfigWithoutHardware) {
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
    EXPECT_NE(
        container->ResolvePort<Siligen::RuntimeExecution::Contracts::Motion::IMotionRuntimePort>(),
        nullptr);
    EXPECT_NE(
        container->ResolvePort<Siligen::Domain::Dispensing::Ports::IValvePort>(),
        nullptr);
    EXPECT_NE(
        container->ResolvePort<Siligen::RuntimeExecution::Contracts::Dispensing::ITaskSchedulerPort>(),
        nullptr);
    EXPECT_NE(container->GetMultiCardInstance(), nullptr);

    EXPECT_TRUE(std::filesystem::exists(workspace.root() / "data" / "schemas" / "recipes"));
}
