#include "runtime/configuration/ConfigFileAdapter.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

using Siligen::Infrastructure::Adapters::ConfigFileAdapter;

std::filesystem::path WriteTempIni(const std::string& suffix, const std::string& content) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() /
                ("siligen_hw_config_" + suffix + "_" + std::to_string(now) + ".ini");
    std::ofstream out(path, std::ios::binary);
    out << content;
    out.close();
    return path;
}

std::string ReplaceAll(std::string text, const std::string& from, const std::string& to) {
    std::size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

std::string BuildBaseIni(const std::string& hardware_suffix = "") {
    return std::string(
        "[Dispensing]\n"
        "pressure=50.0\n"
        "flow_rate=1.0\n"
        "dispensing_time=0.015\n"
        "wait_time=0.05\n"
        "retract_height=1.0\n"
        "approach_height=2.0\n"
        "mode=2\n"
        "open_comp_ms=0\n"
        "close_comp_ms=0\n"
        "retract_enabled=false\n"
        "corner_pulse_scale=1.0\n"
        "curvature_speed_factor=0.8\n"
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
        "mode=Real\n"
        "pulse_per_mm=200\n"
        "max_velocity_mm_s=500.0\n"
        "max_acceleration_mm_s2=2000.0\n"
        "max_deceleration_mm_s2=2000.0\n"
        "position_tolerance_mm=0.1\n"
        "soft_limit_positive_mm=480.0\n"
        "soft_limit_negative_mm=0.0\n"
        "response_timeout_ms=5000\n"
        "motion_timeout_ms=30000\n"
        "num_axes=2\n")
        + hardware_suffix +
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
        "timeout_ms=30000\n"
        "settle_time_ms=50\n"
        "escape_timeout_ms=5000\n"
        "retry_count=1\n"
        "enable_escape=true\n"
        "enable_limit_switch=false\n"
        "enable_index=false\n"
        "escape_max_attempts=1\n";
}

}  // namespace

TEST(ConfigFileAdapterHardwareConfigurationTest, LoadsExplicitAxisEncoderFlags) {
    const auto ini_path = WriteTempIni(
        "explicit_encoder_flags",
        BuildBaseIni("axis1_encoder_enabled=false\naxis2_encoder_enabled=true\n"));

    ConfigFileAdapter adapter(ini_path.string());
    auto result = adapter.GetHardwareConfiguration();

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_FALSE(result.Value().encoder_enabled[0]);
    EXPECT_TRUE(result.Value().encoder_enabled[1]);

    std::error_code ec;
    std::filesystem::remove(ini_path, ec);
}

TEST(ConfigFileAdapterHardwareConfigurationTest, DefaultsMissingAxisEncoderFlagsToTrue) {
    const auto ini_path = WriteTempIni("default_encoder_flags", BuildBaseIni());

    ConfigFileAdapter adapter(ini_path.string());
    auto result = adapter.GetHardwareConfiguration();

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_TRUE(result.Value().encoder_enabled[0]);
    EXPECT_TRUE(result.Value().encoder_enabled[1]);

    std::error_code ec;
    std::filesystem::remove(ini_path, ec);
}

TEST(ConfigFileAdapterHardwareConfigurationTest, LoadsHomeBackoffFlagFromHomingSections) {
    auto ini = BuildBaseIni();
    ini = ReplaceAll(ini,
                     "escape_velocity=10.0\n"
                     "timeout_ms=30000\n",
                     "escape_velocity=10.0\n"
                     "home_backoff_enabled=false\n"
                     "timeout_ms=30000\n");

    const auto ini_path = WriteTempIni("home_backoff_flag", ini);

    ConfigFileAdapter adapter(ini_path.string());
    auto result = adapter.LoadConfiguration();

    ASSERT_TRUE(result.IsSuccess());
    ASSERT_GE(result.Value().homing_configs.size(), 2u);
    EXPECT_FALSE(result.Value().homing_configs[0].home_backoff_enabled);
    EXPECT_FALSE(result.Value().homing_configs[1].home_backoff_enabled);

    std::error_code ec;
    std::filesystem::remove(ini_path, ec);
}

TEST(ConfigFileAdapterHardwareConfigurationTest, DefaultsMissingHomeBackoffFlagToTrue) {
    const auto ini_path = WriteTempIni("default_home_backoff_flag", BuildBaseIni());

    ConfigFileAdapter adapter(ini_path.string());
    auto result = adapter.LoadConfiguration();

    ASSERT_TRUE(result.IsSuccess());
    ASSERT_GE(result.Value().homing_configs.size(), 2u);
    EXPECT_TRUE(result.Value().homing_configs[0].home_backoff_enabled);
    EXPECT_TRUE(result.Value().homing_configs[1].home_backoff_enabled);

    std::error_code ec;
    std::filesystem::remove(ini_path, ec);
}
